#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Renderer2D.h"
#include "Headers/PhysicsSystem.h"

#include <imgui.h>
#include <imgui-SFML.h>
#include <optional>
#include <cmath>
#include <algorithm> // std::clamp

static inline void ClampDockedMinWidth(float minW) {
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

ViewportPanel::ViewportPanel() {}

void ViewportPanel::EnsureRT() {
    const bool needRecreate =
        !m_RT ||
        m_RT->getSize().x != m_VirtW ||
        m_RT->getSize().y != m_VirtH;

    if (needRecreate) {
        // RT principal
        m_RT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ m_VirtW, m_VirtH });
        m_RT->setSmooth(true);
        sf::View v;
        v.setSize(sf::Vector2f(static_cast<float>(m_VirtW), static_cast<float>(m_VirtH)));
        v.setCenter(sf::Vector2f(static_cast<float>(m_VirtW) * 0.5f,
            static_cast<float>(m_VirtH) * 0.5f));
        m_RT->setView(v);

        // RT de presentación (para flip vertical)
        m_PresentRT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ m_VirtW, m_VirtH });
        m_PresentRT->setSmooth(true);
        sf::View pv;
        pv.setSize(sf::Vector2f(static_cast<float>(m_VirtW), static_cast<float>(m_VirtH)));
        pv.setCenter(sf::Vector2f(static_cast<float>(m_VirtW) * 0.5f,
            static_cast<float>(m_VirtH) * 0.5f));
        m_PresentRT->setView(pv);
    }
}

void ViewportPanel::OnUpdate(const gp::Timestep& dt) {
    auto& ctx = SceneContext::Get();
    if (!ctx.scene) return;

    // 1) Input jugador
    // Pausar el control del jugador mientras arrastrás (evita que “pelee” con el drag)
    if (!m_Dragging) {
        Systems::PlayerControllerSystem::Update(*ctx.scene, dt.dt);
    }

    // 2) Física
    Systems::PhysicsSystem::Update(*ctx.scene, dt.dt);

    // 3) Colisiones (suelo plano + AABB con estáticos)
    Systems::CollisionSystem::SolveGround(*ctx.scene, /*groundY*/ m_VirtH); // borde inferior del “mundo” 16:9
    Systems::CollisionSystem::SolveAABB(*ctx.scene);

    // (Si querés, podés hacer post-procesos aquí)
}

void ViewportPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ClampDockedMinWidth(480.0f);

    ImVec2 avail = ImGui::GetContentRegionAvail();

    float targetW = std::floor(avail.x > 1 ? avail.x : 1.0f);
    float targetH = std::floor(targetW * 9.0f / 16.0f);
    if (targetH > avail.y) {
        targetH = std::floor(avail.y > 1 ? avail.y : 1.0f);
        targetW = std::floor(targetH * 16.0f / 9.0f);
    }

    EnsureRT();

    if (m_RT && m_PresentRT) {
        // 1) Dibujar escena en RT principal
        m_RT->clear(sf::Color(30, 30, 35));
        if (ctx.scene) {
            // Cámara
            EntityID playerId = 0;
            if (ctx.selected && ctx.scene->playerControllers.contains(ctx.selected.id)) {
                playerId = ctx.selected.id;
            }
            else if (!ctx.scene->playerControllers.empty()) {
                playerId = ctx.scene->playerControllers.begin()->first;
            }

            sf::Vector2f camCenter{ static_cast<float>(m_VirtW) * 0.5f,
                                    static_cast<float>(m_VirtH) * 0.5f };

            if (playerId && ctx.scene->transforms.contains(playerId)) {
                camCenter = ctx.scene->transforms[playerId].position;
                //const float worldW = static_cast<float>(m_VirtW);
                //const float worldH = static_cast<float>(m_VirtH);
                //camCenter.x = std::clamp(camCenter.x, worldW * 0.5f, worldW - worldW * 0.5f);
                //camCenter.y = std::clamp(camCenter.y, worldH * 0.5f, worldH - worldH * 0.5f);
            }

            sf::View v = m_RT->getView();
            v.setCenter(camCenter);
            m_RT->setView(v);

            Renderer2D::Draw(*ctx.scene, *m_RT);
            DrawSelectionGizmo(*m_RT);
        }
        m_RT->display();

        // 2) Espejar verticalmente en el RT de presentación
        m_PresentRT->clear(sf::Color::Black);
        sf::Sprite spr(m_RT->getTexture());
        spr.setScale(sf::Vector2f{ 1.f, -1.f });                  
        spr.setPosition(sf::Vector2f{ 0.f, static_cast<float>(m_VirtH) });          
        m_PresentRT->draw(spr);
        m_PresentRT->display();

        // 3) Letterboxing y muestra
        ImVec2 imgSize{ targetW, targetH };
        ImVec2 cur = ImGui::GetCursorPos();
        ImVec2 offset{ (avail.x - imgSize.x) * 0.5f, (avail.y - imgSize.y) * 0.5f };
        if (offset.x < 0) offset.x = 0;
        if (offset.y < 0) offset.y = 0;
        ImGui::SetCursorPos(ImVec2(cur.x + offset.x, cur.y + offset.y));

        ImGui::Image(m_PresentRT->getTexture(), imgSize);

        // --- Picking & Drag ---
        ImVec2 imgMin = ImGui::GetItemRectMin();
        ImVec2 imgMax = ImGui::GetItemRectMax();
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsItemHovered()) {
            if (auto worldOpt = ScreenToWorld(io.MousePos, imgMin, imgMax)) {
                sf::Vector2f world = *worldOpt;

                // Click para seleccionar
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    EntityID id = PickEntityAt(world);
                    auto& c = SceneContext::Get();
                    if (c.scene && id) {
                        c.selected = Entity{ id };
                        m_Dragging = true;
                        m_DragEntity = id;
                        // offset desde el centro para arrastre “cómodo”
                        m_DragOffset = c.scene->transforms[id].position - world;
                    }
                    else {
                        c.selected = Entity{}; // nada seleccionado
                    }
                }

                // Arrastre
                if (m_Dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_DragEntity) {
                    auto& c2 = SceneContext::Get();
                    if (c2.scene && c2.scene->transforms.contains(m_DragEntity)) {
                        sf::Vector2f pos = world + m_DragOffset;
                        if (m_EnableSnap && m_Grid > 0.0f) {
                            pos.x = std::round(pos.x / m_Grid) * m_Grid;
                            pos.y = std::round(pos.y / m_Grid) * m_Grid;
                        }
                        c2.scene->transforms[m_DragEntity].position = pos;

                        if (c2.scene->physics.contains(m_DragEntity)) {
                            auto& ph = c2.scene->physics[m_DragEntity];
                            ph.velocity = { 0.f, 0.f };
                            ph.onGround = false;
                        }
                    }
                }
            }
        }

        // Soltar
        if (m_Dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_Dragging = false;
            m_DragEntity = 0;
        }

        // --- UI mini-overlay: Snap ---
        ImGui::SetCursorPos(ImVec2(cur.x + 8.f, cur.y + 8.f)); // esquina sup-izq del área de contenido
        ImGui::BeginChild("##vp_overlay", ImVec2(180, 70), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::Checkbox("Snap", &m_EnableSnap);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f);
        ImGui::DragFloat("Grid", &m_Grid, 1.f, 1.f, 512.f, "%.0f");
        ImGui::EndChild();
    }
    else {
        ImGui::TextUnformatted("Creando RenderTextures...");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

std::optional<sf::Vector2f> ViewportPanel::ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const {
    if (!m_RT) return std::nullopt;

    // ¿está el mouse dentro del rectángulo de la imagen?
    if (mouse.x < imgMin.x || mouse.y < imgMin.y || mouse.x > imgMax.x || mouse.y > imgMax.y)
        return std::nullopt;

    // uv en [0..1] dentro de la imagen mostrada por ImGui
    ImVec2 imgSize{ imgMax.x - imgMin.x, imgMax.y - imgMin.y };
    float u = (mouse.x - imgMin.x) / imgSize.x;
    float v = (mouse.y - imgMin.y) / imgSize.y;

    // píxel en la textura de presentación
    int pxPresent = static_cast<int>(u * static_cast<float>(m_VirtW));
    int pyPresent = static_cast<int>(v * static_cast<float>(m_VirtH));

    // convertir a píxel del RT original (flipeado verticalmente)
    int pxRT = pxPresent;
    int pyRT = static_cast<int>(m_VirtH) - 1 - pyPresent;
    if (pxRT < 0) pxRT = 0; if (pxRT >= static_cast<int>(m_VirtW)) pxRT = static_cast<int>(m_VirtW) - 1;
    if (pyRT < 0) pyRT = 0; if (pyRT >= static_cast<int>(m_VirtH)) pyRT = static_cast<int>(m_VirtH) - 1;

    // mapear a coords de mundo con la View actual del RT
    // (SFML 3 mantiene mapPixelToCoords en RenderTarget)
    sf::Vector2f world = m_RT->mapPixelToCoords(sf::Vector2i{ pxRT, pyRT }, m_RT->getView());
    return world;
}

EntityID ViewportPanel::PickEntityAt(const sf::Vector2f& worldPos) const {
    auto& ctx = SceneContext::Get();
    if (!ctx.scene) return 0;

    const auto& entities = ctx.scene->Entities();
    for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
        EntityID id = it->id;
        if (!ctx.scene->transforms.contains(id)) continue;

        const auto& t = ctx.scene->transforms.at(id);

        // half extents efectivos: Sprite.size primero; si no hay Sprite, usa Collider
        sf::Vector2f he{ 0.f, 0.f };
        sf::Vector2f offset{ 0.f, 0.f };

        sf::Vector2f scaleAbs{ std::abs(t.scale.x), std::abs(t.scale.y) };
        if (auto itS = ctx.scene->sprites.find(id); itS != ctx.scene->sprites.end()) {
            he = { (itS->second.size.x * scaleAbs.x) * 0.5f,
                   (itS->second.size.y * scaleAbs.y) * 0.5f };
        }
        else if (auto itC = ctx.scene->colliders.find(id); itC != ctx.scene->colliders.end()) {
            he = { itC->second.halfExtents.x * scaleAbs.x,
                   itC->second.halfExtents.y * scaleAbs.y };
            offset = itC->second.offset;
        }
        else {
            continue; // nada para “pegarle”
        }

        sf::Vector2f center = t.position + offset;
        sf::Vector2f min = { center.x - he.x, center.y - he.y };
        sf::Vector2f max = { center.x + he.x, center.y + he.y };

        if (worldPos.x >= min.x && worldPos.x <= max.x &&
            worldPos.y >= min.y && worldPos.y <= max.y) {
            return id;
        }
    }
    return 0;
}

void ViewportPanel::DrawSelectionGizmo(sf::RenderTarget& rt) const {
    auto& ctx = SceneContext::Get();
    if (!ctx.scene || !ctx.selected) return;

    EntityID id = ctx.selected.id;
    if (!ctx.scene->transforms.contains(id)) return;
    const auto& t = ctx.scene->transforms.at(id);

    sf::Vector2f he{ 0.f, 0.f };
    sf::Vector2f offset{ 0.f, 0.f };
    sf::Vector2f scaleAbs{ std::abs(t.scale.x), std::abs(t.scale.y) };

    if (auto itS = ctx.scene->sprites.find(id); itS != ctx.scene->sprites.end()) {
        he = { (itS->second.size.x * scaleAbs.x) * 0.5f,
               (itS->second.size.y * scaleAbs.y) * 0.5f };
    }
    else if (auto itC = ctx.scene->colliders.find(id); itC != ctx.scene->colliders.end()) {
        he = { itC->second.halfExtents.x * scaleAbs.x,
               itC->second.halfExtents.y * scaleAbs.y };
        offset = itC->second.offset;
    }
    else return;

    sf::RectangleShape box;
    box.setSize(sf::Vector2f{ he.x * 2.f, he.y * 2.f });
    box.setOrigin(he); // centrado
    box.setPosition(t.position + offset);
    box.setFillColor(sf::Color(0, 0, 0, 0));
    box.setOutlineColor(sf::Color(255, 220, 80));
    box.setOutlineThickness(2.f);
    rt.draw(box);
}
