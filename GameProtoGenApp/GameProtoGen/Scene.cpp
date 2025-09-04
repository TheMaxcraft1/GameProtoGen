#include "Headers/Scene.h"

Entity Scene::CreateEntity() {
    Entity e{ m_Next++ };
    m_Entities.push_back(e);
    return e;
}

void Scene::DestroyEntity(Entity e) {
    if (!e) return;
    // borrar componentes
    transforms.erase(e.id);
    sprites.erase(e.id);
    colliders.erase(e.id);
    // borrar de la lista de entidades (O(n), suficiente para MVP)
    for (auto it = m_Entities.begin(); it != m_Entities.end(); ++it) {
        if (it->id == e.id) { m_Entities.erase(it); break; }
    }
}
