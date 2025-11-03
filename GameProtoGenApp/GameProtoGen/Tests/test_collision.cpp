#include <gtest/gtest.h>
#include "ECS/Scene.h"
#include "Systems/PhysicsSystem.h"

TEST(Collision, FallsOnPlatformSetsOnGround) {
    Scene s;
    auto player = s.CreateEntity();
    s.transforms[player.id] = Transform{ {0,0},{1,1},0 };
    s.sprites[player.id] = Sprite{ {50,100}, sf::Color::White };
    s.colliders[player.id] = Collider{ {25,50},{0,0} };
    s.physics[player.id] = Physics2D{ .velocity = {0,0}, .gravity = 980.f, .gravityEnabled = true };

    auto plat = s.CreateEntity();
    s.transforms[plat.id] = Transform{ {0,200},{1,1},0 };
    s.sprites[plat.id] = Sprite{ {300,40}, sf::Color::White };
    s.colliders[plat.id] = Collider{ {150,20},{0,0} };

    const float dt = 1.f / 60.f;
    bool grounded = false;
    for (int i = 0; i < 240; ++i) {           // hasta 4s como máximo
        Systems::PhysicsSystem::Update(s, dt);
        Systems::CollisionSystem::SolveAABB(s);
        if (s.physics[player.id].onGround) { grounded = true; break; }
    }
    ASSERT_TRUE(grounded);
}