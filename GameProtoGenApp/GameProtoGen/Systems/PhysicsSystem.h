#pragma once
#include "ECS/Scene.h"

namespace Systems {

    class PhysicsSystem {
    public:
        static void Update(Scene& scene, float dt);
    };

    class CollisionSystem {
    public:
        static void SolveGround(Scene& scene, float groundY);
        static void SolveAABB(Scene& scene);
        static void ResetTriggers();
    };

    class PlayerControllerSystem {
    public:
        static void Update(Scene& scene, float dt);
    };

} // namespace Systems
