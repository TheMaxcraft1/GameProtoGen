#include "Headers/ImGuiLayer.h"
#include "Headers/SFMLWindow.h"

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_internal.h>

ImGuiLayer::ImGuiLayer(gp::SFMLWindow& window)
    : m_Window(window), m_Circle(50.f) {
    m_Circle.setOrigin({ 50.f, 50.f });
    m_Circle.setFillColor(sf::Color::Green);
}

void ImGuiLayer::OnAttach() {
    auto& win = m_Window.Native();
    ImGui::SFML::Init(win);

    m_Window.SetRawEventCallback([this](const void* e) {
        const sf::Event& ev = *static_cast<const sf::Event*>(e);
        ImGui::SFML::ProcessEvent(m_Window.Native(), ev);
    });

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();
    ImFont* roboto = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Regular.ttf", 22.0f, nullptr, ranges);
    io.FontDefault = roboto;
    ImGui::SFML::UpdateFontTexture();
    ImGui::StyleColorsLight();
}

void ImGuiLayer::OnDetach() { ImGui::SFML::Shutdown(); }
 
void ImGuiLayer::EnsureRT(unsigned w, unsigned h) {
    if (w == 0 || h == 0) return;
    if (!m_RT || m_RT->getSize().x != w || m_RT->getSize().y != h) {
        m_RT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ w, h });
        m_RT->setSmooth(true);
    }
}

void ImGuiLayer::OnUpdate(const gp::Timestep&) {
    ImGui::SFML::Update(m_Window.Native(), m_Clock.restart());
}

void ImGuiLayer::OnGuiRender() {
    // Dockspace
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("###MainDockHost", nullptr, hostFlags);
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0), 0);

        if (!m_BuiltDock) {
            m_BuiltDock = true;
            ImGuiIO& io = ImGui::GetIO();
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, 0);
            ImGui::DockBuilderSetNodeSize(dockspace_id, io.DisplaySize);
            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
            ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }
        ImGui::End();
    }

    // Inspector
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    ImGui::Text("Controles del círculo");
    if (ImGui::SliderFloat("Radio", &m_Radius, 10.f, 200.f)) {
        m_Circle.setRadius(m_Radius);
        m_Circle.setOrigin({ m_Radius, m_Radius });
    }
    if (ImGui::ColorEdit3("Color", (float*)&m_Color)) {
        m_Circle.setFillColor(sf::Color(
            int(m_Color.x * 255), int(m_Color.y * 255), int(m_Color.z * 255)
        ));
    }
    if (m_RT) {
        ImGui::Separator();
        ImGui::Text("RT size: %ux%u", m_RT->getSize().x, m_RT->getSize().y);
    }
    ImGui::End();

    // Viewport
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 avail = ImGui::GetContentRegionAvail();
    unsigned vw = (unsigned)std::max(1.0f, avail.x);
    unsigned vh = (unsigned)std::max(1.0f, avail.y);
    EnsureRT(vw, vh);

    if (m_RT) {
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

    // Presentar ImGui sobre la ventana
    ImGui::SFML::Render(m_Window.Native());
}
