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
#include <SFML/Graphics.hpp>
#include <imgui-SFML.h>
#include <fstream>
#include <vector>
#include <filesystem>
#include "ViewportPanel.h"
#include "Systems/Renderer2D.h"

using json = nlohmann::json;

static bool SaveTextToFile(const std::string& outPath, const std::string& text) {
    try {
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;
        ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
        return ofs.good();
    }
    catch (...) { return false; }
}

// ---------------- base64 decode helper (con soporte de padding '=') ----------------
static std::vector<unsigned char> Base64Decode(const std::string& input) {
    // -1: inválido/ignorar, -2: '=' padding
    static const int8_t DT[256] = {
        /* 0..15  */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* 16..31 */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        /* 32..47 */ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62, -1,-1,-1,63,
        /* 48..63 */ 52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
        /* 64..79 */ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        /* 80..95 */ 15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        /* 96..111*/ -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        /*112..127*/ 41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        /*128..255*/ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
                     -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    std::vector<unsigned char> out;
    out.reserve(input.size() * 3 / 4);
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        int d = DT[c];
        if (d == -1) continue;   // ignora espacios/chars inválidos
        if (d == -2) break;      // '=' padding -> fin
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

static bool SaveBase64ToFile(const std::string& base64, const std::string& outPath) {
    try {
        auto bytes = Base64Decode(base64);
        if (bytes.empty()) return false;
        std::filesystem::create_directories(std::filesystem::path(outPath).parent_path());
        std::ofstream ofs(outPath, std::ios::binary);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return ofs.good();
    }
    catch (...) {
        return false;
    }
}

// --- Helpers locales para dibujar iconos como botón (misma técnica que usás en otro componente) ---
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

    // Zona clickeable
    ImGui::InvisibleButton(id, ImVec2(sz.x, sz.y));
    bool pressed = ImGui::IsItemClicked();
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    // Dibujo del icono
    ImGui::SetCursorScreenPos(posStart);
    ImGui::Image(tex, sz);
    ImGui::SetCursorScreenPos(ImVec2(posStart.x + sz.x, posStart.y));

    // Feedback visual (borde)
    if (hovered || active || toggled) {
        ImU32 col = ImGui::GetColorU32(active ? ImGuiCol_ButtonActive
            : (toggled ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered));
        dl->AddRect(posStart, ImVec2(posStart.x + sz.x, posStart.y + sz.y), col, 6.0f, 0, 2.0f);
    }
    if (hovered && tooltip && *tooltip) ImGui::SetTooltip("%s", tooltip);
    return pressed;
}

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
                    else if (kind == "asset") {
                        // NUEVO: guardar la imagen y mostrar resultado
                        const std::string fileName = root.value("fileName", "asset.png");
                        const std::string data = root.value("data", "");
                        std::string safeName = fileName;
                        for (char& ch : safeName) {
                            if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
                                ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ch = '_';
                        }
                        const std::string outPath = "Assets/Generated/" + safeName;
                        if (!data.empty() && SaveBase64ToFile(data, outPath)) {
                            typingBubble.text = std::string("Imagen guardada en:\n") + std::filesystem::absolute(outPath).string();
                            ViewportPanel::AppendLog(std::string("[ASSET] Guardado: ") + outPath);
                        }
                        else {
                            typingBubble.text = "No pude guardar la imagen (payload incompleto o base64 inválido).";
                            ViewportPanel::AppendLog("[ASSET] ERROR al guardar la imagen.");
                        }
                    }
                    else if (kind == "script") {
                        const std::string fileName = root.value("fileName", "script.lua");
                        const std::string code = root.value("code", "");
                        // sanitizar nombre
                        std::string safeName = fileName;
                        for (char& ch : safeName) {
                            if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ch = '_';
                        }
                        const std::string outPath = "Assets/Scripts/" + safeName;

                        if (!code.empty() && SaveTextToFile(outPath, code)) {
                            // elegir target: seleccionado -> player -> crear entidad nueva
                            auto& ctx = SceneContext::Get();
                            EntityID target = 0;
                            if (ctx.selected) target = ctx.selected.id;
                            else if (ctx.scene && !ctx.scene->playerControllers.empty())
                                target = ctx.scene->playerControllers.begin()->first;

                            if (!target && ctx.scene) {
                                // creamos una “caja” para alojar el script
                                Entity e = ctx.scene->CreateEntity();
                                ctx.scene->transforms[e.id] = Transform{ ctx.cameraCenter, {1.f,1.f}, 0.f };
                                ctx.scene->sprites[e.id] = Sprite{ {64.f,64.f}, sf::Color(255,255,255,255) };
                                ctx.scene->colliders[e.id] = Collider{ {32.f,32.f}, {0.f,0.f} };
                                target = e.id;
                                ctx.selected = e;
                            }

                            if (ctx.scene && target) {
                                auto& sc = ctx.scene->scripts[target];
                                sc.path = outPath;
                                sc.inlineCode.clear();
                                sc.loaded = false; // fuerza re-run en próximo Play
                            }

                            typingBubble.text = std::string("Script guardado en:\n") + std::filesystem::absolute(outPath).string()
                                + (target ? ("\nAsignado a entidad id=" + std::to_string(target)) : "");
                            ViewportPanel::AppendLog(std::string("[SCRIPT] Guardado: ") + outPath
                                + (target ? ("  -> id=" + std::to_string(target)) : ""));
                        }
                        else {
                            typingBubble.text = "No pude guardar el script (payload vacío o error de escritura).";
                            ViewportPanel::AppendLog("[SCRIPT] ERROR al guardar script.");
                        }
                    }
                    else if (kind == "bundle") {
                        OpCounts total{};
                        std::vector<std::string> texts;

                        // 1) PRIMER PASO: guardar assets
                        if (root.contains("items") && root["items"].is_array()) {
                            for (const auto& it : root["items"]) {
                                const std::string ik = it.value("kind", "");
                                if (ik == "asset") {
                                    const std::string fileName = it.value("fileName", "asset.png");
                                    const std::string data = it.value("data", "");
                                    std::string safeName = fileName;
                                    for (char& ch : safeName) {
                                        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
                                            ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ch = '_';
                                    }
                                    const std::string outPath = "Assets/Generated/" + safeName;
                                    if (!data.empty() && SaveBase64ToFile(data, outPath)) {
                                        ViewportPanel::AppendLog(std::string("[ASSET] Guardado: ") + outPath);
                                    }
                                    else {
                                        ViewportPanel::AppendLog("[ASSET] ERROR al guardar: " + outPath);
                                    }
                                }
                                if (ik == "script") {
                                    const std::string fileName = it.value("fileName", "script.lua");
                                    const std::string code = it.value("code", "");
                                    std::string safeName = fileName;
                                    for (char& ch : safeName) {
                                        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ch = '_';
                                    }
                                    const std::string outPath = "Assets/Scripts/" + safeName;
                                    if (!code.empty() && SaveTextToFile(outPath, code)) {
                                        ViewportPanel::AppendLog(std::string("[SCRIPT] Guardado: ") + outPath);
                                        // Asignación automática (misma regla que arriba)
                                        auto& ctx = SceneContext::Get();
                                        EntityID target = 0;
                                        if (ctx.selected) target = ctx.selected.id;
                                        else if (ctx.scene && !ctx.scene->playerControllers.empty())
                                            target = ctx.scene->playerControllers.begin()->first;
                                        if (!target && ctx.scene) {
                                            Entity e = ctx.scene->CreateEntity();
                                            ctx.scene->transforms[e.id] = Transform{ ctx.cameraCenter, {1.f,1.f}, 0.f };
                                            ctx.scene->sprites[e.id] = Sprite{ {64.f,64.f}, sf::Color(255,255,255,255) };
                                            ctx.scene->colliders[e.id] = Collider{ {32.f,32.f}, {0.f,0.f} };
                                            target = e.id;
                                            ctx.selected = e;
                                        }
                                        if (ctx.scene && target) {
                                            auto& sc = ctx.scene->scripts[target];
                                            sc.path = outPath;
                                            sc.inlineCode.clear();
                                            sc.loaded = false;
                                        }
                                    }
                                    else {
                                        ViewportPanel::AppendLog("[SCRIPT] ERROR al guardar (bundle).");
                                    }
                                }
                            }
                        }

                        // 2) SEGUNDO PASO: aplicar ops + colectar textos
                        if (root.contains("items") && root["items"].is_array()) {
                            for (const auto& it : root["items"]) {
                                const std::string ik = it.value("kind", "");
                                if (ik == "ops") {
                                    OpCounts c = ApplyOpsFromJson(it);
                                    total.created += c.created;
                                    total.modified += c.modified;
                                    total.removed += c.removed;
                                }
                                else if (ik == "text") {
                                    texts.push_back(it.value("message", ""));
                                }
                            }
                        }

                        // 3) Mensaje en la burbuja "typing"
                        if (total.created || total.modified || total.removed) {
                            typingBubble.text = "Listo: "
                                + std::to_string(total.created) + " creadas, "
                                + std::to_string(total.modified) + " modificadas, "
                                + std::to_string(total.removed) + " eliminadas.";
                            for (auto& t : texts) if (!t.empty()) m_History.push_back({ Role::Assistant, t, false });
                        }
                        else {
                            if (!texts.empty()) typingBubble.text = texts.front();
                            for (size_t i = 1; i < texts.size(); ++i)
                                if (!texts[i].empty()) m_History.push_back({ Role::Assistant, texts[i], false });
                            if (texts.empty()) typingBubble.text = "No hubo cambios ni mensajes.";
                        }
                    }
                    else {
                        if (root.contains("ops")) {
                            OpCounts c = ApplyOpsFromJson(root);
                            typingBubble.text = "Listo: "
                                + std::to_string(c.created) + " creadas, "
                                + std::to_string(c.modified) + " modificadas, "
                                + std::to_string(c.removed) + " eliminadas.";
                        }
                        else {
                            typingBubble.text = root.dump();
                        }
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

    const float availW = ImGui::GetContentRegionAvail().x;        
    const float bubbleW = std::min(520.0f, availW * 0.9f);
    const float pad = 8.0f;
    const float spacingY = 6.0f;

    // Carga perezosa del icono de copiar (igual que antes)
    static sf::Texture s_CopyTex;
    static bool s_CopyOk = false;
    static bool s_CopyInit = false;
    if (!s_CopyInit) {
        s_CopyInit = true;
        s_CopyOk = s_CopyTex.loadFromFile("Assets/Icons/copy.png");
        s_CopyTex.setSmooth(true);
    }

    static std::vector<double> s_CopyUntil;
    if (s_CopyUntil.size() < m_History.size())
        s_CopyUntil.resize(m_History.size(), -1.0);

    for (size_t i = 0; i < m_History.size(); ++i) {
        auto& msg = m_History[i];
        const bool isUser = (msg.role == Role::User);

        // Colores
        ImVec4 bg = isUser ? ImVec4(0.16f, 0.45f, 0.92f, 1.0f)
            : ImVec4(0.92f, 0.92f, 0.95f, 1.0f);
        ImVec4 fg = isUser ? ImVec4(1, 1, 1, 1)
            : ImVec4(0.10f, 0.10f, 0.12f, 1.0f);

        float startX = isUser ? (availW - bubbleW) : 0.0f;
        ImGui::SetCursorPosX(startX);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        std::string child_id = "##msg" + std::to_string(i);
#if IMGUI_VERSION_NUM >= 19000
        ImGuiChildFlags cflags = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border;
        ImGuiWindowFlags wflags = ImGuiWindowFlags_NoScrollbar;
        ImGui::BeginChild(child_id.c_str(), ImVec2(bubbleW, 0.0f), cflags, wflags);
#else
        ImGui::BeginChild(child_id.c_str(), ImVec2(bubbleW, 0.0f), true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
#endif

        ImGui::PushStyleColor(ImGuiCol_Text, fg);

        // Padding interno manual
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + pad);

        const float innerW = bubbleW - 2.0f * pad;

        if (!msg.typing) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + innerW);
            ImGui::TextUnformatted(msg.text.c_str());
            ImGui::PopTextWrapPos();

            if (!isUser) {
                ImGui::Dummy(ImVec2(0, pad * 0.5f));

                float x0 = ImGui::GetCursorPosX();
                float y0 = ImGui::GetCursorPosY();

                float iconH = 18.0f;
                ImVec2 iconSz = s_CopyOk ? FitIconHeight(s_CopyTex, iconH) : ImVec2(40.0f, iconH);

                // ✅ Posicionar el botón a la derecha en local
                ImGui::SetCursorPosX(x0 + innerW - iconSz.x);

                const double now = (double)ImGui::GetTime();
                const bool showCopied = (s_CopyUntil[i] >= 0.0 && now < s_CopyUntil[i]);
                const char* tooltip = showCopied ? "Copiado" : "Copiar";

                bool clicked = false;
                if (s_CopyOk) {
                    // Este helper usa SetCursorScreenPos internamente, pero ya dentro de este child.
                    clicked = IconButtonFromTexture(
                        (std::string("##cpy_") + std::to_string(i)).c_str(),
                        s_CopyTex,
                        iconH,
                        tooltip
                    );
                }
                else {
                    clicked = ImGui::SmallButton((std::string("Copiar##cpy_") + std::to_string(i)).c_str());
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
                }

                if (clicked) {
                    ImGui::SetClipboardText(msg.text.c_str());
                    s_CopyUntil[i] = now + 1.5;
                }

                // Bajar el cursor debajo del icono
                ImGui::SetCursorPosY(y0 + iconSz.y + pad * 0.5f);
            }
        }
        else {
            int dots = 1 + (int)(ImGui::GetTime() * 3.0) % 3;
            static const char* DOTS[4] = { "", ".", "..", "..." };
            ImGui::TextUnformatted(DOTS[dots]);
            ImGui::Dummy(ImVec2(0, pad * 0.5f));
        }

        ImGui::Dummy(ImVec2(0, pad)); // padding inferior

        ImGui::PopStyleColor(); // text
        ImGui::EndChild();

        ImGui::PopStyleVar();   // WindowPadding
        ImGui::PopStyleVar();   // ChildRounding
        ImGui::PopStyleColor(); // ChildBg

        // Espacio entre mensajes (local)
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spacingY);
    }
}

// Aplica ops + devuelve conteo (creadas / modificadas / eliminadas)
ChatPanel::OpCounts ChatPanel::ApplyOpsFromJson(const json& resp) {
    OpCounts counts;

    // --- Caso especial: asset suelto (por compatibilidad si alguien llama a esta función) ---
    if (resp.contains("kind") && resp["kind"].is_string() && resp["kind"] == "asset") {
        std::string fname = resp.value("fileName", "asset.png");
        std::string data = resp.value("data", "");
        // sanitizar nombre
        for (char& ch : fname) {
            if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
                ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') ch = '_';
        }
        if (!data.empty()) {
            std::string outPath = "Assets/Generated/" + fname;
            if (SaveBase64ToFile(data, outPath)) {
                ViewportPanel::AppendLog("[ASSET] Guardada imagen: " + outPath);
            }
            else {
                ViewportPanel::AppendLog("[ASSET] ERROR al guardar: " + outPath);
            }
        }
        return counts; // no hay ops
    }

    // --- Procesamiento normal de ops ---
    if (!resp.contains("ops") || !resp["ops"].is_array()) return counts;

    auto& ctx = SceneContext::Get();
    if (!ctx.scene) ctx.scene = std::make_shared<Scene>();

    // Conjuntos para coalescer por entidad
    std::unordered_set<uint32_t> created, modified, removed;

    for (auto& op : resp["ops"]) {
        std::string type = op.value("op", "");

        if (type == "spawn_box") {
            auto pos = op["pos"];
            auto size = op["size"];
            sf::Color col = TryParseColor(op, sf::Color(60, 60, 70, 255));
            Entity e = ctx.scene->CreateEntity();
            ctx.scene->transforms[e.id] = Transform{ {pos[0].get<float>(), pos[1].get<float>()}, {1.f,1.f}, 0.f };
            ctx.scene->sprites[e.id] = Sprite{ {size[0].get<float>(), size[1].get<float>()}, col };
            ctx.scene->colliders[e.id] = Collider{ {size[0].get<float>() * 0.5f, size[1].get<float>() * 0.5f}, {0.f,0.f} };

            // NUEVO: textura opcional
            if (op.contains("texturePath") && op["texturePath"].is_string()) {
                ctx.scene->textures[e.id] = Texture2D{ op["texturePath"].get<std::string>() };
            }
            created.insert(e.id);
        }
        else if (type == "set_transform") {
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

                if (created.count(id) == 0) modified.insert(id);
            }
        }
        else if (type == "set_component") {
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
            else if (id && comp == "Texture2D" && op.contains("value") && op["value"].is_object()) {
                auto& scene = *ctx.scene;
                const auto& value = op["value"];
                if (value.contains("path") && value["path"].is_string()) {
                    std::string newPath = value["path"].get<std::string>();

                    // crea o muta el componente
                    auto& tex = scene.textures[id];
                    std::string oldPath = tex.path;
                    tex.path = newPath;

                    // invalidar caché para forzar recarga (old y new)
                    if (!oldPath.empty() && oldPath != newPath)
                        Renderer2D::InvalidateTexture(oldPath);
                    Renderer2D::InvalidateTexture(newPath);

                    if (created.count(id) == 0) modified.insert(id);
                }
            }
        }
        else if (type == "remove_entity") {
            uint32_t id = GetEntityId(op);
            if (id && ctx.scene) {
                ctx.scene->DestroyEntity(Entity{ id });
                if (ctx.selected.id == id) ctx.selected = {};

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
