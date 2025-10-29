#include <SFML/Graphics.hpp>
#include <optional>
#include "Runtime/SceneContext.h"
#include "ECS/Scene.h"
#include "ECS/SceneSerializer.h"
#include "Systems/Renderer2D.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/ScriptSystem.h"

int main(int argc, char** argv) {
    std::string scenePath = (argc >= 2) ? argv[1] : "scene.json";

    sf::RenderWindow window(sf::VideoMode({ 1600u, 900u }), "GameProtoGen - Player");
    window.setVerticalSyncEnabled(true);

    // MUY IMPORTANTE: activar el contexto en este hilo
    window.setActive(true);

    auto& ctx = SceneContext::Get();
    ctx.scene = std::make_shared<Scene>();
    SceneSerializer::Load(*ctx.scene, scenePath);

    EntityID playerId = 0;
    if (!ctx.scene->playerControllers.empty())
        playerId = ctx.scene->playerControllers.begin()->first;

    sf::Clock clock;
    const float fixedDt = 1.f / 120.f;
    float acc = 0.f;

    while (window.isOpen()) {
        while (auto ev = window.pollEvent()) {
            // Cerrar ventana
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            }
            // Ejemplo: teclado (por si querés capturar)
            if (auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                }
            }
            if (auto* rz = ev->getIf<sf::Event::Resized>()) {
                sf::View v = window.getView();
                sf::Vector2f newSize(
                    static_cast<float>(rz->size.x),
                    static_cast<float>(rz->size.y)
                );
                v.setSize(newSize);
                v.setCenter(sf::Vector2f(newSize.x * 0.5f, newSize.y * 0.5f));
                window.setView(v);
            }
        }

        float dt = clock.restart().asSeconds();
        acc += dt;
        while (acc >= fixedDt) {
            Systems::ScriptSystem::Update(*ctx.scene, fixedDt);
            Systems::PlayerControllerSystem::Update(*ctx.scene, fixedDt);
            Systems::PhysicsSystem::Update(*ctx.scene, fixedDt);
            Systems::CollisionSystem::SolveGround(*ctx.scene, 900.f);
            Systems::CollisionSystem::SolveAABB(*ctx.scene);
            acc -= fixedDt;
        }

        if (playerId && ctx.scene->transforms.contains(playerId))
            ctx.cameraCenter = ctx.scene->transforms[playerId].position;

        window.clear(sf::Color(30, 30, 35));
        Renderer2D::Draw(*ctx.scene, window);
        window.display();
    }
    return 0;
}
