#include "Scene.h"

Entity Scene::CreateEntity() {
    Entity e{ m_Next++ };
    m_Entities.push_back(e);
    return e;
}

Entity Scene::CreateEntityWithId(EntityID id) {
    Entity e{ id };
    m_Entities.push_back(e);
    if (id >= m_Next) m_Next = id + 1; // mantener el contador coherente
    return e;
}

void Scene::DestroyEntity(Entity e) {
    if (!e) return;
    // borrar componentes
    transforms.erase(e.id);
    sprites.erase(e.id);
    colliders.erase(e.id);
    textures.erase(e.id);
    physics.erase(e.id);
    scripts.erase(e.id);
    playerControllers.erase(e.id);
    // borrar de la lista de entidades (O(n), suficiente para MVP)
    for (auto it = m_Entities.begin(); it != m_Entities.end(); ++it) {
        if (it->id == e.id) { m_Entities.erase(it); break; }
    }
}
