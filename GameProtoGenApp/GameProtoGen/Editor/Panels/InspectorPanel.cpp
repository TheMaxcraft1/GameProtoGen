#include "InspectorPanel.h"
#include "Runtime/SceneContext.h"
#include "ECS/Components.h"
#include "Editor/EditorFonts.h"
#include "Systems/Renderer2D.h"
#include <imgui_stdlib.h>
#include <imgui-SFML.h>
#include <cfloat>

#include <imgui.h>
#include <cstdint>
#include <algorithm>

static inline void ClampDockedMinWidth(float minW) {
    // En dock, los constraints no se respetan: forzamos tamaño si se pasa.
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

static void DrawTexture2DEditor(Scene& scene, Entity e) {
    if (!e) return;

    // ¿La entidad tiene componente Texture2D?
    bool hasTex = scene.textures.contains(e.id);
    if (ImGui::CollapsingHeader("Texture2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(false);

        if (!hasTex) {
            if (ImGui::Button("Agregar componente Texture2D")) {
                scene.textures[e.id] = Texture2D{}; // path vacío por ahora
            }
            ImGui::EndDisabled();
            return;
        }

        // Campo path (editable)
        auto& tex = scene.textures[e.id];
        std::string originalPath = tex.path;

        ImGui::InputText("Path", &tex.path);

        ImGui::SameLine();
        if (ImGui::Button("Quitar")) {
            // Borra el componente de textura y deja que el renderer dibuje el rectángulo de Sprite
            scene.textures.erase(e.id);
            ImGui::EndDisabled();
            return;
        }

        // Botones de utilidad
        if (ImGui::Button("Recargar")) {
            // Invalidar sólo esta textura para forzar reload
            Renderer2D::InvalidateTexture(tex.path);
        }
        ImGui::SameLine();
        if (ImGui::Button("Limpiar caché")) {
            Renderer2D::ClearTextureCache();
        }

        // Hint: dónde poner los assets
        ImGui::TextUnformatted("Sugerido: colocar archivos bajo Assets/ ...");

        // Preview (si hay path)
        if (!tex.path.empty()) {
            auto sp = Renderer2D::GetTextureCached(tex.path);
            if (sp && sp->getSize().x > 0 && sp->getSize().y > 0) {
                const auto sz = sp->getSize();
                const float texW = static_cast<float>(sz.x);
                const float texH = static_cast<float>(sz.y);

                // área disponible en el Inspector
                const float availW = std::max(32.0f, ImGui::GetContentRegionAvail().x);
                const float maxH = 220.0f; // altura máx. del preview (ajustá a gusto)

                // Fit proporcional (contain)
                const float scale = std::min(availW / texW, maxH / texH);
                const ImVec2 drawSz{ texW * scale, texH * scale };

                ImGui::Separator();
                ImGui::Text("Preview (%u x %u)", sz.x, sz.y);

                // Marco con altura fija para centrar la imagen
#if IMGUI_VERSION_NUM >= 19000
                ImGui::BeginChild("##texprev", ImVec2(0, maxH), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
#else
                ImGui::BeginChild("##texprev", ImVec2(0, maxH), true, ImGuiWindowFlags_NoScrollbar);
#endif
                // centrar dentro del child
                const float padX = std::max(0.0f, (ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2 - drawSz.x) * 0.5f);
                const float padY = std::max(0.0f, (maxH - drawSz.y) * 0.5f);
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + padX, ImGui::GetCursorPosY() + padY));

                // ImGui-SFML: usá la overload que recibe sf::Texture y sf::Vector2f
                ImGui::Image(*sp, sf::Vector2f(drawSz.x, drawSz.y));
                ImGui::EndChild();
            }
            else {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "No se pudo cargar la textura.");
            }
        }

        // Si cambió el path, invalidamos la textura anterior para que se recargue
        if (tex.path != originalPath && !originalPath.empty()) {
            Renderer2D::InvalidateTexture(originalPath);
        }

        ImGui::EndDisabled();
    }
}

void InspectorPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ClampDockedMinWidth(360.0f);

    if (!ctx.selected) {
        ImGui::TextUnformatted("No hay entidad seleccionada.");
        ImGui::End();
        return;
    }

    const auto e = ctx.selected;
    const bool playing = ctx.runtime.playing;
    const bool isPlayer = (ctx.scene && ctx.scene->playerControllers.contains(e.id));

    // ─────────────────────────────
    // Fila: ID a la izquierda y botón "Eliminar" a la derecha (tipo justify-between)
    // ─────────────────────────────
    {
        if (ImGui::BeginTable("tbl_header_row", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 100.f);

            ImGui::TableNextRow();

            // Columna izquierda: texto
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("ID de Entidad: %u", e.id);

            // Columna derecha: botón eliminar (alineado a la derecha de su celda)
            ImGui::TableSetColumnIndex(1);
            ImGui::BeginDisabled(playing || isPlayer);
            bool doDelete = ImGui::Button("Eliminar", ImVec2(-FLT_MIN, 0));
            ImGui::EndDisabled();

            // Tooltips cuando está deshabilitado
            if ((playing || isPlayer) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (playing) ImGui::SetTooltip("Pausa (F5) para editar.");
                else if (isPlayer) ImGui::SetTooltip("No se puede eliminar el Player.");
            }

            // Acción eliminar (solo si estaba habilitado)
            if (doDelete) {
                if (ctx.scene && ctx.selected && !isPlayer) {
                    ctx.scene->DestroyEntity(ctx.selected);
                    ctx.selected = {};
                    ImGui::EndTable();
                    ImGui::End();
                    return;
                }
            }

            ImGui::EndTable();
        }
    }

    if (ctx.scene) {
        // -------------------------
        // Transform
        // -------------------------
        if (auto it = ctx.scene->transforms.find(e.id); it != ctx.scene->transforms.end()) {
            auto& t = it->second;

            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Transform");
            ImGui::PopFont();

            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Posición:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_pos", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("X");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##posx", &t.position.x, 1.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Y");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##posy", &t.position.y, 1.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::EndTable();
            }

            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Escala:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_scale", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("X");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##scalex", &t.scale.x, 0.01f, 0.01f, 10.f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Y");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##scaley", &t.scale.y, 0.01f, 0.01f, 10.f);

                ImGui::EndTable();
            }

            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Rotación:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_rot", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##rot", &t.rotationDeg, 0.5f, -360.f, 360.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("grados");

                ImGui::EndTable();
            }
        }

        // -------------------------
        // Sprite
        // -------------------------
        if (auto it = ctx.scene->sprites.find(e.id); it != ctx.scene->sprites.end()) {
            auto& s = it->second;

            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Sprite");
            ImGui::PopFont();

            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Tamaño:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_size", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Ancho");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##ancho", &s.size.x, 1.f, 1.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Alto");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##alto", &s.size.y, 1.f, 1.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::EndTable();
            }

            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Color:");
            ImGui::PopFont();
            float col[4] = { s.color.r / 255.f, s.color.g / 255.f, s.color.b / 255.f, s.color.a / 255.f };
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::ColorEdit4("##color", col)) {
                auto clamp01 = [](float v) { return std::clamp(v, 0.f, 1.f); };
                s.color = sf::Color(
                    (std::uint8_t)(clamp01(col[0]) * 255.f),
                    (std::uint8_t)(clamp01(col[1]) * 255.f),
                    (std::uint8_t)(clamp01(col[2]) * 255.f),
                    (std::uint8_t)(clamp01(col[3]) * 255.f)
                );
            }
        }

        if (ctx.scene && ctx.selected) {
            DrawTexture2DEditor(*ctx.scene, ctx.selected);
        }

        // -------------------------
        // Player / Jugador
        // -------------------------
        if (ctx.scene->playerControllers.contains(e.id)) {
            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Jugador");
            ImGui::PopFont();

            auto& pc = ctx.scene->playerControllers[e.id];

            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Parámetros de control:");
            ImGui::PopFont();

            if (ImGui::BeginTable("tbl_player", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Speed Velocity");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##movespeed", &pc.moveSpeed, 5.f, 0.f, 5000.f, "%.1f");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Jump Force");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##jumpspeed", &pc.jumpSpeed, 5.f, 0.f, 5000.f, "%.1f");

                ImGui::EndTable();
            }
        }
    }

    ImGui::End();
}
