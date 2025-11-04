#include "InspectorPanel.h"
#include "Runtime/SceneContext.h"
#include "Runtime/EditorContext.h"
#include "ECS/Components.h"
#include "Editor/EditorFonts.h"
#include "Systems/Renderer2D.h"
#include <imgui_stdlib.h>
#include <imgui-SFML.h>
#include <cfloat>

#include <imgui.h>
#include <cstdint>
#include <algorithm>
#include "tinyfiledialogs.h"
#include <filesystem>
#include <unordered_set>

static bool EntityExists(const Scene& scene, EntityID id) {
    return scene.transforms.contains(id) ||
        scene.sprites.contains(id) ||
        scene.colliders.contains(id) ||
        scene.scripts.contains(id) ||
        scene.physics.contains(id) ||
        scene.playerControllers.contains(id);
}

// Recorre todas las “fuentes” de entidades y arma un set de IDs vivos
static std::vector<EntityID> GatherAllEntityIds(const Scene& scene) {
    std::unordered_set<EntityID> set;
    set.reserve(scene.transforms.size() + scene.sprites.size() + scene.colliders.size() +
        scene.scripts.size() + scene.physics.size() + scene.playerControllers.size());

    auto add = [&](auto const& map) { for (auto& kv : map) set.insert(kv.first); };
    add(scene.transforms);
    add(scene.sprites);
    add(scene.colliders);
    add(scene.scripts);
    add(scene.physics);
    add(scene.playerControllers);

    std::vector<EntityID> ids(set.begin(), set.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

static Entity PickFallbackSelection(const Scene& scene, EntityID prev) {
    // 1) Si hay player, priorizarlo
    if (!scene.playerControllers.empty()) {
        return Entity{ scene.playerControllers.begin()->first };
    }

    // 2) Siguiente por ID (o primera)
    auto ids = GatherAllEntityIds(scene);
    if (ids.empty()) return {}; // nada que seleccionar
    auto it = std::upper_bound(ids.begin(), ids.end(), prev);
    if (it != ids.end()) return Entity{ *it };
    return Entity{ ids.front() };
}

static inline void ClampDockedMinWidth(float minW) {
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

static std::string NormalizeToAssetsOrAbsolute(const std::string& chosen)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path chosenAbs = fs::absolute(chosen, ec);
    if (ec) return chosen;

    fs::path assetsRoot = fs::absolute("Assets", ec);
    if (!ec)
    {
        fs::path rel = fs::relative(chosenAbs, assetsRoot, ec);
        bool hasTraversal = false;
        for (const auto& part : rel) if (part == "..") { hasTraversal = true; break; }
        if (!ec && !rel.empty() && !hasTraversal)
            return (fs::path("Assets") / rel).generic_string();
    }
    return chosenAbs.generic_string();
}

static inline bool HasLuaExtension(const std::string& p) {
    auto dot = p.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = p.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return (ext == "lua");
}

static void ApplyNewScriptPath(Script& sc, const std::string& newPath) {
    sc.path = newPath;
    sc.inlineCode.clear();
    sc.loaded = false;
}

static void HandleScriptDropPayload(Script& sc)
{
    const ImGuiPayload* p = nullptr;

    auto takePath = [&](const std::string& s) {
        if (s.empty()) return;
        std::string norm = NormalizeToAssetsOrAbsolute(s);
        if (HasLuaExtension(norm))
            ApplyNewScriptPath(sc, norm);
        };

    if ((p = ImGui::AcceptDragDropPayload("SCRIPT_PATH")) ||
        (p = ImGui::AcceptDragDropPayload("FILE_PATH")))
    {
        std::string dropped((const char*)p->Data, p->DataSize);
        if (!dropped.empty() && dropped.back() == '\0') dropped.pop_back();
        takePath(dropped);
        return;
    }
    if ((p = ImGui::AcceptDragDropPayload("text/uri-list")))
    {
        std::string uris((const char*)p->Data, p->DataSize);
        if (!uris.empty() && uris.back() == '\0') uris.pop_back();
        size_t eol = uris.find_first_of("\r\n");
        std::string first = (eol == std::string::npos) ? uris : uris.substr(0, eol);

        auto strip_prefix = [](const std::string& s, const char* pref) {
            size_t n = std::strlen(pref);
            return s.rfind(pref, 0) == 0 ? s.substr(n) : s;
            };
        first = strip_prefix(first, "file://");
#ifdef _WIN32
        if (first.size() > 3 && first[0] == '/' && std::isalpha((unsigned char)first[1]) && first[2] == ':')
            first.erase(first.begin());
#endif
        takePath(first);
        return;
    }
}

static void DrawTexture2DEditor(Scene& scene, Entity e) {
    if (!e) return;

    ImGui::PushID("Texture2D");
    ImGui::PushID((int)e.id);

    constexpr float kInputRatio = 0.70f;

    bool hasTex = scene.textures.contains(e.id);
    if (ImGui::CollapsingHeader("Texture2D", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(false);

        if (!hasTex) {
            if (ImGui::Button("Agregar componente Texture2D")) {
                scene.textures[e.id] = Texture2D{};
            }
            ImGui::EndDisabled();
            ImGui::PopID();
            ImGui::PopID();
            return;
        }

        auto& tex = scene.textures[e.id];
        std::string originalPath = tex.path;

        {
            const float totalW = ImGui::GetContentRegionAvail().x;
            const float btnW = ImGui::GetFrameHeight();
            const float inputW = std::max(120.0f, totalW * kInputRatio - 1.0f);
            const float spacer = 0.0f;

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacer, ImGui::GetStyle().ItemSpacing.y));
            ImGui::SetNextItemWidth(inputW);
            ImGui::InputTextWithHint("##path", "Assets/...", &tex.path, ImGuiInputTextFlags_ReadOnly);

            ImGui::SameLine(0.0f, spacer);
            bool pick = ImGui::Button("...", ImVec2(btnW, 0));
            ImGui::PopStyleVar();

            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Elegir archivo");

            if (pick) {
                const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga" };
                const char* selected = tinyfd_openFileDialog(
                    "Elegí una textura",
                    "Assets",
                    (int)(sizeof(filters) / sizeof(filters[0])),
                    filters,
                    "Imágenes",
                    0
                );
                if (selected && *selected) {
                    try {
                        namespace fs = std::filesystem;
                        std::error_code ec;
                        fs::path chosenAbs = fs::absolute(selected, ec);
                        fs::path assetsRoot = fs::absolute("Assets", ec);

                        std::string newPath = chosenAbs.generic_string();
                        if (!ec) {
                            fs::path rel = fs::relative(chosenAbs, assetsRoot, ec);
                            bool hasTraversal = false;
                            for (const auto& part : rel) { if (part == "..") { hasTraversal = true; break; } }
                            if (!ec && !rel.empty() && !hasTraversal)
                                newPath = (fs::path("Assets") / rel).generic_string();
                        }

                        std::string oldPath = tex.path;
                        tex.path = newPath;
                        if (!oldPath.empty() && oldPath != newPath)
                            Renderer2D::InvalidateTexture(oldPath);
                        Renderer2D::InvalidateTexture(newPath);
                    }
                    catch (...) {}
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Quitar")) {
                scene.textures.erase(e.id);
                ImGui::EndDisabled();
                ImGui::PopID();
                ImGui::PopID();
                return;
            }
        }

        if (ImGui::Button("Recargar")) { Renderer2D::InvalidateTexture(tex.path); }
        ImGui::SameLine();
        if (ImGui::Button("Limpiar caché")) { Renderer2D::ClearTextureCache(); }

        if (!tex.path.empty()) {
            auto sp = Renderer2D::GetTextureCached(tex.path);
            if (sp && sp->getSize().x > 0 && sp->getSize().y > 0) {
                const auto sz = sp->getSize();
                const float tw = (float)sz.x, th = (float)sz.y;

                const float availW = std::max(32.0f, ImGui::GetContentRegionAvail().x);
                const float maxH = 220.0f;
                const float scale = std::min(availW / tw, maxH / th);
                const ImVec2 drawSz{ tw * scale, th * scale };

                ImGui::Separator();
                ImGui::Text("Preview (%u x %u)", sz.x, sz.y);

                char childId[32];
                std::snprintf(childId, sizeof(childId), "##texprev_%u", e.id);

#if IMGUI_VERSION_NUM >= 19000
                ImGui::BeginChild(childId, ImVec2(0, maxH), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar);
#else
                ImGui::BeginChild(childId, ImVec2(0, maxH), true, ImGuiWindowFlags_NoScrollbar);
#endif
                const float innerW = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;
                const float padX = std::max(0.0f, (innerW - drawSz.x) * 0.5f);
                const float padY = std::max(0.0f, (maxH - drawSz.y) * 0.5f);
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + padX, ImGui::GetCursorPosY() + padY));

                ImGui::Image(*sp, sf::Vector2f(drawSz.x, drawSz.y));
                ImGui::EndChild();
            }
            else {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "No se pudo cargar la textura.");
            }
        }

        if (tex.path != originalPath && !originalPath.empty())
            Renderer2D::InvalidateTexture(originalPath);

        ImGui::EndDisabled();
    }

    ImGui::PopID();
    ImGui::PopID();
}

static void DrawScriptEditor(Scene& scene, Entity e) {
    if (!e) return;

    ImGui::PushID("Script");
    ImGui::PushID((int)e.id);

    const bool hasScript = scene.scripts.contains(e.id);
    if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(false);

        if (!hasScript) {
            if (ImGui::Button("Agregar componente Script")) {
                scene.scripts[e.id] = Script{};
            }
            ImGui::EndDisabled();
            ImGui::PopID(); ImGui::PopID();
            return;
        }

        auto& sc = scene.scripts[e.id];
        const std::string originalPath = sc.path;

        {
            constexpr float kInputRatio = 0.70f;
            const float totalW = ImGui::GetContentRegionAvail().x;
            const float btnW = ImGui::GetFrameHeight();
            const float inputW = std::max(120.0f, totalW * kInputRatio - 1.0f);
            const float spacer = 0.0f;

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacer, ImGui::GetStyle().ItemSpacing.y));
            ImGui::SetNextItemWidth(inputW);

            std::string display = sc.path.empty() ? (sc.inlineCode.empty() ? "" : "(inline)") : sc.path;
            ImGui::InputTextWithHint("##script_path", "Assets/Scripts/...", &display, ImGuiInputTextFlags_ReadOnly);

            if (ImGui::BeginDragDropTarget()) {
                HandleScriptDropPayload(sc);
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine(0.0f, spacer);
            bool pick = ImGui::Button("...", ImVec2(btnW, 0));
            ImGui::PopStyleVar();

            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Elegir archivo .lua");

            if (pick) {
                const char* filters[] = { "*.lua" };
                const char* selected = tinyfd_openFileDialog(
                    "Elegí un script (.lua)",
                    "Assets\\Scripts",
                    (int)(sizeof(filters) / sizeof(filters[0])),
                    filters,
                    "Lua script",
                    0
                );
                if (selected && *selected) {
                    try { ApplyNewScriptPath(sc, NormalizeToAssetsOrAbsolute(selected)); }
                    catch (...) {}
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Quitar")) {
                scene.scripts.erase(e.id);
                ImGui::EndDisabled();
                ImGui::PopID(); ImGui::PopID();
                return;
            }
        }

        if (ImGui::Button("Recargar")) { sc.loaded = false; }
        ImGui::SameLine();
        if (ImGui::Button("Limpiar ruta")) { sc.path.clear(); sc.loaded = false; }

        if (!sc.path.empty())
            ImGui::TextDisabled("Asignado: %s", sc.path.c_str());
        else if (!sc.inlineCode.empty())
            ImGui::TextDisabled("Usando código inline (%zu chars).", sc.inlineCode.size());
        else
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Sin script asignado.");

        ImGui::EndDisabled();
    }

    ImGui::PopID();
    ImGui::PopID();
}

static void DrawColliderSection(Scene& scene, Entity e) {
    if (!e) return;

    ImGui::PushFont(EditorFonts::H1);
    ImGui::SeparatorText("Collider");
    ImGui::PopFont();

    auto& c = scene.colliders[e.id];

    ImGui::Checkbox("Disparador (trigger)", &c.isTrigger);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("No colisión física; solo eventos.");
}

void InspectorPanel::OnGuiRender() {
    auto& scx = SceneContext::Get();
    auto& edx = EditorContext::Get();

    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ClampDockedMinWidth(360.0f);

    if (scx.scene && edx.selected) {
        if (!EntityExists(*scx.scene, edx.selected.id)) {
            edx.selected = PickFallbackSelection(*scx.scene, edx.selected.id);
        }
    }

    if (!edx.selected) {
        ImGui::TextUnformatted("No hay entidad seleccionada.");
        ImGui::End();
        return;
    }

    const auto e = edx.selected;
    const bool playing = edx.runtime.playing;
    const bool isPlayer = (scx.scene && scx.scene->playerControllers.contains(e.id));

    // Header: ID + Eliminar
    {
        if (ImGui::BeginTable("tbl_header_row", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 100.f);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("ID de Entidad: %u", e.id);

            ImGui::TableSetColumnIndex(1);
            ImGui::BeginDisabled(playing || isPlayer);
            bool doDelete = ImGui::Button("Eliminar", ImVec2(-FLT_MIN, 0));
            ImGui::EndDisabled();

            if ((playing || isPlayer) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (playing) ImGui::SetTooltip("Pausa (F5) para editar.");
                else if (isPlayer) ImGui::SetTooltip("No se puede eliminar el Player.");
            }

            if (doDelete) {
                if (scx.scene && edx.selected && !isPlayer) {
                    const EntityID old = edx.selected.id;
                    scx.scene->DestroyEntity(edx.selected);
                    edx.selected = PickFallbackSelection(*scx.scene, old);
                    ImGui::EndTable();
                    ImGui::End();
                    return;
                }
            }

            ImGui::EndTable();
        }
    }

    if (scx.scene) {
        // Transform
        if (auto it = scx.scene->transforms.find(e.id); it != scx.scene->transforms.end()) {
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

        if (auto it = scx.scene->sprites.find(e.id); it != scx.scene->sprites.end()) {
            auto& s = it->second;

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

        if (scx.scene && edx.selected) {
            DrawColliderSection(*scx.scene, edx.selected);
            DrawTexture2DEditor(*scx.scene, edx.selected);
            DrawScriptEditor(*scx.scene, edx.selected);
        }

        // Jugador
        if (scx.scene->playerControllers.contains(e.id)) {
            ImGui::PushFont(EditorFonts::H1);
            ImGui::SeparatorText("Jugador");
            ImGui::PopFont();

            auto& pc = scx.scene->playerControllers[e.id];

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
