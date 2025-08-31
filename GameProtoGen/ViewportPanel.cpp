#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Renderer2D.h"
#include "Headers/PhysicsSystem.h"

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
    const bool needRecreate =
        !m_RT ||
        m_RT->getSize().x != m_VirtW ||
        m_RT->getSize().y != m_VirtH;

    if (needRecreate) {
        // RT principal
        m_RT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ m_VirtW, m_VirtH });
        m_RT->setSmooth(true);
        sf::View v;
        v.setSize(sf::Vector2f(static_cast<float>(m_VirtW), static_cast<float>(m_VirtH)));
        v.setCenter(sf::Vector2f(static_cast<float>(m_VirtW) * 0.5f,
            static_cast<float>(m_VirtH) * 0.5f));
        m_RT->setView(v);

        // RT de presentación (para flip vertical)
        m_PresentRT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ m_VirtW, m_VirtH });
        m_PresentRT->setSmooth(true);
        sf::View pv;
        pv.setSize(sf::Vector2f(static_cast<float>(m_VirtW), static_cast<float>(m_VirtH)));
        pv.setCenter(sf::Vector2f(static_cast<float>(m_VirtW) * 0.5f,
            static_cast<float>(m_VirtH) * 0.5f));
        m_PresentRT->setView(pv);
    }
}

void ViewportPanel::OnUpdate(const gp::Timestep& dt) {
    auto& ctx = SceneContext::Get();
    if (!ctx.scene) return;

    // 1) Input jugador
    Systems::PlayerControllerSystem::Update(*ctx.scene, dt.dt);

    // 2) Física
    Systems::PhysicsSystem::Update(*ctx.scene, dt.dt);

    // 3) Colisiones (suelo plano + AABB con estáticos)
    Systems::CollisionSystem::SolveGround(*ctx.scene, /*groundY*/ 900.f); // borde inferior del “mundo” 16:9
    Systems::CollisionSystem::SolveAABB(*ctx.scene);

    // (Si querés, podés hacer post-procesos aquí)
}

void ViewportPanel::OnGuiRender() {
    auto& ctx = SceneContext::Get();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ClampDockedMinWidth(480.0f);

    ImVec2 avail = ImGui::GetContentRegionAvail();

    float targetW = std::floor(avail.x > 1 ? avail.x : 1.0f);
    float targetH = std::floor(targetW * 9.0f / 16.0f);
    if (targetH > avail.y) {
        targetH = std::floor(avail.y > 1 ? avail.y : 1.0f);
        targetW = std::floor(targetH * 16.0f / 9.0f);
    }

    EnsureRT();

    if (m_RT && m_PresentRT) {
        // 1) Dibujar escena en RT principal
        m_RT->clear(sf::Color(30, 30, 35));
        if (ctx.scene) {
            // Cámara
            EntityID playerId = 0;
            if (ctx.selected && ctx.scene->playerControllers.contains(ctx.selected.id)) {
                playerId = ctx.selected.id;
            }
            else if (!ctx.scene->playerControllers.empty()) {
                playerId = ctx.scene->playerControllers.begin()->first;
            }

            sf::Vector2f camCenter{ static_cast<float>(m_VirtW) * 0.5f,
                                    static_cast<float>(m_VirtH) * 0.5f };

            if (playerId && ctx.scene->transforms.contains(playerId)) {
                camCenter = ctx.scene->transforms[playerId].position;
                //const float worldW = static_cast<float>(m_VirtW);
                //const float worldH = static_cast<float>(m_VirtH);
                //camCenter.x = std::clamp(camCenter.x, worldW * 0.5f, worldW - worldW * 0.5f);
                //camCenter.y = std::clamp(camCenter.y, worldH * 0.5f, worldH - worldH * 0.5f);
            }

            sf::View v = m_RT->getView();
            v.setCenter(camCenter);
            m_RT->setView(v);

            Renderer2D::Draw(*ctx.scene, *m_RT);
        }
        m_RT->display();

        // 2) Espejar verticalmente en el RT de presentación
        m_PresentRT->clear(sf::Color::Black);
        sf::Sprite spr(m_RT->getTexture());
        spr.setScale(sf::Vector2f{ 1.f, -1.f });                  
        spr.setPosition(sf::Vector2f{ 0.f, static_cast<float>(m_VirtH) });          
        m_PresentRT->draw(spr);
        m_PresentRT->display();

        // 3) Letterboxing y muestra
        ImVec2 imgSize{ targetW, targetH };
        ImVec2 cur = ImGui::GetCursorPos();
        ImVec2 offset{ (avail.x - imgSize.x) * 0.5f, (avail.y - imgSize.y) * 0.5f };
        if (offset.x < 0) offset.x = 0;
        if (offset.y < 0) offset.y = 0;
        ImGui::SetCursorPos(ImVec2(cur.x + offset.x, cur.y + offset.y));

        ImGui::Image(m_PresentRT->getTexture(), imgSize);
    }
    else {
        ImGui::TextUnformatted("Creando RenderTextures...");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
