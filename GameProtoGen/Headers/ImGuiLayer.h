// ImGuiLayer.h
#pragma once

#include <SFML/Graphics.hpp>      // CircleShape, RenderTexture, Color, etc.
#include <SFML/System/Clock.hpp>   // Clock
#include <imgui.h>                 // ImVec4, etc.
#include <memory>

#include "Application.h"           // gp::Layer, Timestep

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

    float m_Radius = 50.f;
    ImVec4 m_Color = ImVec4(0, 1, 0, 1);
    std::unique_ptr<sf::RenderTexture> m_RT;
    sf::CircleShape m_Circle;
    sf::Clock m_Clock;

    void EnsureRT(unsigned w, unsigned h);
};
