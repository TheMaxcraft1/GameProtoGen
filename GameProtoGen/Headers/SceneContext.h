#pragma once
#include <memory>
#include "Scene.h"
#include "Entity.h"

struct SceneContext {
    std::shared_ptr<Scene> scene;
    Entity selected{};

    // Estado de ejecución (simulación)
    struct RuntimeState {
        bool playing = false;   // true = simula (inputs, física, colisiones). false = pausa/edición.
    } runtime;

    static SceneContext& Get() {
        static SceneContext ctx;
        if (!ctx.scene) ctx.scene = std::make_shared<Scene>();
        return ctx;
    }
};
