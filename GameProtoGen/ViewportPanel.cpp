#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"

#include <imgui.h>
#include <imgui-SFML.h> // para ImGui::Image(sf::Texture,...)

ViewportPanel::ViewportPanel() : m_Circle(50.f) {
    m_Circle.setOrigin({ 50.f, 50.f });
    m_Circle.setFillColor(sf::Color::Green);
}

void ViewportPanel::EnsureRT(unsigned w, unsigned h) {
    if (w == 0 || h == 0) return;
    if (!m_RT || m_RT->getSize().x != w || m_RT->getSize().y != h) {
        m_RT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ w, h });
        m_RT->setSmooth(true);
    }
}

void ViewportPanel::OnUpdate(const gp::Timestep&) {
    // Nada por ahora (cuando metamos ECS/física, irá aquí el render del “juego”)
}

void ViewportPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::Begin("Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    unsigned vw = (unsigned)std::max(1.0f, avail.x);
    unsigned vh = (unsigned)std::max(1.0f, avail.y);
    EnsureRT(vw, vh);

    if (m_RT) {
        // aplicar estado del Inspector
        m_Circle.setRadius(ctx.radius);
        m_Circle.setOrigin({ ctx.radius, ctx.radius });
        m_Circle.setFillColor(sf::Color(
            int(ctx.color.x * 255),
            int(ctx.color.y * 255),
            int(ctx.color.z * 255)
        ));

        m_RT->clear(sf::Color(30, 30, 35));
        m_Circle.setPosition({ vw * 0.5f, vh * 0.5f });
        m_RT->draw(m_Circle);
        m_RT->display();

        ImGui::Image(m_RT->getTexture(), { avail.x, avail.y });
    }
    else {
        ImGui::TextUnformatted("Creando RenderTexture...");
    }
    ImGui::End();
}
