#include "Headers/PhysicsSystem.h"
#include "Headers/Components.h"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>

namespace Systems {

    static inline bool Has(Scene& scene, EntityID id, const char* which) {
        if (std::string{ which } == "Transform") return scene.transforms.contains(id);
        if (std::string{ which } == "Physics2D") return scene.colliders.contains(id) == false; // placeholder (no uses)
        return false;
    }

    void PlayerControllerSystem::Update(Scene& scene, float dt) {
        (void)dt;
        // MVP: controlamos la PRIMER entidad que tenga PlayerController
        EntityID playerId = 0;
        for (auto& [id, pc] : scene.colliders) { (void)id; (void)pc; } // placeholder para call graph

        for (auto& [id, _pc] : scene.sprites) { (void)id; (void)_pc; } // noop

        // Buscamos player por la existencia del componente PlayerController (que guardamos en un map aparte?).
        // Para MVP, guardemos PlayerController en sprites? No. Mejor: agreguemos un map más al Scene.

        // Como no queremos cambiar mucho, haremos una heurística: usaremos el PRIMER entity creado (seed) como jugador.
        // Si querés más prolijo, agregá: std::unordered_map<EntityID, PlayerController> playerControllers; en Scene.h

        // Hecha la nota, implementemos VERSION PROLIJA: asumimos que agregaste playerControllers en Scene.h
        for (auto& [id, pc] : scene.playerControllers) {
            playerId = id;
            (void)pc;
            break;
        }
        if (!playerId) return;
        auto itT = scene.transforms.find(playerId);
        auto itP = scene.physics.find(playerId);
        if (itT == scene.transforms.end() || itP == scene.physics.end()) return;

        auto& t = itT->second;
        auto& ph = itP->second;

        // Input horizontal
        float dir = 0.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A) || sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  dir -= 1.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::D) || sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) dir += 1.f;

        // MoveSpeed: si tenés多个 jugadores, usa scene.playerControllers[playerId].moveSpeed
        float moveSpeed = scene.playerControllers[playerId].moveSpeed;
        t.position.x += dir * moveSpeed * dt;

        // Salto (solo si onGround)
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && ph.onGround) {
            ph.velocity.y = -scene.playerControllers[playerId].jumpSpeed;
            ph.onGround = false;
        }
    }

    void PhysicsSystem::Update(Scene& scene, float dt) {
        // Integrar gravedad + velocidad
        for (auto& [id, ph] : scene.physics) {
            if (!scene.transforms.contains(id)) continue;
            auto& t = scene.transforms[id];

            if (ph.gravityEnabled) ph.velocity.y += ph.gravity * dt;

            t.position += ph.velocity * dt;
        }
    }

    static inline bool Overlap1D(float aMin, float aMax, float bMin, float bMax) {
        return (aMin <= bMax) && (bMin <= aMax);
    }

    static inline bool AABBvsAABB(const sf::Vector2f& posA, const Collider& colA,
        const sf::Vector2f& posB, const Collider& colB,
        sf::Vector2f& mtvOut) {
        // Centers son pos + offset; half extents del collider
        sf::Vector2f cA = posA + colA.offset;
        sf::Vector2f cB = posB + colB.offset;

        sf::Vector2f d = cB - cA;
        float ox = (colA.halfExtents.x + colB.halfExtents.x) - std::abs(d.x);
        float oy = (colA.halfExtents.y + colB.halfExtents.y) - std::abs(d.y);

        if (ox > 0.f && oy > 0.f) {
            // Penetración mínima
            if (ox < oy) {
                mtvOut = { (d.x < 0.f ? -ox : ox), 0.f };
            }
            else {
                mtvOut = { 0.f, (d.y < 0.f ? -oy : oy) };
            }
            return true;
        }
        return false;
    }

    void CollisionSystem::SolveGround(Scene& scene, float groundY) {
        // Suelo plano: si el centro + halfExtents.y supera groundY -> clampa posición y resetea vel
        for (auto& [id, ph] : scene.physics) {
            if (!scene.transforms.contains(id) || !scene.colliders.contains(id)) continue;
            auto& t = scene.transforms[id];
            auto& c = scene.colliders[id];

            float bottom = (t.position.y + c.offset.y) + c.halfExtents.y;
            if (bottom > groundY) {
                float correction = bottom - groundY;
                t.position.y -= correction;               // subimos hasta tocar el suelo
                ph.velocity.y = 0.f;
                ph.onGround = true;
            }
        }
    }

    void CollisionSystem::SolveAABB(Scene& scene) {
        // Colisión entidad dinámica (tenga Physics2D) vs colliders estáticos (sin Physics2D)
        for (auto& [idA, phA] : scene.physics) {
            if (!scene.transforms.contains(idA) || !scene.colliders.contains(idA)) continue;

            auto& tA = scene.transforms[idA];
            auto& cA = scene.colliders[idA];

            for (auto& [idB, cB] : scene.colliders) {
                if (idA == idB) continue;
                // Considerá estático si no tiene Physics2D
                if (scene.physics.contains(idB)) continue;
                if (!scene.transforms.contains(idB)) continue;

                auto& tB = scene.transforms[idB];
                sf::Vector2f mtv{ 0.f, 0.f };
                if (AABBvsAABB(tA.position, cA, tB.position, cB, mtv)) {
                    tA.position -= mtv;
                    // Reacción simple: si resolvimos en Y hacia arriba, marcamos onGround
                    if (mtv.y < 0.f) {
                        phA.velocity.y = 0.f;
                        phA.onGround = true;
                    }
                    if (mtv.y > 0.f) {
                        phA.velocity.y = 0.f;
                    }
                    if (mtv.x != 0.f) {
                        phA.velocity.x = 0.f;
                    }
                }
            }
        }
    }

} // namespace Systems
