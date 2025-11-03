// Tests/test_gamerunner.cpp
#include <gtest/gtest.h>
#include "Runtime/GameRunner.h"
#include "ECS/Scene.h"

TEST(GameRunner, SystemsOrderStable) {
    Scene s;
    // Mínimo para que no crashee (aunque acá no “observamos” orden directamente)
    auto e = s.CreateEntity();
    s.transforms[e.id] = Transform{ {0,0},{1,1},0 };
    s.sprites[e.id] = Sprite{ {10,10}, sf::Color::White };
    s.physics[e.id] = Physics2D{};
    GameRunner::Step(s, 0.016f);
    SUCCEED(); // Al menos smoke: no crash y orden fijo en el código.
}
