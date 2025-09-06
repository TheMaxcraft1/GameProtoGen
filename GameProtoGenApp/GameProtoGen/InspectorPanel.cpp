#include "Headers/InspectorPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Components.h"
#include "Headers/EditorFonts.h"

#include <imgui.h>
#include <cstdint>
#include <algorithm>

static inline void ClampDockedMinWidth(float minW) {
    // En dock, los constraints no se respetan: forzamos tamaño si se pasa.
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

void InspectorPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ClampDockedMinWidth(360.0f);  // <--- mínimo real

    if (!ctx.selected) {
        ImGui::TextUnformatted("No hay entidad seleccionada.");
        ImGui::End();
        return;
    }

    const auto e = ctx.selected;
    ImGui::Text("ID de Entidad: %u", e.id);

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

        // -------------------------
        // Player / Jugador (detectado por presencia de PlayerController)
        // -------------------------
        {
            // Si es jugador, exponer parámetros editables
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

                    // Speed Velocity (moveSpeed)
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Speed Velocity");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("##movespeed", &pc.moveSpeed, 5.f, 0.f, 5000.f, "%.1f");

                    // Jump Force (jumpSpeed). Nota: en tu física Y+ es hacia abajo,
                    // el salto aplica -jumpSpeed, por eso aquí se edita como positivo.
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Jump Force");
                    ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::DragFloat("##jumpspeed", &pc.jumpSpeed, 5.f, 0.f, 5000.f, "%.1f");

                    ImGui::EndTable();
                }
            }
        }

    }

    ImGui::End();
}
