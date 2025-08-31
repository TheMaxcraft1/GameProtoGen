#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Renderer2D.h"

#include <imgui.h>
#include <imgui-SFML.h>
#include <cmath>
#include <algorithm> // std::clamp

static inline void ClampDockedMinWidth(float minW) {
    if (ImGui::GetWindowWidth() < minW) {
        ImGui::SetWindowSize(ImVec2(minW, ImGui::GetWindowHeight()));
    }
}

ViewportPanel::ViewportPanel() {}

void ViewportPanel::EnsureRT() {
    if (!m_RT || m_RT->getSize().x != m_VirtW || m_RT->getSize().y != m_VirtH) {
        m_RT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ m_VirtW, m_VirtH });
        m_RT->setSmooth(true);

        // Configurar la View en SFML 3 (sin reset):
        sf::View v;
        // Tamaño de la view (world units = píxeles virtuales)
        v.setSize(sf::Vector2f(static_cast<float>(m_VirtW), static_cast<float>(m_VirtH)));
        // Centro de la view (usa Vector2f, NO dos floats)
        v.setCenter(sf::Vector2f(static_cast<float>(m_VirtW) * 0.5f,
            static_cast<float>(m_VirtH) * 0.5f));
        // (Opcional: viewport de la view en el render target; por defecto [0,0,1,1])
        m_RT->setView(v);
    }
}

void ViewportPanel::OnUpdate(const gp::Timestep&) {
    // lógica/física, si aplica (los sistemas pueden correrse acá)
}

void ViewportPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    // Ventana sin padding + mínimo de ancho real
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ClampDockedMinWidth(480.0f);

    // Medimos área disponible
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Calculamos rectángulo 16:9 máximo que entra (fit-width, fallback a fit-height)
    float targetW = std::floor(avail.x > 1 ? avail.x : 1.0f);
    float targetH = std::floor(targetW * 9.0f / 16.0f);
    if (targetH > avail.y) {
        targetH = std::floor(avail.y > 1 ? avail.y : 1.0f);
        targetW = std::floor(targetH * 16.0f / 9.0f);
    }

    // Creamos (o reusamos) RT de resolución virtual fija
    EnsureRT();

    if (m_RT) {
        // Render a resolución fija (m_VirtW x m_VirtH)
        m_RT->clear(sf::Color(30, 30, 35));

        if (ctx.scene) {
            // -------------------------------
            // Cámara que sigue al jugador
            // -------------------------------
            // 1) Elegimos un "player" preferente:
            //    - si la entidad seleccionada tiene PlayerController -> esa
            //    - si no, tomamos el primero del map playerControllers (fallback)
            EntityID playerId = 0;
            if (ctx.selected && ctx.scene->playerControllers.contains(ctx.selected.id)) {
                playerId = ctx.selected.id;
            }
            else if (!ctx.scene->playerControllers.empty()) {
                playerId = ctx.scene->playerControllers.begin()->first;
            }

            // 2) Centro de cámara: por defecto al centro de la vista
            sf::Vector2f camCenter{ static_cast<float>(m_VirtW) * 0.5f,
                                    static_cast<float>(m_VirtH) * 0.5f };

            if (playerId && ctx.scene->transforms.contains(playerId)) {
                camCenter = ctx.scene->transforms[playerId].position;

                // (Opcional) Clamp para no mostrar más allá del "mundo".
                // En MVP, el “mundo” tiene mismo tamaño que la vista virtual (m_VirtW x m_VirtH).
                // Si tu nivel es más grande, actualizá estos límites a tu world bounds reales.
                const float worldW = static_cast<float>(m_VirtW);
                const float worldH = static_cast<float>(m_VirtH);
                camCenter.x = std::clamp(camCenter.x, worldW * 0.5f, worldW - worldW * 0.5f);
                camCenter.y = std::clamp(camCenter.y, worldH * 0.5f, worldH - worldH * 0.5f);
            }

            // 3) Aplicamos el centro a la view del RenderTexture
            sf::View v = m_RT->getView();
            v.setCenter(camCenter);
            m_RT->setView(v);

            // 4) Dibujamos la escena con la cámara ya ajustada
            Renderer2D::Draw(*ctx.scene, *m_RT);
        }

        m_RT->display();

        // Centrado (letterboxing) en la ventana ImGui
        ImVec2 imgSize{ targetW, targetH };
        ImVec2 cur = ImGui::GetCursorPos();
        ImVec2 offset{ (avail.x - imgSize.x) * 0.5f, (avail.y - imgSize.y) * 0.5f };
        if (offset.x < 0) offset.x = 0;
        if (offset.y < 0) offset.y = 0;
        ImGui::SetCursorPos(ImVec2(cur.x + offset.x, cur.y + offset.y));

        // ¡Escalado! — mostramos la textura virtual al tamaño calculado
        ImGui::Image(m_RT->getTexture(), imgSize);
    }
    else {
        ImGui::TextUnformatted("Creando RenderTexture...");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
