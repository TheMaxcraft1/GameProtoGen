#pragma once
#include <unordered_map>
#include <vector>
#include <optional>
#include "Entity.h"
#include "Components.h"

class Scene {
public:
    Scene() = default;

    Entity CreateEntity();
    void DestroyEntity(Entity e);

    // Component storage (MVP: maps por tipo)
    std::unordered_map<EntityID, Transform> transforms;
    std::unordered_map<EntityID, Sprite> sprites;
    std::unordered_map<EntityID, Collider> colliders;
    std::unordered_map<EntityID, Physics2D> physics;
    std::unordered_map<EntityID, PlayerController> playerControllers;

    const std::vector<Entity>& Entities() const { return m_Entities; }

private:
    std::vector<Entity> m_Entities;
    EntityID m_Next{ 1 };
};
