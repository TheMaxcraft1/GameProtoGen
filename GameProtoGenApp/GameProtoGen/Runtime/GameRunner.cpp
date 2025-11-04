#include "GameRunner.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/ScriptSystem.h"
#include "Systems/Renderer2D.h"
#include <SFML/Graphics.hpp>
#include "ECS/SceneSerializer.h"
#include "Core/Log.h"

std::string s_scenePath = "scene.json";

void GameRunner::SetScenePath(std::string path) { s_scenePath = std::move(path); }
const std::string& GameRunner::GetScenePath() { return s_scenePath; }

void GameRunner::Step(Scene& scene, float dt) {
    // Orden recomendado: input -> scripts -> física -> colisiones
    Systems::PlayerControllerSystem::Update(scene, dt);
    Systems::ScriptSystem::Update(scene, dt);
    Systems::PhysicsSystem::Update(scene, dt);
    Systems::CollisionSystem::SolveAABB(scene);
    // Si usás “suelo infinito” además de plataformas estáticas:
    // Systems::CollisionSystem::SolveGround(scene, 900.f); // opcional
}

void GameRunner::Render(const Scene& scene,
    sf::RenderTarget& target,
    const sf::Vector2f& cameraCenter,
    sf::Vector2u virtSize) {
    // Tomo la view actual del target para no perder viewport/scissor del host
    sf::View v = target.getView();

    // SFML 3: setSize recibe UN Vector2f
    v.setSize(sf::Vector2f{
        static_cast<float>(virtSize.x),
        static_cast<float>(virtSize.y)
        });

    v.setCenter(cameraCenter);
    target.setView(v);

    // Si querés limpiar acá, descomentá:
    // target.clear(sf::Color(22, 24, 29));

    Renderer2D::Draw(scene, target);
}

void GameRunner::EnterPlay(Scene& scene) {
    // Resetear VM y forzar on_spawn en el primer update
    Systems::ScriptSystem::ResetVM();
    for (auto& [id, sc] : scene.scripts) sc.loaded = false;

    // (Opcional) limpiar estados físicos transitorios
    for (auto& [id, ph] : scene.physics) {
        ph.onGround = false;
        // ph.velocity = {0.f, 0.f}; // si querés arrancar “quieto”
    }

    Systems::CollisionSystem::ResetTriggers();
}

void GameRunner::ExitPlay(Scene& scene) {
    // 1) Apagar/descartar el estado del VM (borra envs de Lua)
    Systems::ScriptSystem::ResetVM();

    // 2) Limpiar flags/eventos transitorios de colisiones/trigger
    Systems::CollisionSystem::ResetTriggers();

    // 3) (Opcional) normalizar estado físico efímero
    //    No toques posiciones/escena “de diseño” acá.
    for (auto& [id, ph] : scene.physics) {
        ph.onGround = false;
        // ph.velocity = {0.f, 0.f}; // ← descomentá si querés salir sin inercias
    }

    // 4) (Recomendado) cache gráfico: si cambiaste assets durante Play,
    //    al volver a edición forzás un reload limpio.
    Renderer2D::ClearTextureCache();
}

bool GameRunner::ReloadFromDisk(Scene& scene) {
    Scene tmp;
    if (!SceneSerializer::Load(tmp, s_scenePath)) {
        Log::Error(std::string("[RESET] No se pudo cargar: ") + s_scenePath);
        return false;
    }
    scene = std::move(tmp);
    Renderer2D::ClearTextureCache();
    EnterPlay(scene); // rearmar VM/estados para Play
    Log::Info(std::string("[RESET] Reload OK desde: ") + s_scenePath);
    return true;
}