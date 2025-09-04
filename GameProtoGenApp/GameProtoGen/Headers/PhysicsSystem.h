#pragma once
#include "Scene.h"

namespace Systems {

    class PhysicsSystem {
    public:
        static void Update(Scene& scene, float dt);
    };

    class CollisionSystem {
    public:
        // MVP: colisiona contra “suelo” (y = groundY) y AABB contra colliders estáticos
        static void SolveGround(Scene& scene, float groundY);
        static void SolveAABB(Scene& scene);
    };

    class PlayerControllerSystem {
    public:
        // Lee input global y aplica a la primera entidad que tenga PlayerController (MVP)
        static void Update(Scene& scene, float dt);
    };

} // namespace Systems
