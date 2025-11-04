#include <SFML/Graphics.hpp>
#include <filesystem>
#include <iostream>
#include "ECS/Scene.h"
#include "ECS/SceneSerializer.h"
#include "Runtime/GameRunner.h"
#include "Systems/Renderer2D.h"

using std::filesystem::exists;
using std::filesystem::path;

static path DetectScenePath(int argc, char** argv) {
#ifdef _WIN32
    char sep = '\\';
#else
    char sep = '/';
#endif
    // 1) Si pasaron ruta por argv[1], usala
    if (argc > 1) return path(argv[1]);

    // 2) Si no, probá "scene.json" junto al ejecutable
    path exeDir = path(argv[0]).parent_path();
    path p = exeDir / "scene.json";
    if (exists(p)) return p;

    // 3) Fallback: "./scene.json"
    return path("scene.json");
}

static sf::Vector2f FindPlayerCenter(const Scene& scene, sf::Vector2f fallback = { 800.f, 450.f }) {
    EntityID playerId = 0;
    if (!scene.playerControllers.empty())
        playerId = scene.playerControllers.begin()->first;
    if (!playerId) return fallback;
    auto itT = scene.transforms.find(playerId);
    if (itT == scene.transforms.end()) return fallback;
    return itT->second.position;
}

int main(int argc, char** argv) {
    // Ventana 1600x900 (16:9)
    unsigned virtW = 1600, virtH = 900;
    sf::RenderWindow window(sf::VideoMode({ virtW, virtH }), "GameProtoGen — Player");
    window.setVerticalSyncEnabled(true);

    // Carga de escena
    Scene scene;
    const path scenePath = DetectScenePath(argc, argv);
    GameRunner::SetScenePath(scenePath.string());
    if (!exists(scenePath) || !SceneSerializer::Load(scene, scenePath.string())) {
        std::cerr << "[PLAYER] No se pudo cargar la escena desde: " << scenePath << "\n";
        // Semilla mínima
        auto e = scene.CreateEntity();
        scene.transforms[e.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
        scene.sprites[e.id] = Sprite{ {80.f,120.f}, sf::Color(0,255,0,255) };
        scene.physics[e.id] = Physics2D{};
        scene.playerControllers[e.id] = PlayerController{ 500.f, 900.f };
        auto ground = scene.CreateEntity();
        scene.transforms[ground.id] = Transform{ {800.f, 820.f}, {1.f,1.f}, 0.f };
        scene.sprites[ground.id] = Sprite{ {1600.f,160.f}, sf::Color(60,60,70,255) };
        scene.colliders[ground.id] = Collider{ {800.f,80.f}, {0.f,0.f} };
    }

    // Preparar play-state
    GameRunner::EnterPlay(scene);

    sf::Clock clock;
    sf::Vector2f cameraCenter = FindPlayerCenter(scene);

    while (window.isOpen()) {
        // Eventos (SFML 3)
        while (auto ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            }
            // Ejemplos extra:
            // if (ev->is<sf::Event::KeyPressed>()) { ... }
            // if (auto* resized = ev->getIf<sf::Event::Resized>()) { ... }
        }

        float dt = clock.restart().asSeconds();

        GameRunner::Step(scene, dt);

        sf::Vector2f target = FindPlayerCenter(scene, cameraCenter);
        const float lerp = 0.15f;
        cameraCenter.x += (target.x - cameraCenter.x) * lerp;
        cameraCenter.y += (target.y - cameraCenter.y) * lerp;

        window.clear(sf::Color(22, 24, 29));
        GameRunner::Render(scene, window, cameraCenter, { virtW, virtH });
        window.display();
    }

    GameRunner::ExitPlay(scene);
    return 0;
}
