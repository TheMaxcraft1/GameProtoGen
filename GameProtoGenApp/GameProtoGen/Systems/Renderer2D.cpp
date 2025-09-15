#include "Renderer2D.h"
#include "ECS/Scene.h"
#include "ECS/Components.h"

void Renderer2D::Draw(const Scene& scene, sf::RenderTarget& target) {
    // Recorremos entidades que tengan Transform + Sprite
    for (const auto& e : scene.Entities()) {
        auto itT = scene.transforms.find(e.id);
        auto itS = scene.sprites.find(e.id);
        if (itT == scene.transforms.end() || itS == scene.sprites.end()) continue;

        const Transform& tr = itT->second;
        const Sprite& sp = itS->second;

        sf::RectangleShape rect;
        rect.setSize(sp.size);
        rect.setOrigin(sp.size * 0.5f);
        rect.setPosition(tr.position);
        rect.setScale(tr.scale);
        rect.setRotation(sf::degrees(tr.rotationDeg));
        rect.setFillColor(sp.color);

        target.draw(rect);
    }
}
