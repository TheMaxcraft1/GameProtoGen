#pragma once
#include "Core/Application.h"
#include <SFML/System/Clock.hpp>

namespace gp { class SFMLWindow; }

class ImGuiLayer : public gp::Layer {
public:
    explicit ImGuiLayer(gp::SFMLWindow& window);
    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(const gp::Timestep& dt) override;
    void OnGuiRender() override;

private:
    gp::SFMLWindow& m_Window;
    bool m_BuiltDock = false;
    sf::Clock m_Clock;
};
