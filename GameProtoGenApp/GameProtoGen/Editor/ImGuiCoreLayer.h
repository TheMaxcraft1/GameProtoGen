#pragma once
#include "Core/Application.h"
#include <SFML/System/Clock.hpp>
namespace gp { class SFMLWindow; }
class ImGuiCoreLayer : public gp::Layer {
public:
    explicit ImGuiCoreLayer(gp::SFMLWindow& window);
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(const gp::Timestep& dt) override;
    void OnGuiRender() override {} // NO dibuja nada (sin menús/dock)
private:
    gp::SFMLWindow& m_Window;
    sf::Clock m_Clock;
    bool m_Initialized = false;
};
