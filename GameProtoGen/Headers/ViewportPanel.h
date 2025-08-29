#pragma once
#include "Application.h"
#include <memory>
#include <SFML/Graphics.hpp>

class ViewportPanel : public gp::Layer {
public:
    ViewportPanel();
    void OnUpdate(const gp::Timestep& dt) override;
    void OnGuiRender() override;

private:
    std::unique_ptr<sf::RenderTexture> m_RT;
    sf::CircleShape m_Circle;
    sf::Clock m_Clock; // por si lo querés usar más adelante

    void EnsureRT(unsigned w, unsigned h);
};
