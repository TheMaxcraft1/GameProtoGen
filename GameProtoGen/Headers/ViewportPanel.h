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
    void OnAttach() override;               // carga de iconos
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
    bool m_Playing = false;                 // arranca en pausa

    /// --- herramientas ---
    enum class Tool { Select, Pan };
    Tool m_Tool = Tool::Select;

    /// --- picking/drag ---
    bool         m_Dragging = false;
    EntityID     m_DragEntity = 0;
    sf::Vector2f m_DragOffset{ 0.f, 0.f };

    // Snap siempre activo
    float m_Grid = 32.f;

    // Cámara
    sf::Vector2f m_CamCenter{ m_VirtW * 0.5f, m_VirtH * 0.5f };

    // Pan (solo en pausa)
    bool m_Panning = false;

    // Iconos (PNG)
    bool        m_IconPlayOK = false;
    bool        m_IconPauseOK = false;
    bool        m_IconSelectOK = false;
    bool        m_IconPanOK = false;
    sf::Texture m_IcoPlay, m_IcoPause, m_IcoSelect, m_IcoPan;

    void EnsureRT(); // crea/ajusta RTs con m_VirtW x m_VirtH

    // Convierte mouse a mundo (si está sobre la imagen)
    std::optional<sf::Vector2f> ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const;

    // Devuelve la entidad bajo worldPos (última dibujada primero)
    EntityID PickEntityAt(const sf::Vector2f& worldPos) const;

    // Dibujo auxiliar
    void DrawSelectionGizmo(sf::RenderTarget& rt) const;
    void DrawGrid(sf::RenderTarget& rt) const;

    // Toolbar con iconos
    bool IconButtonPlayPause();           // ▶ / ⏸
    bool IconButtonSelect(bool active);   // select
    bool IconButtonPan(bool active);      // pan
};
