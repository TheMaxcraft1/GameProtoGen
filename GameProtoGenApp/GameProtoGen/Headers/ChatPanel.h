// GameProtoGen/Headers/ChatPanel.h
#pragma once
#include "Application.h"
#include "ApiClient.h"      // 👈 trae ApiClient::Result COMPLETO
#include <memory>
#include <string>
#include <future>

class ChatPanel : public gp::Layer {
public:
    explicit ChatPanel(std::shared_ptr<ApiClient> client);

    void OnGuiRender() override;

private:
    std::shared_ptr<ApiClient> m_Client;
    std::string m_Input;
    std::string m_LastResponse;
    std::string m_Status;

    bool m_Busy = false;
    std::future<ApiClient::Result> m_Fut;  // 👈 ahora el tipo es completo

    void ApplyOpsFromJson(const nlohmann::json& j);
};
