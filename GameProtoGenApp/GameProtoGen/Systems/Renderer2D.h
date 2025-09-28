#pragma once
#include <SFML/Graphics.hpp>
class Scene;

class Renderer2D {
public:
    static void Draw(const Scene& scene, sf::RenderTarget& target);
    static void ClearTextureCache();
    static std::shared_ptr<sf::Texture> GetTextureCached(const std::string& path);
    static void InvalidateTexture(const std::string& path);
};
