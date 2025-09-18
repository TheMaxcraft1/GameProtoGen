// GameProtoGen/ChatPanel.cpp
#include "ChatPanel.h"
#include "Runtime/SceneContext.h"
#include "ECS/Scene.h"
#include "ECS/SceneSerializer.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>


using json = nlohmann::json;

// ------------------------- helpers de color -------------------------
static inline bool is_hex_digit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}
static sf::Color ColorFromHexString(const std::string& in) {
    std::string s = in;
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    if (!(s.size() == 6 || s.size() == 8)) return sf::Color(60, 60, 70, 255);
    if (!std::all_of(s.begin(), s.end(), is_hex_digit)) return sf::Color(60, 60, 70, 255);
    auto hexByte = [&](size_t pos) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::stoul(s.substr(pos, 2), nullptr, 16));
        };
    std::uint8_t r = hexByte(0);
    std::uint8_t g = hexByte(2);
    std::uint8_t b = hexByte(4);
    std::uint8_t a = (s.size() == 8) ? hexByte(6) : 255;
    return sf::Color(r, g, b, a);
}
static sf::Color TryParseColor(const json& j, const sf::Color& fallback = sf::Color(60, 60, 70, 255)) {
    if (j.contains("colorHex") && j["colorHex"].is_string()) return ColorFromHexString(j["colorHex"].get<std::string>());
    if (j.contains("color") && j["color"].is_object()) {
        const auto& c = j["color"];
        auto clamp255 = [](int v) { return std::clamp(v, 0, 255); };
        int r = c.value("r", 255), g = c.value("g", 255), b = c.value("b", 255), a = c.value("a", 255);
        return sf::Color((std::uint8_t)clamp255(r), (std::uint8_t)clamp255(g), (std::uint8_t)clamp255(b), (std::uint8_t)clamp255(a));
    }
    return fallback;
}
// ------------------------- helper entity id -------------------------
static uint32_t GetEntityId(const nlohmann::json& j) {
    if (!j.contains("entity") || j["entity"].is_null()) return 0;
    const auto& v = j["entity"];
    if (v.is_number_unsigned()) return v.get<uint32_t>();
    if (v.is_number_integer()) { int vi = v.get<int>(); return vi > 0 ? static_cast<uint32_t>(vi) : 0; }
    if (v.is_number_float()) { double vf = v.get<double>(); return vf >= 0.0 ? static_cast<uint32_t>(std::round(vf)) : 0; }
    if (v.is_string()) { try { return static_cast<uint32_t>(std::stoul(v.get<std::string>())); } catch (...) { return 0; } }
    return 0;
}

// ========================= ChatPanel =========================

ChatPanel::ChatPanel(std::shared_ptr<ApiClient> client)
    : m_Client(std::move(client)) {
    // Mensaje inicial opcional
    m_History.push_back({ Role::Assistant, "Decime qué querés hacer (ej: \"crear plataforma\" \"mover jugador\").", false });
}

void ChatPanel::OnGuiRender() {
    ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    // ----- HISTORIAL (scrollable) -----
    const float footerH = ImGui::GetFrameHeightWithSpacing() + 10.0f;
#if IMGUI_VERSION_NUM >= 19000
    ImGuiChildFlags cflags = ImGuiChildFlags_None;
    ImGuiWindowFlags wflags = ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("##history", ImVec2(0, -footerH), cflags, wflags);
#else
    ImGui::BeginChild("##history", ImVec2(0, -footerH), false, ImGuiWindowFlags_HorizontalScrollbar);
#endif

    RenderHistory();

    if (m_RequestScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_RequestScrollToBottom = false;
    }
    ImGui::EndChild();

    // ----- INPUT -----
    ImGui::Separator();
    bool send = false;

    ImGui::PushItemWidth(-100.0f);
    if (ImGui::InputText("##prompt", &m_Input, ImGuiInputTextFlags_EnterReturnsTrue)) send = true;
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::BeginDisabled(m_Busy || m_Input.empty());
    if (ImGui::Button(m_Busy ? "Enviando..." : "Enviar")) send = true;
    ImGui::EndDisabled();

    if (send && !m_Busy && !m_Input.empty()) {
        SendCurrentPrompt();
    }

    // ----- Pool de la futura (si hay) -----
    if (m_Busy && m_Fut.valid()) {
        using namespace std::chrono_literals;
        if (m_Fut.wait_for(0ms) == std::future_status::ready) {
            auto res = m_Fut.get();
            m_Busy = false;
            if (m_TypingIndex >= 0 && m_TypingIndex < (int)m_History.size()) {
                auto& typingBubble = m_History[m_TypingIndex];
                typingBubble.typing = false;

                if (!res.ok()) {
                    typingBubble.text = std::string("Error: ") + res.error;
                }
                else {
                    const nlohmann::json& root = *res.data;
                    const std::string kind = root.value("kind", "");

                    if (kind == "ops") {
                        OpCounts c = ApplyOpsFromJson(root);
                        typingBubble.text = "Listo: "
                            + std::to_string(c.created) + " creadas, "
                            + std::to_string(c.modified) + " modificadas, "
                            + std::to_string(c.removed) + " eliminadas.";
                    }
                    else if (kind == "text") {
                        typingBubble.text = root.value("message", "");
                    }
                    else if (kind == "bundle") {
                        OpCounts total{};
                        std::vector<std::string> texts;

                        if (root.contains("items") && root["items"].is_array()) {
                            for (const auto& it : root["items"]) {
                                const std::string ik = it.value("kind", "");
                                if (ik == "ops") {
                                    total.created += ApplyOpsFromJson(it).created;
                                    total.modified += ApplyOpsFromJson(it).modified;
                                    total.removed += ApplyOpsFromJson(it).removed;
                                }
                                else if (ik == "text") {
                                    texts.push_back(it.value("message", ""));
                                }
                            }
                        }

                        // Si hubo cambios, resumimos en la burbuja "typing".
                        if (total.created || total.modified || total.removed) {
                            typingBubble.text = "Listo: "
                                + std::to_string(total.created) + " creadas, "
                                + std::to_string(total.modified) + " modificadas, "
                                + std::to_string(total.removed) + " eliminadas.";
                            // Y si además hay textos, los agregamos como mensajes aparte
                            for (auto& t : texts) {
                                if (!t.empty())
                                    m_History.push_back({ Role::Assistant, t, false });
                            }
                        }
                        else {
                            // Si NO hubo ops, usamos el primer texto como respuesta principal
                            if (!texts.empty()) typingBubble.text = texts.front();
                            // y el resto, como burbujas adicionales
                            for (size_t i = 1; i < texts.size(); ++i) {
                                if (!texts[i].empty())
                                    m_History.push_back({ Role::Assistant, texts[i], false });
                            }
                            if (texts.empty()) {
                                typingBubble.text = "No hubo cambios ni mensajes.";
                            }
                        }
                    }
                    else {
                        // Compat / fallback
                        // - Si el backend devolvió un JSON legacy con "ops" sin "kind"
                        if (root.contains("ops"))
                            typingBubble.text = "Listo: "
                            + std::to_string(ApplyOpsFromJson(root).created) + " creadas, "
                            + std::to_string(ApplyOpsFromJson(root).modified) + " modificadas, "
                            + std::to_string(ApplyOpsFromJson(root).removed) + " eliminadas.";
                        else
                            typingBubble.text = root.dump(); // mostrar algo útil
                    }
                }
            }
            m_TypingIndex = -1;
            m_RequestScrollToBottom = true;
        }
    }

    ImGui::End();
}

// Enviar: agrega burbuja del usuario + burbuja de "..." del asistente y dispara la request
void ChatPanel::SendCurrentPrompt() {
    // 1) Usuario
    m_History.push_back({ Role::User, m_Input, false });

    // 2) Burbuja “tipeando” del asistente
    m_History.push_back({ Role::Assistant, "", true });
    m_TypingIndex = (int)m_History.size() - 1;
    m_RequestScrollToBottom = true;

    // 3) Disparar request
    m_Busy = true;
    auto& ctx = SceneContext::Get();
    nlohmann::json sceneJson = ctx.scene ? SceneSerializer::Dump(*ctx.scene)
        : nlohmann::json::object();
    m_Fut = m_Client->SendCommandAsync(m_Input, sceneJson);

    // 4) limpiar input
    m_Input.clear();
}

void ChatPanel::RenderHistory() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 regionMin = ImGui::GetWindowPos();
    const ImVec2 regionAvail = ImGui::GetContentRegionAvail();
    const float maxWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

    const float bubbleW = std::min(520.0f, maxWidth * 0.9f);
    const float pad = 8.0f;
    const float spacingY = 6.0f;

    for (size_t i = 0; i < m_History.size(); ++i) {
        const auto& msg = m_History[i];
        const bool isUser = (msg.role == Role::User);

        // Colores
        ImVec4 bg = isUser ? ImVec4(0.16f, 0.45f, 0.92f, 1.0f)   // azul user
            : ImVec4(0.92f, 0.92f, 0.95f, 1.0f);  // gris claro assistant
        ImVec4 fg = isUser ? ImVec4(1, 1, 1, 1) : ImVec4(0.10f, 0.10f, 0.12f, 1.0f);

        // Alineación: user a la derecha, assistant a la izquierda
        float cursorY = ImGui::GetCursorPosY();
        float startX = isUser ? (ImGui::GetWindowContentRegionMax().x - bubbleW)
            : (ImGui::GetWindowContentRegionMin().x);
        // Trasladar a coords locales
        startX += ImGui::GetWindowPos().x;

        // Set cursor
        ImGui::SetCursorScreenPos(ImVec2(startX, regionMin.y + cursorY));

        // Child con fondo (altura auto)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

        std::string child_id = "##msg" + std::to_string(i);

#if IMGUI_VERSION_NUM >= 19000
        ImGuiChildFlags cflags = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border;
        ImGuiWindowFlags wflags = ImGuiWindowFlags_NoScrollbar;
        ImGui::BeginChild(child_id.c_str(), ImVec2(bubbleW, 0.0f), cflags, wflags);
#else
        ImGui::BeginChild(child_id.c_str(), ImVec2(bubbleW, 0.0f), /*border*/true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
#endif

        ImGui::PushStyleColor(ImGuiCol_Text, fg);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad);

        // Contenido: texto normal o “...”
        if (!msg.typing) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + (bubbleW - 2 * pad));
            ImGui::TextUnformatted(msg.text.c_str());
            ImGui::PopTextWrapPos();
        }
        else {
            // Animación de puntos: 1..3
            int dots = 1 + (int)(ImGui::GetTime() * 3.0) % 3;
            static const char* DOTS[4] = { "", ".", "..", "..." };
            ImGui::TextUnformatted(DOTS[dots]);
        }

        // padding inferior
        ImGui::Dummy(ImVec2(0, pad));

        ImGui::PopStyleColor(); // text
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // Espacio entre mensajes
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacingY);
    }
}

// Aplica ops + devuelve conteo (creadas / modificadas / eliminadas)
ChatPanel::OpCounts ChatPanel::ApplyOpsFromJson(const json& resp) {
    OpCounts counts;
    if (!resp.contains("ops") || !resp["ops"].is_array()) return counts;

    auto& ctx = SceneContext::Get();
    if (!ctx.scene) ctx.scene = std::make_shared<Scene>();

    // Conjuntos para coalescer por entidad
    std::unordered_set<uint32_t> created, modified, removed;

    for (auto& op : resp["ops"]) {
        std::string type = op.value("op", "");

        if (type == "spawn_box") {
            // ---- Crear entidad ----
            auto pos = op["pos"];
            auto size = op["size"];
            sf::Color col = TryParseColor(op, sf::Color(60, 60, 70, 255));

            Entity e = ctx.scene->CreateEntity();
            ctx.scene->transforms[e.id] = Transform{ {pos[0].get<float>(), pos[1].get<float>()}, {1.f,1.f}, 0.f };
            ctx.scene->sprites[e.id] = Sprite{ {size[0].get<float>(), size[1].get<float>()}, col };
            ctx.scene->colliders[e.id] = Collider{ {size[0].get<float>() * 0.5f, size[1].get<float>() * 0.5f}, {0.f,0.f} };

            created.insert(e.id);
        }
        else if (type == "set_transform") {
            // ---- Modificar transform / (size) ----
            uint32_t id = GetEntityId(op);
            if (id && ctx.scene->transforms.contains(id)) {
                auto& t = ctx.scene->transforms[id];

                if (op.contains("position") && !op["position"].is_null()) {
                    auto p = op["position"];
                    t.position = { p[0].get<float>(), p[1].get<float>() };
                }

                if (op.contains("size") && !op["size"].is_null() && ctx.scene->sprites.contains(id)) {
                    auto s = op["size"];
                    float w = std::max(1.f, s[0].get<float>());
                    float h = std::max(1.f, s[1].get<float>());
                    ctx.scene->sprites[id].size = { w, h };
                }

                if (op.contains("scale") && !op["scale"].is_null()) {
                    auto s = op["scale"];
                    float sx = s[0].get<float>(), sy = s[1].get<float>();
                    bool looksLikeSize = (std::fabs(sx) > 10.f) || (std::fabs(sy) > 10.f);
                    if (looksLikeSize && ctx.scene->sprites.contains(id)) {
                        float w = std::max(1.f, sx), h = std::max(1.f, sy);
                        ctx.scene->sprites[id].size = { w, h };
                        t.scale = { 1.f, 1.f };
                    }
                    else {
                        auto clampScale = [](float v) { return std::clamp(v, 0.05f, 10.f); };
                        t.scale = { clampScale(sx), clampScale(sy) };
                    }
                }

                if (op.contains("rotation") && !op["rotation"].is_null()) {
                    t.rotationDeg = op["rotation"].get<float>();
                }

                if (created.count(id) == 0) modified.insert(id); // no contar como “modificada” si fue creada recién
            }
        }
        else if (type == "set_component") {
            // ---- Modificar componentes (Sprite, etc.) ----
            uint32_t id = GetEntityId(op);
            std::string comp = op.value("component", "");

            if (id && comp == "Sprite" && ctx.scene->sprites.contains(id) && op.contains("value")) {
                auto& sp = ctx.scene->sprites[id];
                const auto& value = op["value"];

                if (value.contains("colorHex") && value["colorHex"].is_string()) {
                    sp.color = ColorFromHexString(value["colorHex"].get<std::string>());
                }
                else if (value.contains("color") && value["color"].is_object()) {
                    auto c = value["color"];
                    auto clamp255 = [](int v) { return std::clamp(v, 0, 255); };
                    sp.color = sf::Color(
                        (std::uint8_t)clamp255(c.value("r", 255)),
                        (std::uint8_t)clamp255(c.value("g", 255)),
                        (std::uint8_t)clamp255(c.value("b", 255)),
                        (std::uint8_t)clamp255(c.value("a", 255))
                    );
                }

                if (value.contains("size") && value["size"].is_array() && value["size"].size() >= 2) {
                    float w = std::max(1.f, value["size"][0].get<float>());
                    float h = std::max(1.f, value["size"][1].get<float>());
                    sp.size = { w, h };
                }

                if (created.count(id) == 0) modified.insert(id);
            }
        }
        else if (type == "remove_entity") {
            // ---- Eliminar entidad (coalescer con casos “creada/ modificada” en el mismo batch) ----
            uint32_t id = GetEntityId(op);
            if (id && ctx.scene) {
                ctx.scene->DestroyEntity(Entity{ id });
                if (ctx.selected.id == id) ctx.selected = {};

                // Si la creamos en este mismo batch, dejarla solo como “eliminada”
                created.erase(id);
                modified.erase(id);
                removed.insert(id);
            }
        }
    }

    counts.created = (int)created.size();
    counts.modified = (int)modified.size();
    counts.removed = (int)removed.size();
    return counts;
}
