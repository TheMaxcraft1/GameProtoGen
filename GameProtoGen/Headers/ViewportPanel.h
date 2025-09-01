#pragma once
#include "Application.h"
#include "Entity.h"
#include <memory>
#include <optional>
#include <imgui.h>
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

    // Resolución virtual fija (16:9)
    const unsigned m_VirtW = 1600;
    const unsigned m_VirtH = 900;

    /// --- simulación ---
    // Arranca en PAUSA
    bool m_Playing = false;

    /// --- herramientas ---
    enum class Tool { Select, Pan };
    Tool m_Tool = Tool::Select;

    /// --- picking/drag ---
    bool         m_Dragging = false;
    EntityID     m_DragEntity = 0;
    sf::Vector2f m_DragOffset{ 0.f, 0.f };

    // Snap siempre activo (tamaño de grilla)
    float m_Grid = 32.f;

    // Cámara
    sf::Vector2f m_CamCenter{ m_VirtW * 0.5f, m_VirtH * 0.5f };

    // Pan (Q + LMB o herramienta Pan) solo en pausa
    bool m_Panning = false;

    void EnsureRT(); // crea/ajusta RTs con m_VirtW x m_VirtH

    // Convierte mouse en mundo (si está sobre la imagen). nullopt si está fuera.
    std::optional<sf::Vector2f> ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const;

    // Devuelve la entidad bajo worldPos (última dibujada primero)
    EntityID PickEntityAt(const sf::Vector2f& worldPos) const;

    // Dibujo auxiliar
    void DrawSelectionGizmo(sf::RenderTarget& rt) const;
    void DrawGrid(sf::RenderTarget& rt) const;  // grilla visual
};
