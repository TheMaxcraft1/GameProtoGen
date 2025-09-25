#pragma once
#include <SFML/Graphics.hpp>
class Scene;

class Renderer2D {
public:
    static void Draw(const Scene& scene, sf::RenderTarget& target);
    static void ClearTextureCache();
};
