#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Renderer2D.h"
#include "Headers/PhysicsSystem.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui-SFML.h>
#include <optional>
#include <cmath>
#include <algorithm>

static inline void ClampDockedMinWidth(float minW) {
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

ViewportPanel::ViewportPanel() {}

void ViewportPanel::OnAttach() {
    // Intentar cargar iconos desde Assets/Icons
    m_IconPlayOK = m_IcoPlay.loadFromFile("Assets/Icons/play.png");
    m_IconPauseOK = m_IcoPause.loadFromFile("Assets/Icons/pause.png");
    m_IconSelectOK = m_IcoSelect.loadFromFile("Assets/Icons/select.png");
    m_IconPanOK = m_IcoPan.loadFromFile("Assets/Icons/pan.png");

    m_IcoPlay.setSmooth(true);
    m_IcoPause.setSmooth(true);
    m_IcoSelect.setSmooth(true);
    m_IcoPan.setSmooth(true);
}

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

        // Publicamos un valor inicial del centro de cámara
        SceneContext::Get().cameraCenter = m_CamCenter;

        // RT de presentación (flip vertical sólo visual)
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

    if (m_Playing) {
        // En play, simulamos (sin drag del editor)
        Systems::PlayerControllerSystem::Update(*ctx.scene, dt.dt);
        Systems::PhysicsSystem::Update(*ctx.scene, dt.dt);
        Systems::CollisionSystem::SolveGround(*ctx.scene, /*groundY*/ m_VirtH);
        Systems::CollisionSystem::SolveAABB(*ctx.scene);
    }
}

void ViewportPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    auto TogglePlay = [&]() {
        m_Playing = !m_Playing;
        m_Dragging = false;
        m_DragEntity = 0;
        m_Panning = false;
        // Reflejar estado global para que otras capas (menú, etc.) puedan reaccionar
        SceneContext::Get().runtime.playing = m_Playing;
        };

    // Hotkey F5
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) TogglePlay();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ClampDockedMinWidth(480.0f);

    // ───────────────────────────── Toolbar ─────────────────────────────
    const float TB_H = 44.f;
    ImGui::BeginChild("##toolbar", ImVec2(0, TB_H), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Play/Pause
    if (IconButtonPlayPause()) TogglePlay();
    ImGui::SameLine(0.f, 12.f);

    // Pan temporal (Q o Espacio) - queremos que el botón Pan se vea activo mientras se mantiene
    const bool panChord = (!m_Playing) && (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_Space));

    // Herramientas (deshabilitadas en Play)
    ImGui::BeginDisabled(m_Playing);
    {
        ImGuiStyle& st = ImGui::GetStyle();
        const ImVec4 on = st.Colors[ImGuiCol_ButtonActive];
        const ImVec4 off = st.Colors[ImGuiCol_Button];

        // Select (activo sólo si es la tool real y NO hay panChord)
        {
            const bool selectActive = (m_Tool == Tool::Select) && !panChord;
            ImGui::PushStyleColor(ImGuiCol_Button, selectActive ? on : off);
            bool pressed = IconButtonSelect(selectActive);
            ImGui::PopStyleColor();
            if (pressed) m_Tool = Tool::Select;
            ImGui::SameLine();
        }

        // Pan (activo si es la tool real o si hay panChord)
        {
            const bool panActive = (m_Tool == Tool::Pan) || panChord;
            ImGui::PushStyleColor(ImGuiCol_Button, panActive ? on : off);
            bool pressed = IconButtonPan(panActive);
            ImGui::PopStyleColor();
            if (pressed) m_Tool = Tool::Pan;
        }

        // Hotkeys (sólo en pausa)
        if (ImGui::IsKeyPressed(ImGuiKey_1)) m_Tool = Tool::Select;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) m_Tool = Tool::Pan;
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0.f, 24.f);
    ImGui::TextUnformatted("|  Snap: ON  ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.f);
    ImGui::DragFloat("Grid", &m_Grid, 1.f, 4.f, 512.f, "%.0f");

    ImGui::EndChild();

    // ───────────────────────── Viewport area ───────────────────────────
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
            // Cámara: en play sigue al player, en pausa se mantiene donde esté
            sf::Vector2f desiredCenter = m_CamCenter;
            if (m_Playing) {
                EntityID playerId = 0;
                if (ctx.selected && ctx.scene->playerControllers.contains(ctx.selected.id))
                    playerId = ctx.selected.id;
                else if (!ctx.scene->playerControllers.empty())
                    playerId = ctx.scene->playerControllers.begin()->first;
                if (playerId && ctx.scene->transforms.contains(playerId))
                    desiredCenter = ctx.scene->transforms[playerId].position;
            }
            if (!m_Dragging && !m_Panning) m_CamCenter = desiredCenter;

            sf::View v = m_RT->getView();
            v.setCenter(m_CamCenter);
            m_RT->setView(v);

            // 👈 ACTUALIZAMOS cameraCenter CADA FRAME
            SceneContext::Get().cameraCenter = m_CamCenter;

            // Grilla
            DrawGrid(*m_RT);

            // Objetos
            Renderer2D::Draw(*ctx.scene, *m_RT);

            // Gizmo de selección SOLO en pausa
            if (!m_Playing) DrawSelectionGizmo(*m_RT);
        }
        m_RT->display();

        // 2) Espejo vertical a RT de presentación (sólo visual)
        m_PresentRT->clear(sf::Color::Black);
        sf::Sprite spr(m_RT->getTexture());
        spr.setScale(sf::Vector2f{ 1.f, -1.f });
        spr.setPosition(sf::Vector2f{ 0.f, static_cast<float>(m_VirtH) });
        m_PresentRT->draw(spr);
        m_PresentRT->display();

        // 3) Mostrar con letterboxing
        ImVec2 imgSize{ targetW, targetH };
        ImVec2 cur = ImGui::GetCursorPos();
        ImVec2 offset{ (avail.x - imgSize.x) * 0.5f, (avail.y - imgSize.y) * 0.5f };
        if (offset.x < 0) offset.x = 0;
        if (offset.y < 0) offset.y = 0;
        ImGui::SetCursorPos(ImVec2(cur.x + offset.x, cur.y + offset.y));

        ImGui::Image(m_PresentRT->getTexture(), imgSize);

        // --- Picking / Drag / Pan ---
        ImVec2 imgMin = ImGui::GetItemRectMin();
        ImVec2 imgMax = ImGui::GetItemRectMax();
        ImGuiIO& io = ImGui::GetIO();

        // PAN (herramienta Pan o panChord + LMB) SOLO EN PAUSA
        if (!m_Playing) {
            const bool hovered = ImGui::IsItemHovered();

            // Cambiar cursor cuando hay contexto de pan (herramienta activa o atajo pulsado)
            if (hovered && ((m_Tool == Tool::Pan) || panChord)) {
                ImGui::SetMouseCursor(m_Panning ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);
            }

            const bool wantPan =
                ((m_Tool == Tool::Pan) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) ||
                (panChord && ImGui::IsMouseDown(ImGuiMouseButton_Left));

            // Iniciar pan
            if (!m_Panning && wantPan && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_Panning = true;
                m_Dragging = false;
                m_DragEntity = 0;
            }

            // Actualizar pan
            if (m_Panning) {
                if (!wantPan || ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    m_Panning = false;
                }
                else if (hovered) {
                    // Convertir delta ImGui -> RT
                    const float scaleX = static_cast<float>(m_VirtW) / (imgMax.x - imgMin.x);
                    const float scaleY = static_cast<float>(m_VirtH) / (imgMax.y - imgMin.y);
                    sf::Vector2f deltaRT{ io.MouseDelta.x * scaleX, io.MouseDelta.y * scaleY };
                    m_CamCenter -= deltaRT; // arrastrar lienzo
                }
            }
        }

        // EDITAR ENTIDADES (sólo en pausa, herramienta Select y si NO estamos paneando)
        if (!m_Playing && m_Tool == Tool::Select && !m_Panning) {
            if (ImGui::IsItemHovered()) {
                if (auto worldOpt = ScreenToWorld(io.MousePos, imgMin, imgMax)) {
                    sf::Vector2f world = *worldOpt;

                    // Click para seleccionar + comenzar drag
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        EntityID id = PickEntityAt(world);
                        auto& c = SceneContext::Get();
                        if (c.scene && id) {
                            c.selected = Entity{ id };
                            m_Dragging = true;
                            m_DragEntity = id;
                            m_DragOffset = c.scene->transforms[id].position - world; // offset cómodo
                        }
                        else {
                            c.selected = Entity{};
                        }
                    }

                    // Arrastre (Snap ON)
                    if (m_Dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_DragEntity) {
                        auto& c2 = SceneContext::Get();
                        if (c2.scene && c2.scene->transforms.contains(m_DragEntity)) {
                            sf::Vector2f pos = world + m_DragOffset;
                            if (m_Grid > 0.0f) {
                                pos.x = std::round(pos.x / m_Grid) * m_Grid;
                                pos.y = std::round(pos.y / m_Grid) * m_Grid;
                            }
                            c2.scene->transforms[m_DragEntity].position = pos;

                            // Si tiene física, lo frenamos
                            if (c2.scene->physics.contains(m_DragEntity)) {
                                auto& ph = c2.scene->physics[m_DragEntity];
                                ph.velocity = { 0.f, 0.f };
                                ph.onGround = false;
                            }
                        }
                    }
                }
            }

            // Soltar drag de entidad
            if (m_Dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_Dragging = false;
                m_DragEntity = 0;
            }
        }
    }
    else {
        ImGui::TextUnformatted("Creando RenderTextures...");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// --------------------------- Picking / utilidades ---------------------------

std::optional<sf::Vector2f> ViewportPanel::ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const {
    if (!m_RT) return std::nullopt;

    // ¿mouse dentro del rectángulo de la imagen?
    if (mouse.x < imgMin.x || mouse.y < imgMin.y || mouse.x > imgMax.x || mouse.y > imgMax.y)
        return std::nullopt;

    // uv [0..1] dentro de la imagen (flip visual ya manejado en presentación)
    ImVec2 imgSize{ imgMax.x - imgMin.x, imgMax.y - imgMin.y };
    float u = (mouse.x - imgMin.x) / imgSize.x;
    float v = (mouse.y - imgMin.y) / imgSize.y;

    // píxel en el RT
    int pxRT = static_cast<int>(u * static_cast<float>(m_VirtW));
    int pyRT = static_cast<int>(v * static_cast<float>(m_VirtH));

    // Clamp
    if (pxRT < 0) pxRT = 0; if (pxRT >= static_cast<int>(m_VirtW)) pxRT = static_cast<int>(m_VirtW) - 1;
    if (pyRT < 0) pyRT = 0; if (pyRT >= static_cast<int>(m_VirtH)) pyRT = static_cast<int>(m_VirtH) - 1;

    // mapear a coords de mundo con la View actual del RT
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

        // half extents efectivos (Sprite.size primero; si no hay, usa Collider)
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
            continue;
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

// --------------------------- Gizmos / dibujo ---------------------------

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
    box.setOrigin(he);
    box.setPosition(t.position + offset);
    box.setFillColor(sf::Color(0, 0, 0, 0));
    box.setOutlineColor(sf::Color(255, 220, 80));
    box.setOutlineThickness(2.f);
    rt.draw(box);
}

void ViewportPanel::DrawGrid(sf::RenderTarget& rt) const {
    if (m_Grid <= 0.0f) return;

    const sf::View& view = rt.getView();
    const sf::Vector2f c = view.getCenter();
    const sf::Vector2f s = view.getSize();

    const float left = c.x - s.x * 0.5f;
    const float right = c.x + s.x * 0.5f;
    const float top = c.y - s.y * 0.5f;
    const float bottom = c.y + s.y * 0.5f;

    const float startX = std::floor(left / m_Grid) * m_Grid;
    const float startY = std::floor(top / m_Grid) * m_Grid;

    const sf::Color minor(60, 60, 70, 255);
    const sf::Color major(90, 90, 110, 255);

    sf::VertexArray lines(sf::PrimitiveType::Lines);

    for (float x = startX; x <= right + m_Grid; x += m_Grid) {
        bool isMajor = (static_cast<int>(std::round(x / m_Grid)) % 10) == 0;
        sf::Color col = isMajor ? major : minor;
        lines.append(sf::Vertex(sf::Vector2f(x, top), col));
        lines.append(sf::Vertex(sf::Vector2f(x, bottom), col));
    }
    for (float y = startY; y <= bottom + m_Grid; y += m_Grid) {
        bool isMajor = (static_cast<int>(std::round(y / m_Grid)) % 10) == 0;
        sf::Color col = isMajor ? major : minor;
        lines.append(sf::Vertex(sf::Vector2f(left, y), col));
        lines.append(sf::Vertex(sf::Vector2f(right, y), col));
    }

    rt.draw(lines);
}

// ───────────────────────── Toolbar Icon Buttons ─────────────────────────

static ImVec2 FitIconHeight(const sf::Texture& tex, float btnH) {
    const auto size = tex.getSize();
    if (size.y == 0) return ImVec2(btnH, btnH);
    float scale = btnH / static_cast<float>(size.y);
    return ImVec2(size.x * scale, btnH);
}

static bool IconButtonFromTexture(const char* id,
    const sf::Texture& tex,
    float height,
    const char* tooltip,
    bool toggled = false)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 posStart = ImGui::GetCursorScreenPos();
    ImVec2 sz = FitIconHeight(tex, height);

    ImGui::InvisibleButton(id, ImVec2(sz.x, sz.y));
    bool pressed = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImGui::SetCursorScreenPos(posStart);
    ImGui::Image(tex, sz);
    ImGui::SetCursorScreenPos(ImVec2(posStart.x + sz.x, posStart.y));

    if (hovered || active || toggled) {
        ImU32 col = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive
            : (toggled ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered));
        dl->AddRect(posStart, ImVec2(posStart.x + sz.x, posStart.y + sz.y), col, 6.0f, 0, 2.0f);
    }

    if (hovered && tooltip && *tooltip) ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

bool ViewportPanel::IconButtonPlayPause() {
    const float h = 28.f;
    if (m_Playing ? m_IconPauseOK : m_IconPlayOK) {
        const sf::Texture& tex = m_Playing ? m_IcoPause : m_IcoPlay;
        return IconButtonFromTexture("##btn_play", tex, h, m_Playing ? "Pause (F5)" : "Play (F5)");
    }
    else {
        bool pressed = ImGui::Button(m_Playing ? "Pause" : "Play", ImVec2(48, h));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", m_Playing ? "Pause (F5)" : "Play (F5)");
        return pressed;
    }
}

bool ViewportPanel::IconButtonSelect(bool active) {
    const float h = 28.f;
    if (m_IconSelectOK) {
        return IconButtonFromTexture("##btn_select", m_IcoSelect, h, "Seleccionar (1)", active);
    }
    else {
        bool pressed = ImGui::Button("Select", ImVec2(56, h));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Seleccionar (1)");
        return pressed;
    }
}

bool ViewportPanel::IconButtonPan(bool active) {
    const float h = 28.f;
    if (m_IconPanOK) {
        return IconButtonFromTexture("##btn_pan", m_IcoPan, h,
            "Arrastrar (2)",
            active);
    }
    else {
        bool pressed = ImGui::Button("Pan", ImVec2(44, h));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", "Arrastrar (2)");
        return pressed;
    }
}
