#pragma once
#include <memory>
#include "ECS/Entity.h"
#include "Net/ApiClient.h"
#include "Auth/TokenManager.h"

struct EditorContext {
    std::shared_ptr<ApiClient> apiClient;
    std::shared_ptr<TokenManager> tokenManager;
    Entity selected{}; // selección del editor

    static EditorContext& Get() {
        static EditorContext ctx;
        return ctx;
    }
};
