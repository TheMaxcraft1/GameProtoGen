#pragma once
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

struct Transform {
    sf::Vector2f position{ 0.f, 0.f };
    sf::Vector2f scale{ 1.f, 1.f };
    float rotationDeg = 0.f;
};

struct Sprite {
    sf::Vector2f size{ 100.f, 100.f };  // ancho/alto en píxeles
    sf::Color color{ sf::Color::Green };
    // (MVP) sin textura por ahora
};

struct Collider {
    // AABB simple relativo al centro del objeto
    sf::Vector2f halfExtents{ 50.f, 50.f };
    sf::Vector2f offset{ 0.f, 0.f };
};

struct Physics2D {
    sf::Vector2f velocity{ 0.f, 0.f };
    float gravity = 2000.f;   // px/s^2 (ajustá a gusto)
    bool gravityEnabled = true;
    bool onGround = false;
};

struct PlayerController {
    float moveSpeed = 500.f;  // px/s
    float jumpSpeed = 900.f;  // px/s (hacia arriba = negativo si y+ va hacia abajo)
};
