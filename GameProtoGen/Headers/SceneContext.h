#pragma once
#include <memory>
#include "Scene.h"
#include "Entity.h"

struct SceneContext {
    std::shared_ptr<Scene> scene;
    Entity selected{};

    static SceneContext& Get() {
        static SceneContext ctx;
        if (!ctx.scene) ctx.scene = std::make_shared<Scene>();
        return ctx;
    }
};
