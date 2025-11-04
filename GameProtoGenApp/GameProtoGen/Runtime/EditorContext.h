#pragma once
#include <memory>
#include "ECS/Entity.h"
#include "Net/ApiClient.h"
#include "Auth/TokenManager.h"

// Contexto exclusivo del Editor: auth, APIs, selección, flags de ejecución, etc.
struct EditorContext {
    std::shared_ptr<ApiClient> apiClient;
    std::shared_ptr<TokenManager> tokenManager;
    Entity selected{};
    std::string projectPath = "";
    struct RuntimeState {
        bool playing = false;
        std::shared_ptr<Scene> sceneBackup;
        sf::Vector2f cameraBackup{ 800.f, 450.f };
        Entity selectedBackup{};
    } runtime;
    bool requestSelectTool = false; // pedir pasar a herramienta Select

    static EditorContext& Get() {
        static EditorContext ctx;
        return ctx;
    }
};
