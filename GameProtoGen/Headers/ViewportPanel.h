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
    std::unique_ptr<sf::RenderTexture> m_PresentRT;
    sf::Clock m_Clock;

    // Resolución virtual fija (16:9) – estilo Unity "Game View"
    const unsigned m_VirtW = 1600;
    const unsigned m_VirtH = 900;

    void EnsureRT(); // crea m_RT con m_VirtW x m_VirtH
};
