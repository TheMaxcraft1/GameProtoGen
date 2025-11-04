#include "ViewportPanel.h"
#include "Runtime/SceneContext.h"
#include "Runtime/EditorContext.h"   // ⬅️ nuevo
#include "Runtime/GameRunner.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui-SFML.h>
#include <optional>
#include <cmath>
#include <algorithm>

std::vector<std::string> ViewportPanel::s_Log{};  // ← DEFINICIÓN ÚNICA

static inline void ClampDockedMinWidth(float minW) {
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

ViewportPanel::ViewportPanel() {}

void ViewportPanel::OnAttach() {
    // Intentar cargar iconos desde Assets/Icons
    m_IconPlayOK = m_IcoPlay.loadFromFile("Internal/Icons/play.png");
    m_IconPauseOK = m_IcoPause.loadFromFile("Internal/Icons/pause.png");
    m_IconSelectOK = m_IcoSelect.loadFromFile("Internal/Icons/select.png");
    m_IconPanOK = m_IcoPan.loadFromFile("Internal/Icons/pan.png");
    m_IconRotateOK = m_IcoRotate.loadFromFile("Internal/Icons/rotate.png");
    m_IconScaleOK = m_IcoScale.loadFromFile("Internal/Icons/scale.png");

    m_IcoPlay.setSmooth(true);
    m_IcoPause.setSmooth(true);
    m_IcoSelect.setSmooth(true);
    m_IcoPan.setSmooth(true);
    m_IcoRotate.setSmooth(true);
    m_IcoScale.setSmooth(true);
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

        // Publicamos un valor inicial del centro de cámara (EditorContext)
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
    auto& scx = SceneContext::Get();
    if (!scx.scene) return;

    if (m_Playing) {
        GameRunner::Step(*scx.scene, dt.dt);
    }
}

void ViewportPanel::OnGuiRender() {
    auto& scx = SceneContext::Get();
    auto& edx = EditorContext::Get();

    // Se usa cuando se duplica una entidad.
    if (edx.requestSelectTool) {
        m_Tool = Tool::Select;
        edx.requestSelectTool = false;
    }

    auto TogglePlay = [&]() {
        auto& scx = SceneContext::Get();
        auto& edx = EditorContext::Get();

        const bool wasPlaying = m_Playing;
        const bool toPlay = !m_Playing;

        // reset de gestos de edición
        m_Dragging = false;
        m_DragEntity = 0;
        m_Panning = false;

        if (toPlay) {
            // ──────────────── ENTER PLAY ────────────────
            if (!scx.scene) return;
            // 1) snapshot profundo de la escena
            edx.runtime.sceneBackup = std::make_shared<Scene>(*scx.scene);
            // 2) backup de cámara + selección
            edx.runtime.cameraBackup = m_CamCenter;
            edx.runtime.selectedBackup = edx.selected;

            // 3) runtime on
            GameRunner::EnterPlay(*scx.scene);
            m_Playing = true;
            edx.runtime.playing = true;

            AppendLog("Runtime: Play");
        }
        else {
            // ──────────────── EXIT PLAY ────────────────
            if (scx.scene) {
                GameRunner::ExitPlay(*scx.scene);
            }

            // 1) restaurar snapshot si lo tenemos
            if (edx.runtime.sceneBackup) {
                scx.scene = edx.runtime.sceneBackup;
                edx.runtime.sceneBackup.reset();
            }

            m_CamCenter = edx.runtime.cameraBackup;
            scx.cameraCenter = m_CamCenter;
            edx.selected = edx.runtime.selectedBackup;

            // 4) runtime off
            m_Playing = false;
            edx.runtime.playing = false;

            AppendLog("Runtime: Stop (escena restaurada)");
        }
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

    // Pan temporal (Q o Espacio)
    const bool panChord = (!m_Playing) && (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_Space));

    // Herramientas (deshabilitadas en Play)
    ImGui::BeginDisabled(m_Playing);
    {
        ImGuiStyle& st = ImGui::GetStyle();
        const ImVec4 on = st.Colors[ImGuiCol_ButtonActive];
        const ImVec4 off = st.Colors[ImGuiCol_Button];

        // Select
        {
            const bool selectActive = (m_Tool == Tool::Select) && !panChord;
            ImGui::PushStyleColor(ImGuiCol_Button, selectActive ? on : off);
            bool pressed = IconButtonSelect(selectActive);
            ImGui::PopStyleColor();
            if (pressed) m_Tool = Tool::Select;
            ImGui::SameLine();
        }

        // Pan
        {
            const bool panActive = (m_Tool == Tool::Pan) || panChord;
            ImGui::PushStyleColor(ImGuiCol_Button, panActive ? on : off);
            bool pressed = IconButtonPan(panActive);
            ImGui::PopStyleColor();
            if (pressed) m_Tool = Tool::Pan;
            ImGui::SameLine();
        }

        // Rotate
        {
            const bool rotActive = (m_Tool == Tool::Rotate);
            ImGui::PushStyleColor(ImGuiCol_Button, rotActive ? on : off);
            bool pressed = IconButtonRotate(rotActive);
            ImGui::PopStyleColor();
            if (pressed) m_Tool = Tool::Rotate;
        }

        // Scale
        {
            const bool scaleActive = (m_Tool == Tool::Scale);
            ImGui::PushStyleColor(ImGuiCol_Button, scaleActive ? on : off);
            bool pressed = IconButtonScale(scaleActive);
            ImGui::PopStyleColor();
            if (pressed) m_Tool = Tool::Scale;
        }

        // Hotkeys (sólo en pausa)
        if (ImGui::IsKeyPressed(ImGuiKey_1)) m_Tool = Tool::Select;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) m_Tool = Tool::Pan;
        if (ImGui::IsKeyPressed(ImGuiKey_3)) m_Tool = Tool::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_4)) m_Tool = Tool::Scale;
    }
    ImGui::EndDisabled();

    ImGui::EndChild();

    // ───────────────────────── Viewport area ───────────────────────────
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Alto fijo consola
    const float consoleHeight = 140.f;
    float availForImageY = std::max(0.0f, avail.y - consoleHeight - 6.0f);

    float targetW = std::floor(avail.x > 1 ? avail.x : 1.0f);
    float targetH = std::floor(targetW * 9.0f / 16.0f);
    if (targetH > availForImageY) {
        targetH = std::floor(availForImageY > 1 ? availForImageY : 1.0f);
        targetW = std::floor(targetH * 16.0f / 9.0f);
    }

    EnsureRT();

    if (m_RT && m_PresentRT) {
        // 1) Dibujar escena en RT principal
        m_RT->clear(sf::Color(30, 30, 35));
        if (scx.scene) {
            // Cámara: en play sigue al player; en pausa usa m_CamCenter
            sf::Vector2f desiredCenter = m_CamCenter;
            if (m_Playing) {
                EntityID playerId = 0;
                if (edx.selected && scx.scene->playerControllers.contains(edx.selected.id))
                    playerId = edx.selected.id;
                else if (!scx.scene->playerControllers.empty())
                    playerId = scx.scene->playerControllers.begin()->first;
                if (playerId && scx.scene->transforms.contains(playerId))
                    desiredCenter = scx.scene->transforms[playerId].position;
            }
            if (!m_Dragging && !m_Panning) m_CamCenter = desiredCenter;

            sf::View v = m_RT->getView();
            v.setCenter(m_CamCenter);
            m_RT->setView(v);

            scx.cameraCenter = m_CamCenter;

            // Grilla SOLO en pausa/edición
            if (!m_Playing) DrawGrid(*m_RT);

            // Objetos
            GameRunner::Render(*scx.scene, *m_RT, m_CamCenter, { m_VirtW, m_VirtH });

            // Gizmo de selección SOLO en pausa
            if (!m_Playing) DrawSelectionGizmo(*m_RT);
        }
        m_RT->display();

        // 2) Espejo vertical a RT de presentación
        m_PresentRT->clear(sf::Color::Black);
        sf::Sprite spr(m_RT->getTexture());
        spr.setScale(sf::Vector2f{ 1.f, -1.f });
        spr.setPosition(sf::Vector2f{ 0.f, static_cast<float>(m_VirtH) });
        m_PresentRT->draw(spr);
        m_PresentRT->display();

        // 3) Mostrar con letterboxing
        ImVec2 imgSize{ targetW, targetH };
        ImVec2 cur = ImGui::GetCursorPos();
        ImVec2 offset{ (avail.x - imgSize.x) * 0.5f, (availForImageY - imgSize.y) * 0.5f };
        if (offset.x < 0) offset.x = 0;
        if (offset.y < 0) offset.y = 0;
        ImGui::SetCursorPos(ImVec2(cur.x + offset.x, cur.y + offset.y));

        ImGui::Image(m_PresentRT->getTexture(), imgSize);

        // --- Picking / Drag / Pan ---
        ImVec2 imgMin = ImGui::GetItemRectMin();
        ImVec2 imgMax = ImGui::GetItemRectMax();
        ImGuiIO& io = ImGui::GetIO();

        // PAN (sólo en pausa)
        if (!m_Playing) {
            const bool hovered = ImGui::IsItemHovered();

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
                    const float scaleX = static_cast<float>(m_VirtW) / (imgMax.x - imgMin.x);
                    const float scaleY = static_cast<float>(m_VirtH) / (imgMax.y - imgMin.y);
                    sf::Vector2f deltaRT{ io.MouseDelta.x * scaleX, io.MouseDelta.y * scaleY };
                    m_CamCenter -= deltaRT;
                }
            }
        }

        // ROTATE (sólo en pausa)
        if (!m_Playing && m_Tool == Tool::Rotate && !m_Panning) {
            const bool hovered = ImGui::IsItemHovered();

            if (hovered) {
                ImGui::SetMouseCursor(m_Rotating ? ImGuiMouseCursor_ResizeNESW : ImGuiMouseCursor_Hand);

                if (auto worldOpt = ScreenToWorld(io.MousePos, imgMin, imgMax)) {
                    sf::Vector2f world = *worldOpt;

                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        EntityID hit = PickEntityAt(world);
                        if (scx.scene && hit) {
                            if (!edx.selected || edx.selected.id != hit) {
                                edx.selected = Entity{ hit };
                                AppendLog("Seleccionado entity id=" + std::to_string(hit));
                            }

                            m_Rotating = true;
                            m_Dragging = false;
                            m_DragEntity = hit;

                            auto& t = scx.scene->transforms[hit];
                            sf::Vector2f center = t.position;
                            if (auto itC = scx.scene->colliders.find(hit); itC != scx.scene->colliders.end()) {
                                center += itC->second.offset;
                            }

                            auto angleFrom = [&](const sf::Vector2f& p) -> float {
                                return std::atan2(p.y - center.y, p.x - center.x) * 180.0f / 3.14159265f;
                                };

                            m_RotateStartAngle = angleFrom(world);
                            m_RotateStartEntityAngle = t.rotationDeg;
                            AppendLog("Rotar: inicio id=" + std::to_string(hit));
                        }
                    }

                    // Mientras se mantiene
                    if (m_Rotating && ImGui::IsMouseDown(ImGuiMouseButton_Left) && scx.scene && edx.selected) {
                        EntityID id = edx.selected.id;
                        if (scx.scene->transforms.contains(id)) {
                            auto& t = scx.scene->transforms[id];
                            sf::Vector2f center = t.position;
                            if (auto itC = scx.scene->colliders.find(id); itC != scx.scene->colliders.end()) {
                                center += itC->second.offset;
                            }

                            auto angleFrom = [&](const sf::Vector2f& p) -> float {
                                return std::atan2(p.y - center.y, p.x - center.x) * 180.0f / 3.14159265f;
                                };

                            float now = angleFrom(world);
                            float delta = now - m_RotateStartAngle;

                            // Snap con Shift (15°)
                            if (io.KeyShift) {
                                const float step = 15.f;
                                delta = std::round(delta / step) * step;
                            }
                            t.rotationDeg = m_RotateStartEntityAngle + delta;
                        }
                    }
                }
            }

            // Al soltar
            if (m_Rotating && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (scx.scene && edx.selected && scx.scene->transforms.contains(edx.selected.id)) {
                    float finalDeg = scx.scene->transforms[edx.selected.id].rotationDeg;
                    AppendLog("Rotar: fin id=" + std::to_string(edx.selected.id) +
                        " -> " + std::to_string((int)std::round(finalDeg)) + "°");
                }
                m_Rotating = false;
                m_DragEntity = 0;
            }
        }

        // SCALE (sólo en pausa)
        if (!m_Playing && m_Tool == Tool::Scale && !m_Panning) {
            const bool hovered = ImGui::IsItemHovered();

            if (hovered) {
                ImGui::SetMouseCursor(m_Scaling ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);

                if (auto worldOpt = ScreenToWorld(io.MousePos, imgMin, imgMax)) {
                    sf::Vector2f world = *worldOpt;

                    // Iniciar
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        EntityID hit = PickEntityAt(world);
                        if (scx.scene && hit) {
                            if (!edx.selected || edx.selected.id != hit) {
                                edx.selected = Entity{ hit };
                                AppendLog("Seleccionado entity id=" + std::to_string(hit));
                            }
                            if (scx.scene->transforms.contains(hit)) {
                                const auto& t = scx.scene->transforms[hit];
                                sf::Vector2f center = t.position;
                                if (auto itC = scx.scene->colliders.find(hit); itC != scx.scene->colliders.end())
                                    center += itC->second.offset;

                                m_Scaling = true;
                                m_Dragging = false;
                                m_DragEntity = hit;

                                m_ScaleStartMouse = world - center;
                                m_ScaleStartLen = std::max(1e-3f, std::sqrt(m_ScaleStartMouse.x * m_ScaleStartMouse.x + m_ScaleStartMouse.y * m_ScaleStartMouse.y));
                                m_ScaleStartEntityScale = t.scale;

                                AppendLog("Escalar: inicio id=" + std::to_string(hit));
                            }
                        }
                    }

                    // Actualizar
                    if (m_Scaling && ImGui::IsMouseDown(ImGuiMouseButton_Left) && scx.scene && edx.selected) {
                        EntityID id = edx.selected.id;
                        if (scx.scene->transforms.contains(id)) {
                            auto& t = scx.scene->transforms[id];
                            sf::Vector2f center = t.position;
                            if (auto itC = scx.scene->colliders.find(id); itC != scx.scene->colliders.end())
                                center += itC->second.offset;

                            sf::Vector2f cur = world - center;
                            float curLen = std::max(1e-3f, std::sqrt(cur.x * cur.x + cur.y * cur.y));

                            auto safe_ratio = [](float num, float den) {
                                return (std::fabs(den) < 1e-3f) ? 1.f : (num / den);
                                };
                            float rx = safe_ratio(cur.x, m_ScaleStartMouse.x);
                            float ry = safe_ratio(cur.y, m_ScaleStartMouse.y);
                            float ru = curLen / m_ScaleStartLen;

                            auto soften = [&](float r) { return 1.f + m_ScaleSensitivity * (r - 1.f); };

                            const float kAxisEps = 4.f;

                            float fx = (std::fabs(m_ScaleStartMouse.x) < kAxisEps) ? soften(ru) : soften(rx);
                            float fy = (std::fabs(m_ScaleStartMouse.y) < kAxisEps) ? soften(ru) : soften(ry);

                            // Uniforme con Shift
                            if (io.KeyShift) {
                                float fu = soften(ru);
                                fx = fy = fu;
                            }

                            sf::Vector2f newScale{
                                m_ScaleStartEntityScale.x * fx,
                                m_ScaleStartEntityScale.y * fy
                            };

                            // Snap con Ctrl (0.1)
                            if (io.KeyCtrl) {
                                auto snap = [](float v, float step) {
                                    return std::round(v / step) * step;
                                    };
                                newScale.x = snap(newScale.x, 0.1f);
                                newScale.y = snap(newScale.y, 0.1f);
                            }

                            // Clamp mínimo
                            newScale.x = std::clamp(newScale.x, -1000.f, -0.05f) < 0.f ? newScale.x : std::max(newScale.x, 0.05f);
                            newScale.y = std::clamp(newScale.y, -1000.f, -0.05f) < 0.f ? newScale.y : std::max(newScale.y, 0.05f);

                            t.scale = newScale;
                        }
                    }
                }
            }

            // Finalizar
            if (m_Scaling && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (scx.scene && edx.selected && scx.scene->transforms.contains(edx.selected.id)) {
                    const auto s = scx.scene->transforms[edx.selected.id].scale;
                    AppendLog("Escalar: fin id=" + std::to_string(edx.selected.id) +
                        " -> (" + std::to_string(s.x) + ", " + std::to_string(s.y) + ")");
                }
                m_Scaling = false;
                m_DragEntity = 0;
            }
        }

        // SELECT / DRAG (sólo en pausa)
        if (!m_Playing && m_Tool == Tool::Select && !m_Panning) {
            if (ImGui::IsItemHovered()) {
                if (auto worldOpt = ScreenToWorld(io.MousePos, imgMin, imgMax)) {
                    sf::Vector2f world = *worldOpt;

                    // Click para seleccionar + comenzar drag
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        EntityID id = PickEntityAt(world);
                        if (scx.scene && id) {
                            edx.selected = Entity{ id };
                            m_Dragging = true;
                            m_DragEntity = id;
                            m_DragOffset = scx.scene->transforms[id].position - world;
                            AppendLog("Seleccionado entity id=" + std::to_string(id));
                        }
                        else {
                            edx.selected = Entity{};
                        }
                    }

                    // Arrastre
                    if (m_Dragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_DragEntity) {
                        if (scx.scene && scx.scene->transforms.contains(m_DragEntity)) {
                            sf::Vector2f pos = world + m_DragOffset;
                            if (m_Grid > 0.0f) {
                                pos.x = std::round(pos.x / m_Grid) * m_Grid;
                                pos.y = std::round(pos.y / m_Grid) * m_Grid;
                            }
                            scx.scene->transforms[m_DragEntity].position = pos;

                            if (scx.scene->physics.contains(m_DragEntity)) {
                                auto& ph = scx.scene->physics[m_DragEntity];
                                ph.velocity = { 0.f, 0.f };
                                ph.onGround = false;
                            }
                        }
                    }
                }
            }

            // Soltar drag
            if (m_Dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (m_DragEntity) {
                    if (scx.scene && scx.scene->transforms.contains(m_DragEntity)) {
                        const auto p = scx.scene->transforms[m_DragEntity].position;
                        AppendLog("Soltado entity id=" + std::to_string(m_DragEntity) +
                            " en (" + std::to_string((int)p.x) + "," + std::to_string((int)p.y) + ")");
                    }
                }
                m_Dragging = false;
                m_DragEntity = 0;
            }
        }
    }
    else {
        ImGui::TextUnformatted("Creando RenderTextures...");
    }

    // Separación mínima antes de la consola
    ImGui::Dummy(ImVec2(0, 6.0f));

    // ───────────────────────── Consola ───────────────────────────
    DrawConsole(/*height*/ 140.f);

    ImGui::End();
    ImGui::PopStyleVar();
}

// --------------------------- Picking / utilidades ---------------------------

std::optional<sf::Vector2f> ViewportPanel::ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const {
    if (!m_RT) return std::nullopt;

    if (mouse.x < imgMin.x || mouse.y < imgMin.y || mouse.x > imgMax.x || mouse.y > imgMax.y)
        return std::nullopt;

    ImVec2 imgSize{ imgMax.x - imgMin.x, imgMax.y - imgMin.y };
    float u = (mouse.x - imgMin.x) / imgSize.x;
    float v = (mouse.y - imgMin.y) / imgSize.y;

    int pxRT = static_cast<int>(u * static_cast<float>(m_VirtW));
    int pyRT = static_cast<int>(v * static_cast<float>(m_VirtH));

    if (pxRT < 0) pxRT = 0; if (pxRT >= static_cast<int>(m_VirtW)) pxRT = static_cast<int>(m_VirtW) - 1;
    if (pyRT < 0) pyRT = 0; if (pyRT >= static_cast<int>(m_VirtH)) pyRT = static_cast<int>(m_VirtH) - 1;

    sf::Vector2f world = m_RT->mapPixelToCoords(sf::Vector2i{ pxRT, pyRT }, m_RT->getView());
    return world;
}

EntityID ViewportPanel::PickEntityAt(const sf::Vector2f& worldPos) const {
    auto& scx = SceneContext::Get();
    if (!scx.scene) return 0;

    const auto& entities = scx.scene->Entities();
    for (auto it = entities.rbegin(); it != entities.rend(); ++it) {
        EntityID id = it->id;
        if (!scx.scene->transforms.contains(id)) continue;

        const auto& t = scx.scene->transforms.at(id);

        sf::Vector2f he{ 0.f, 0.f };
        sf::Vector2f offset{ 0.f, 0.f };

        sf::Vector2f scaleAbs{ std::abs(t.scale.x), std::abs(t.scale.y) };
        if (auto itS = scx.scene->sprites.find(id); itS != scx.scene->sprites.end()) {
            he = { (itS->second.size.x * scaleAbs.x) * 0.5f,
                   (itS->second.size.y * scaleAbs.y) * 0.5f };
        }
        else if (auto itC = scx.scene->colliders.find(id); itC != scx.scene->colliders.end()) {
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
    auto& scx = SceneContext::Get();
    auto& edx = EditorContext::Get();
    if (!scx.scene || !edx.selected) return;

    EntityID id = edx.selected.id;
    if (!scx.scene->transforms.contains(id)) return;
    const auto& t = scx.scene->transforms.at(id);

    sf::Vector2f he{ 0.f, 0.f };
    sf::Vector2f offset{ 0.f, 0.f };
    sf::Vector2f scaleAbs{ std::abs(t.scale.x), std::abs(t.scale.y) };

    if (auto itS = scx.scene->sprites.find(id); itS != scx.scene->sprites.end()) {
        he = { (itS->second.size.x * scaleAbs.x) * 0.5f,
               (itS->second.size.y * scaleAbs.y) * 0.5f };
    }
    else if (auto itC = scx.scene->colliders.find(id); itC != scx.scene->colliders.end()) {
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
        return IconButtonFromTexture("##btn_pan", m_IcoPan, h, "Arrastrar (2)", active);
    }
    else {
        bool pressed = ImGui::Button("Pan", ImVec2(44, h));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", "Arrastrar (2)");
        return pressed;
    }
}

bool ViewportPanel::IconButtonRotate(bool active) {
    const float h = 28.f;
    if (m_IconRotateOK) {
        return IconButtonFromTexture("##btn_rotate", m_IcoRotate, h, "Rotar (3)", active);
    }
    else {
        bool pressed = ImGui::Button("Rotate", ImVec2(64, h));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", "Rotar (3)");
        return pressed;
    }
}

bool ViewportPanel::IconButtonScale(bool active) {
    const float h = 28.f;
    if (m_IconScaleOK) {
        return IconButtonFromTexture("##btn_scale", m_IcoScale, h, "Escalar (4)", active);
    }
    else {
        bool pressed = ImGui::Button("Scale", ImVec2(56, h));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", "Escalar (4)");
        return pressed;
    }
}

// ───────────────────────── Consola ─────────────────────────

void ViewportPanel::AppendLog(const std::string& line) {
    s_Log.emplace_back(line);
}

void ViewportPanel::DrawConsole(float height) {
    const ImVec4 kHeaderBg = ImVec4(209.0f / 255.0f, 209.0f / 255.0f, 209.0f / 255.0f, 1.0f);
    const ImVec4 kContentBg = ImVec4(227.0f / 255.0f, 227.0f / 255.0f, 227.0f / 255.0f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kHeaderBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("##console", ImVec2(0, height), true, ImGuiWindowFlags_NoScrollbar);

    if (ImGui::Button("Limpiar")) {
        s_Log.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Desplazamiento automático", &m_AutoScroll);

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kContentBg);
    ImGui::BeginChild("##console_scroller", ImVec2(0, -28), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& line : s_Log) {
        ImGui::TextUnformatted(line.c_str());
    }
    if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
