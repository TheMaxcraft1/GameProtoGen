#pragma once
#include "Core/Application.h"
#include "ECS/Entity.h"
#include <memory>
#include <optional>
#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <vector>
#include <string>

class ViewportPanel : public gp::Layer {
public:
    ViewportPanel();

    void OnAttach() override;
    void OnUpdate(const gp::Timestep& dt) override;
    void OnGuiRender() override;

private:
    // Render targets
    std::unique_ptr<sf::RenderTexture> m_RT;
    std::unique_ptr<sf::RenderTexture> m_PresentRT;
    sf::Clock m_Clock;

    // Resolución virtual fija (16:9)
    const unsigned m_VirtW = 1600;
    const unsigned m_VirtH = 900;

    // Estado editor / cámara / herramientas
    enum class Tool { Select, Pan };
    Tool m_Tool = Tool::Select;

    bool m_Playing = false;       // arranca en pausa
    bool m_Panning = false;       // pan de cámara en pausa
    bool m_Dragging = false;      // arrastre de entidad en pausa
    EntityID m_DragEntity = 0;
    sf::Vector2f m_DragOffset{ 0.f, 0.f };

    sf::Vector2f m_CamCenter{ 800.f, 450.f };
    float m_Grid = 32.f;          // snap/grilla (sigue activo pero ya no se muestra en UI)

    // Iconos toolbar
    sf::Texture m_IcoPlay, m_IcoPause, m_IcoSelect, m_IcoPan;
    bool m_IconPlayOK = false, m_IconPauseOK = false, m_IconSelectOK = false, m_IconPanOK = false;

    // ---------- Consola ----------
    std::vector<std::string> m_Log;  // líneas
    bool m_AutoScroll = true;

private:
    void EnsureRT();

    // Picking / utilidades
    std::optional<sf::Vector2f> ScreenToWorld(ImVec2 mouse, ImVec2 imgMin, ImVec2 imgMax) const;
    EntityID PickEntityAt(const sf::Vector2f& worldPos) const;

    // Gizmos / dibujo
    void DrawSelectionGizmo(sf::RenderTarget& rt) const;
    void DrawGrid(sf::RenderTarget& rt) const;

    // Toolbar
    bool IconButtonPlayPause();
    bool IconButtonSelect(bool active);
    bool IconButtonPan(bool active);

    // Consola
    void AppendLog(const std::string& line);
    void DrawConsole(float height); // dibuja la consola al final del panel
};
