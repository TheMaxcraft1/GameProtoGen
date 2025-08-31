#include "Headers/InspectorPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Components.h"
#include "Headers/EditorFonts.h"   // <-- para H1/H2

#include <imgui.h>
#include <cstdint>
#include <algorithm> // std::clamp

void InspectorPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(360.0f, 0.0f));
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (!ctx.selected) {

        ImGui::TextUnformatted("No hay entidad seleccionada.");
        ImGui::End();
        return;
    }

    const auto e = ctx.selected;

    ImGui::Text("ID de Entidad: %u", e.id);

    if (ctx.scene) {
        // -------------------------
        // Transform (H2)
        // -------------------------
        if (auto it = ctx.scene->transforms.find(e.id); it != ctx.scene->transforms.end()) {
            auto& t = it->second;

            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Transform");
            ImGui::PopFont();

            // Posición
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Posición:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_pos", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                // X
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("X");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##posx", &t.position.x, 1.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                // Y
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Y");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##posy", &t.position.y, 1.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::EndTable();
            }

            // Escala
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Escala:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_scale", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                // X
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("X");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##scalex", &t.scale.x, 0.01f, 0.01f, 10.f);

                // Y
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Y");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##scaley", &t.scale.y, 0.01f, 0.01f, 10.f);

                ImGui::EndTable();
            }

            // Rotación
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Rotación:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_rot", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##rot", &t.rotationDeg, 0.5f, -360.f, 360.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("grados");

                ImGui::EndTable();
            }
        }

        // -------------------------
        // Sprite (H2)
        // -------------------------
        if (auto it = ctx.scene->sprites.find(e.id); it != ctx.scene->sprites.end()) {
            auto& s = it->second;

            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Sprite");
            ImGui::PopFont();

            // Tamaño
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Tamaño:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_size", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                // Ancho
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Ancho");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##ancho", &s.size.x, 1.f, 1.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                // Alto
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Alto");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##alto", &s.size.y, 1.f, 1.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::EndTable();
            }

            // Color
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Color:");
            ImGui::PopFont();
            float col[4] = { s.color.r / 255.f, s.color.g / 255.f, s.color.b / 255.f, s.color.a / 255.f };
            ImGui::SetNextItemWidth(-FLT_MIN); // que llene la columna
            if (ImGui::ColorEdit4("##color", col)) {
                auto clamp01 = [](float v) { return std::clamp(v, 0.f, 1.f); };
                s.color = sf::Color(
                    static_cast<std::uint8_t>(clamp01(col[0]) * 255.f),
                    static_cast<std::uint8_t>(clamp01(col[1]) * 255.f),
                    static_cast<std::uint8_t>(clamp01(col[2]) * 255.f),
                    static_cast<std::uint8_t>(clamp01(col[3]) * 255.f)
                );
            }
        }

        // -------------------------
        // Collider (H2) - opcional
        // -------------------------
        if (auto it = ctx.scene->colliders.find(e.id); it != ctx.scene->colliders.end()) {
            auto& c = it->second;

            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Collider (AABB)");
            ImGui::PopFont();

            // Half-extents
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Half-extents:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_he", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                // X
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("X");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##heX", &c.halfExtents.x, 1.f, 0.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                // Y
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Y");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##heY", &c.halfExtents.y, 1.f, 0.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::EndTable();
            }

            // Offset
            ImGui::PushFont(EditorFonts::H2);
            ImGui::TextUnformatted("Offset:");
            ImGui::PopFont();
            if (ImGui::BeginTable("tbl_off", 3, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("inp", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("unit", ImGuiTableColumnFlags_WidthFixed);

                // X
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("X");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##offX", &c.offset.x, 1.f, -4096.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                // Y
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Y");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::DragFloat("##offY", &c.offset.y, 1.f, -4096.f, 4096.f);
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("px");

                ImGui::EndTable();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

}
