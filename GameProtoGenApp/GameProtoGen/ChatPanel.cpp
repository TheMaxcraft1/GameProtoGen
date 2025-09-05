// GameProtoGen/ChatPanel.cpp
#include "Headers/ChatPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Scene.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include "Headers/SceneSerializer.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ChatPanel::ChatPanel(std::shared_ptr<ApiClient> client)
    : m_Client(std::move(client)) {
}

void ChatPanel::OnGuiRender() {
    ImGui::Begin("Chat", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::TextUnformatted("Ejemplos: \"crear plataforma\", \"mover jugador\"");
    ImGui::InputText("##prompt", &m_Input);  // 👈 requiere imgui_stdlib.h

    ImGui::SameLine();
    ImGui::BeginDisabled(m_Busy || m_Input.empty());
    if (ImGui::Button(m_Busy ? "Enviando..." : "Enviar")) {
        m_Status.clear();
        m_LastResponse.clear();
        m_Busy = true;

        // 👇 NUEVO: dumpear la escena actual en JSON
        auto& ctx = SceneContext::Get();
        nlohmann::json sceneJson = ctx.scene ? SceneSerializer::Dump(*ctx.scene)
            : nlohmann::json::object();

        // 👇 NUEVO: enviar prompt + escena al backend
        m_Fut = m_Client->SendCommandAsync(m_Input, sceneJson);
    }
    ImGui::EndDisabled();

    // Poll async sin bloquear
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

            Entity e = ctx.scene->CreateEntity();
            ctx.scene->transforms[e.id] = Transform{ {pos[0].get<float>(), pos[1].get<float>()}, {1.f,1.f}, 0.f };
            ctx.scene->sprites[e.id] = Sprite{ {size[0].get<float>(), size[1].get<float>()}, sf::Color(60,60,70,255) };
            ctx.scene->colliders[e.id] = Collider{ {size[0].get<float>() * 0.5f, size[1].get<float>() * 0.5f}, {0.f,0.f} };
        }
        else if (type == "set_transform") {
            uint32_t id = op.value("entity", 0u);
            if (id && ctx.scene->transforms.contains(id)) {
                auto& t = ctx.scene->transforms[id];
                if (op.contains("position") && !op["position"].is_null()) {
                    auto p = op["position"];
                    t.position = { p[0].get<float>(), p[1].get<float>() };
                }
                if (op.contains("scale") && !op["scale"].is_null()) {
                    auto s = op["scale"];
                    t.scale = { s[0].get<float>(), s[1].get<float>() };
                }
                if (op.contains("rotation") && !op["rotation"].is_null()) {
                    t.rotationDeg = op["rotation"].get<float>();
                }
            }
        }
    }
}
