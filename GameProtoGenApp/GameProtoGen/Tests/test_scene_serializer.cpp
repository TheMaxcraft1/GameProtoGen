// Tests/test_scene_serializer.cpp
#include <gtest/gtest.h>
#include <filesystem>
#include "ECS/Scene.h"
#include "ECS/SceneSerializer.h"

TEST(SceneSerializer, RoundTrip) {
    Scene s;
    auto e = s.CreateEntity();
    s.transforms[e.id] = Transform{ {10,20},{2,3},15 };
    s.sprites[e.id] = Sprite{ {80,40}, sf::Color(1,2,3,4) };
    s.colliders[e.id] = Collider{ {10,5},{1,2} };
    s.physics[e.id] = Physics2D{ .velocity = {5,6}, .gravity = 981.f, .gravityEnabled = true };

    const std::string path = "scene_test.json";
    ASSERT_TRUE(SceneSerializer::Save(s, path));
    Scene s2;
    ASSERT_TRUE(SceneSerializer::Load(s2, path));
    std::filesystem::remove(path);

    auto it = s2.transforms.find(e.id);
    ASSERT_NE(it, s2.transforms.end());
    EXPECT_FLOAT_EQ(it->second.position.x, 10.f);
    EXPECT_FLOAT_EQ(it->second.scale.y, 3.f);
}
