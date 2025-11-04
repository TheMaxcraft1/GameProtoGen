#pragma once
#include "ECS/Scene.h"
#include <SFML/Graphics.hpp>

class GameRunner {
public:
    // Avanza la simulación (input, scripts, física, colisiones).
    static void Step(Scene& scene, float dt);

    // Dibuja la escena con una vista centrada en cameraCenter (resolución virtual opcional).
    static void Render(const Scene& scene, sf::RenderTarget& target,
        const sf::Vector2f& cameraCenter,
        sf::Vector2u virtSize = { 1600, 900 });

    static void EnterPlay(Scene& scene);
    static void ExitPlay(Scene& scene);
    static void SetScenePath(std::string path);
    static const std::string& GetScenePath();
    static bool ReloadFromDisk(Scene& scene);
};
