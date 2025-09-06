// GameProtoGen/ChatPanel.cpp
#include "Headers/ChatPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Scene.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include "Headers/SceneSerializer.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>

using json = nlohmann::json;

// ------------------------- helpers de color -------------------------
static inline bool is_hex_digit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

static sf::Color ColorFromHexString(const std::string& in) {
    // Acepta: #RRGGBB, #RRGGBBAA, RRGGBB, RRGGBBAA
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
    if (j.contains("colorHex") && j["colorHex"].is_string()) {
        return ColorFromHexString(j["colorHex"].get<std::string>());
    }
    if (j.contains("color") && j["color"].is_object()) {
        const auto& c = j["color"];
        auto clamp255 = [](int v) { return std::clamp(v, 0, 255); };
        int r = c.value("r", 255);
        int g = c.value("g", 255);
        int b = c.value("b", 255);
        int a = c.value("a", 255);
        return sf::Color((std::uint8_t)clamp255(r), (std::uint8_t)clamp255(g), (std::uint8_t)clamp255(b), (std::uint8_t)clamp255(a));
    }
    return fallback;
}
// -------------------------------------------------------------------

ChatPanel::ChatPanel(std::shared_ptr<ApiClient> client)
    : m_Client(std::move(client)) {
}

void ChatPanel::OnGuiRender() {
    ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::TextUnformatted("Ejemplos: \"crear plataforma\", \"mover jugador\"");
    ImGui::InputText("##prompt", &m_Input);

    ImGui::SameLine();
    ImGui::BeginDisabled(m_Busy || m_Input.empty());
    if (ImGui::Button(m_Busy ? "Enviando..." : "Enviar")) {
        m_Status.clear();
        m_LastResponse.clear();
        m_Busy = true;

        auto& ctx = SceneContext::Get();
        nlohmann::json sceneJson = ctx.scene ? SceneSerializer::Dump(*ctx.scene)
            : nlohmann::json::object();
        m_Fut = m_Client->SendCommandAsync(m_Input, sceneJson);
    }
    ImGui::EndDisabled();

    if (m_Busy && m_Fut.valid()) {
        using namespace std::chrono_literals;
        if (m_Fut.wait_for(0ms) == std::future_status::ready) {
            auto res = m_Fut.get();
            m_Busy = false;
            if (res.ok()) {
                m_LastResponse = res.data->dump(2);
                ApplyOpsFromJson(*res.data);
            }
            else {
                m_Status = "Error: " + res.error;
            }
        }
    }

    if (!m_Status.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted(m_Status.c_str());
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Respuesta (ops):");
    ImGui::BeginChild("##ops_view", ImVec2(0, 220), true);
    if (!m_LastResponse.empty()) ImGui::TextUnformatted(m_LastResponse.c_str());
    ImGui::EndChild();

    ImGui::End();
}


void ChatPanel::ApplyOpsFromJson(const json& resp) {
    if (!resp.contains("ops") || !resp["ops"].is_array()) return;

    auto& ctx = SceneContext::Get();
    if (!ctx.scene) ctx.scene = std::make_shared<Scene>();

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
        }
        else if (type == "set_transform") {
            uint32_t id = op.value("entity", 0u);
            if (id && ctx.scene->transforms.contains(id)) {
                auto& t = ctx.scene->transforms[id];

                // position
                if (op.contains("position") && !op["position"].is_null()) {
                    auto p = op["position"];
                    t.position = { p[0].get<float>(), p[1].get<float>() };
                }

                // size (permitimos que el modelo use "size" aquí por error común)
                if (op.contains("size") && !op["size"].is_null() && ctx.scene->sprites.contains(id)) {
                    auto s = op["size"];
                    float w = std::max(1.f, s[0].get<float>());
                    float h = std::max(1.f, s[1].get<float>());
                    ctx.scene->sprites[id].size = { w, h };
                }

                // scale — si parecen píxeles, interpretamos como size; si no, clamp de escala
                if (op.contains("scale") && !op["scale"].is_null()) {
                    auto s = op["scale"];
                    float sx = s[0].get<float>();
                    float sy = s[1].get<float>();

                    bool looksLikeSize = (std::fabs(sx) > 10.f) || (std::fabs(sy) > 10.f);
                    if (looksLikeSize && ctx.scene->sprites.contains(id)) {
                        // Interpretar como tamaño en píxeles
                        float w = std::max(1.f, sx);
                        float h = std::max(1.f, sy);
                        ctx.scene->sprites[id].size = { w, h };
                        t.scale = { 1.f, 1.f }; // tamaño absoluto => escala unidad
                    }
                    else {
                        auto clampScale = [](float v) { return std::clamp(v, 0.05f, 10.f); };
                        t.scale = { clampScale(sx), clampScale(sy) };
                    }
                }

                // rotation
                if (op.contains("rotation") && !op["rotation"].is_null()) {
                    t.rotationDeg = op["rotation"].get<float>();
                }
            }
        }
        else if (type == "remove_entity") {
            uint32_t id = op.value("entity", 0u);
            if (id && ctx.scene) ctx.scene->DestroyEntity(Entity{ id });
        }
        else if (type == "set_component") {
            uint32_t id = op.value("entity", 0u);
            std::string comp = op.value("component", "");
            if (id && comp == "Sprite" && ctx.scene->sprites.contains(id) && op.contains("value")) {
                auto& sp = ctx.scene->sprites[id];
                const auto& value = op["value"];

                // colorHex o color{r,g,b,a}
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

                // NUEVO: tamaño por set_component
                if (value.contains("size") && value["size"].is_array() && value["size"].size() >= 2) {
                    float w = std::max(1.f, value["size"][0].get<float>());
                    float h = std::max(1.f, value["size"][1].get<float>());
                    sp.size = { w, h };
                }
            }
        }
    }
}
