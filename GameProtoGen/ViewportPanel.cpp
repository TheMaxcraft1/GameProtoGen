#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Renderer2D.h"

#include <imgui.h>
#include <imgui-SFML.h>
#include <cmath> // std::floor

ViewportPanel::ViewportPanel() {}

void ViewportPanel::EnsureRT(unsigned w, unsigned h) {
    if (w == 0 || h == 0) return;
    if (!m_RT || m_RT->getSize().x != w || m_RT->getSize().y != h) {
        m_RT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ w, h });
        m_RT->setSmooth(true);
    }
}

void ViewportPanel::OnUpdate(const gp::Timestep&) {
    // (física / lógica más adelante)
}

void ViewportPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    // 1) Sin padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // 2) Tomar exactamente el área disponible (en píxeles enteros)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    unsigned w = (unsigned)(avail.x > 1 ? floorf(avail.x) : 1);
    unsigned h = (unsigned)(avail.y > 1 ? floorf(avail.y) : 1);

    // 3) Recrear RT si cambió
    EnsureRT(w, h);

    // 4) Dibujar y estampar la textura al tamaño exacto del panel
    if (m_RT) {
        m_RT->clear(sf::Color(30, 30, 35));
        if (SceneContext::Get().scene) {
            Renderer2D::Draw(*SceneContext::Get().scene, *m_RT);
        }
        m_RT->display();
        ImGui::Image(m_RT->getTexture(), avail);
    }
    else {
        ImGui::TextUnformatted("Creando RenderTexture...");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
