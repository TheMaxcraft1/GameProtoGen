#include "Renderer2D.h"
#include "ECS/Scene.h"
#include "ECS/Components.h"
#include <unordered_map>
#include <memory>

static std::unordered_map<std::string, std::shared_ptr<sf::Texture>> s_TexCache;

static std::shared_ptr<sf::Texture> GetTexture(const std::string& path) {
    if (path.empty()) return nullptr;
    if (auto it = s_TexCache.find(path); it != s_TexCache.end()) return it->second;
    auto tex = std::make_shared<sf::Texture>();
    if (tex->loadFromFile(path)) {
        tex->setSmooth(true);
        s_TexCache[path] = tex;
        return tex;
    }
    return nullptr;
}

void Renderer2D::ClearTextureCache() {
    s_TexCache.clear();
}

void Renderer2D::Draw(const Scene& scene, sf::RenderTarget& target) {
    for (const auto& e : scene.Entities()) {
        auto itT = scene.transforms.find(e.id);
        auto itS = scene.sprites.find(e.id);
        if (itT == scene.transforms.end() || itS == scene.sprites.end()) continue;

        const Transform& tr = itT->second;
        const Sprite& sp = itS->second;

        // ¿Hay textura?
        std::shared_ptr<sf::Texture> tex;
        if (auto itX = scene.textures.find(e.id); itX != scene.textures.end()) {
            tex = GetTexture(itX->second.path);
        }

        if (tex) {
            sf::Sprite spr(*tex);
            // Escalar la textura al tamaño pedido (sp.size), luego aplicar Transform.scale
            auto texSize = tex->getSize();
            if (texSize.x == 0 || texSize.y == 0) continue;
            sf::Vector2f baseScale{ sp.size.x / texSize.x, sp.size.y / texSize.y };
            sf::Vector2f finalScale{
                            baseScale.x * tr.scale.x,
                            baseScale.y * tr.scale.y
                                    };
            spr.setScale(finalScale);
            spr.setOrigin(sf::Vector2f(texSize) * 0.5f);
            spr.setPosition(tr.position);
            spr.setRotation(sf::degrees(tr.rotationDeg));
            target.draw(spr);
        }
        else {
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
}
