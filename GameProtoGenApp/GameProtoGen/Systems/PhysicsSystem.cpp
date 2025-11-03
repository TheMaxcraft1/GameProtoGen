#include "PhysicsSystem.h"
#include "ECS/Components.h"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>   // std::clamp (si lo necesitás)
#include <string>      // ← necesitabas esto si usabas std::string en helpers (ya lo quitamos)
#include <unordered_set>
#include "Core/Log.h"
#include "Systems/ScriptSystem.h"

namespace Systems {

    static std::unordered_set<uint64_t> s_prevOverlaps;
    static std::unordered_set<uint64_t> s_currOverlaps;

    static inline uint64_t PairKey(EntityID a, EntityID b) {
        // dirigido (A → B). Si quisieras no dirigido: ordená min/max.
        return (uint64_t(a) << 32) | uint64_t(b);
    }

    static inline void FireTriggerEnter(Scene& scene, EntityID triggerId, EntityID otherId) {
        (void)scene;
        Log::Info("[TRIGGER] enter  trigger=" + std::to_string(triggerId) +
            " other=" + std::to_string(otherId));

        Systems::ScriptSystem::OnTriggerEnter(scene, triggerId, otherId);
    }

    // --- PlayerController: WASD / ←→ + Space ---
    void PlayerControllerSystem::Update(Scene& scene, float dt) {
        (void)dt;

        // Tomamos el primer entity que tenga PlayerController (MVP)
        EntityID playerId = 0;
        for (auto& [id, pc] : scene.playerControllers) {
            (void)pc;
            playerId = id;
            break;
        }
        if (!playerId) return;

        auto itT = scene.transforms.find(playerId);
        auto itP = scene.physics.find(playerId);
        if (itT == scene.transforms.end() || itP == scene.physics.end()) return;

        auto& t = itT->second;
        auto& ph = itP->second;

        using Key = sf::Keyboard::Key;

        // Input horizontal (SFML 3: usar Keyboard::Key::X)
        float dir = 0.f;
        if (sf::Keyboard::isKeyPressed(Key::A) || sf::Keyboard::isKeyPressed(Key::Left))  dir -= 1.f;
        if (sf::Keyboard::isKeyPressed(Key::D) || sf::Keyboard::isKeyPressed(Key::Right)) dir += 1.f;

        float moveSpeed = scene.playerControllers[playerId].moveSpeed;
        t.position.x += dir * moveSpeed * dt;

        // Salto (solo si está en el suelo)
        if (sf::Keyboard::isKeyPressed(Key::Space) && ph.onGround) {
            ph.velocity.y = -scene.playerControllers[playerId].jumpSpeed; // y- hacia arriba
            ph.onGround = false;
        }
    }

    // --- Física: integrar velocidad + gravedad ---
    void PhysicsSystem::Update(Scene& scene, float dt) {
        for (auto& [id, ph] : scene.physics) {
            if (!scene.transforms.contains(id)) continue;

            // Importante: resetear "onGround" cada frame; colisiones lo volverán a true
            ph.onGround = false;

            if (ph.gravityEnabled) {
                ph.velocity.y += ph.gravity * dt;  // y+ hacia abajo en SFML
            }

            auto& t = scene.transforms[id];
            t.position += ph.velocity * dt;
        }
    }

    // --- AABB helper ---
    static inline bool AABBvsAABB(const sf::Vector2f& posA, const Collider& colA,
        const sf::Vector2f& posB, const Collider& colB,
        sf::Vector2f& mtvOut) {
        const sf::Vector2f cA = posA + colA.offset;
        const sf::Vector2f cB = posB + colB.offset;

        const sf::Vector2f d = cB - cA;
        const float ox = (colA.halfExtents.x + colB.halfExtents.x) - std::abs(d.x);
        const float oy = (colA.halfExtents.y + colB.halfExtents.y) - std::abs(d.y);

        if (ox > 0.f && oy > 0.f) {
            // Resolver por el eje de mínima penetración
            if (ox < oy) mtvOut = { (d.x < 0.f ? -ox : ox), 0.f };
            else         mtvOut = { 0.f, (d.y < 0.f ? -oy : oy) };
            return true;
        }
        return false;
    }

    // --- Suelo plano en y = groundY ---
    void CollisionSystem::SolveGround(Scene& scene, float groundY) {
        for (auto& [id, ph] : scene.physics) {
            if (!scene.transforms.contains(id) || !scene.colliders.contains(id)) continue;

            auto& t = scene.transforms[id];
            auto& c = scene.colliders[id];

            // Escala absoluta (por si hay flips)
            const sf::Vector2f scaleAbs{ std::abs(t.scale.x), std::abs(t.scale.y) };

            // Altura efectiva del AABB (prioriza Sprite.size; sino usa Collider.halfExtents)
            float halfY = c.halfExtents.y * scaleAbs.y;
            if (auto itS = scene.sprites.find(id); itS != scene.sprites.end()) {
                halfY = (itS->second.size.y * scaleAbs.y) * 0.5f;
            }

            const float bottom = (t.position.y + c.offset.y) + halfY;
            if (bottom > groundY) {
                const float correction = bottom - groundY;
                t.position.y -= correction;    // subir hasta tocar el suelo
                ph.velocity.y = 0.f;
                ph.onGround = true;
            }
        }
    }

    void CollisionSystem::ResetTriggers() {
        s_prevOverlaps.clear();
        s_currOverlaps.clear();
    }

    void CollisionSystem::SolveAABB(Scene& scene) {
        // Limpiamos los overlaps de este frame
        s_currOverlaps.clear();

        for (auto& [idA, phA] : scene.physics) {
            if (!scene.transforms.contains(idA) || !scene.colliders.contains(idA)) continue;

            auto& tA = scene.transforms[idA];
            auto& cA = scene.colliders[idA];

            for (auto& [idB, cB] : scene.colliders) {
                if (idA == idB) continue;
                if (scene.physics.contains(idB)) continue;      // B debe ser estático
                if (!scene.transforms.contains(idB)) continue;

                auto& tB = scene.transforms[idB];

                // ----- A: centro y halfExtents efectivos -----
                const sf::Vector2f scaleA{ std::abs(tA.scale.x), std::abs(tA.scale.y) };
                sf::Vector2f heA{
                    cA.halfExtents.x * scaleA.x,
                    cA.halfExtents.y * scaleA.y
                };
                if (auto itSA = scene.sprites.find(idA); itSA != scene.sprites.end()) {
                    heA.x = (itSA->second.size.x * scaleA.x) * 0.5f;
                    heA.y = (itSA->second.size.y * scaleA.y) * 0.5f;
                }
                const sf::Vector2f centerA = tA.position + cA.offset;

                // ----- B: centro y halfExtents efectivos -----
                const auto& cBref = cB; // alias
                const auto& tBref = tB;
                const sf::Vector2f scaleB{ std::abs(tBref.scale.x), std::abs(tBref.scale.y) };
                sf::Vector2f heB{
                    cBref.halfExtents.x * scaleB.x,
                    cBref.halfExtents.y * scaleB.y
                };
                if (auto itSB = scene.sprites.find(idB); itSB != scene.sprites.end()) {
                    heB.x = (itSB->second.size.x * scaleB.x) * 0.5f;
                    heB.y = (itSB->second.size.y * scaleB.y) * 0.5f;
                }
                const sf::Vector2f centerB = tBref.position + cBref.offset;

                // ----- Overlap -----
                const sf::Vector2f d = centerB - centerA;
                const float ox = (heA.x + heB.x) - std::abs(d.x);
                const float oy = (heA.y + heB.y) - std::abs(d.y);

                if (ox > 0.f && oy > 0.f) {
                    const bool aTrig = cA.isTrigger;
                    const bool bTrig = cBref.isTrigger;

                    if (aTrig || bTrig) {
                        // Registrar overlaps para triggerEnter (sin resolver física)
                        if (aTrig) {
                            const uint64_t kAB = PairKey(idA, idB);
                            if (!s_prevOverlaps.count(kAB)) {
                                FireTriggerEnter(scene, idA, idB);
                            }
                            s_currOverlaps.insert(kAB);
                        }
                        if (bTrig) {
                            const uint64_t kBA = PairKey(idB, idA);
                            if (!s_prevOverlaps.count(kBA)) {
                                FireTriggerEnter(scene, idB, idA);
                            }
                            s_currOverlaps.insert(kBA);
                        }
                        // Importante: no empujar al dinámico contra un trigger
                        continue;
                    }

                    // Resolver por eje de mínima penetración (colisión física)
                    if (ox < oy) {
                        const float pushX = (d.x < 0.f ? -ox : ox);
                        tA.position.x -= pushX;
                        phA.velocity.x = 0.f;
                    }
                    else {
                        const float pushY = (d.y < 0.f ? -oy : oy);
                        tA.position.y -= pushY;
                        phA.velocity.y = 0.f;
                        if (pushY > 0.f) phA.onGround = true; // empujamos hacia arriba => aterriza
                    }
                }
            }
        }

        // Rotamos buffers para el próximo frame (lo que fue curr ahora es prev)
        s_prevOverlaps.swap(s_currOverlaps);
    }


} // namespace Systems
