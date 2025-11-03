// GameProtoGenApp/GameProtoGen/Runtime/SceneContext.h
#pragma once
#include <memory>
#include <SFML/System/Vector2.hpp>
#include "ECS/Scene.h"

// Contexto *runtime-only*: compartido entre Editor y Player, sin dependencias de red/auth/UI.
struct SceneContext {
    std::shared_ptr<Scene> scene;
    sf::Vector2f cameraCenter{ 800.f, 450.f };

    static SceneContext& Get() {
        static SceneContext ctx;
        if (!ctx.scene) ctx.scene = std::make_shared<Scene>();
        return ctx;
    }
};
