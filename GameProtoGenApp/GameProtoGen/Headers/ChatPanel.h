// GameProtoGen/Headers/ChatPanel.h
#pragma once
#include "Application.h"
#include "ApiClient.h"
#include <memory>
#include <string>
#include <future>
#include <vector>

class ChatPanel : public gp::Layer {
public:
    explicit ChatPanel(std::shared_ptr<ApiClient> client);

    void OnGuiRender() override;

private:
    // ---- Modelo de mensajes (chat) ----
    enum class Role { User, Assistant };
    struct ChatMessage {
        Role role;
        std::string text;
        bool typing = false; // burbuja "..." del asistente
    };

    std::shared_ptr<ApiClient> m_Client;

    // UI / estado
    std::string m_Input;
    bool m_Busy = false;
    std::future<ApiClient::Result> m_Fut;
    std::vector<ChatMessage> m_History;
    int m_TypingIndex = -1;  // índice de la burbuja "tipeando"
    bool m_RequestScrollToBottom = false;
    bool m_FocusInputNextFrame = false;

    // Render
    void RenderHistory();

    // Lógica de respuesta
    struct OpCounts { int created = 0, modified = 0, removed = 0; };
    OpCounts ApplyOpsFromJson(const nlohmann::json& resp);

    // Helpers
    void SendCurrentPrompt();
};
