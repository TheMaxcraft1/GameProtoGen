#pragma once
#include <memory>
#include <SFML/System/Vector2.hpp>   // 👈 necesario para sf::Vector2f
#include "Scene.h"
#include "Entity.h"

struct SceneContext {
    std::shared_ptr<Scene> scene;
    Entity selected{};

    // Centro actual de la cámara (lo mantiene ViewportPanel)
    sf::Vector2f cameraCenter{ 800.f, 450.f };

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
