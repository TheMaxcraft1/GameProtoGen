#pragma once
#include "Application.h"
#include <memory>
#include <SFML/Graphics.hpp>
#include "Entity.h" 
#include <optional>     
#include <imgui.h>      

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

    /// --- picking/drag ---
    bool m_Dragging = false;
    EntityID m_DragEntity = 0;
    sf::Vector2f m_DragOffset{ 0.f, 0.f };

    bool m_EnableSnap = false;
    float m_Grid = 32.f;

    void EnsureRT(); // crea m_RT con m_VirtW x m_VirtH

    // Convierte mouse en world (si está sobre la imagen). Devuelve nullopt si está fuera.
    std::optional<sf::Vector2f> ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const;

    // Devuelve la entidad bajo worldPos (última dibujada primero)
    EntityID PickEntityAt(const sf::Vector2f& worldPos) const;

    // Dibuja un rectángulo de selección sobre m_RT
    void DrawSelectionGizmo(sf::RenderTarget& rt) const;
};
