#include "Headers/ImGuiLayer.h"
#include "Headers/SFMLWindow.h"
#include "Headers/EditorFonts.h" 

#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_internal.h>

ImGuiLayer::ImGuiLayer(gp::SFMLWindow& window) : m_Window(window) {}

void ImGuiLayer::OnAttach() {
    auto& win = m_Window.Native();
    ImGui::SFML::Init(win);

    m_Window.SetRawEventCallback([this](const void* e) {
        const sf::Event& ev = *static_cast<const sf::Event*>(e);
        ImGui::SFML::ProcessEvent(m_Window.Native(), ev);
        });

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // ---------- Fuentes ----------
    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();

    // Regular (tamaño base del editor)
    EditorFonts::Regular = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Regular.ttf", 20.0f, nullptr, ranges);

    // H2 (título secciones) - Bold mediano
    // Si no tenés Roboto-Bold.ttf, podés repetir Regular a mayor tamaño, pero no será "negrita" real.
    EditorFonts::H2 = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Bold.ttf", 22.0f, nullptr, ranges);

    // H1 (headers grandes) - Bold grande
    EditorFonts::H1 = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Bold.ttf", 26.0f, nullptr, ranges);

    // Fuente por defecto
    io.FontDefault = EditorFonts::Regular;
    ImGui::SFML::UpdateFontTexture();

    ImGui::StyleColorsLight();
}

void ImGuiLayer::OnDetach() { ImGui::SFML::Shutdown(); }

void ImGuiLayer::OnUpdate(const gp::Timestep&) {
    ImGui::SFML::Update(m_Window.Native(), m_Clock.restart());
}

void ImGuiLayer::OnGuiRender() {
    // Sólo DockSpace host
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
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
            dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);
        ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
        ImGui::DockBuilderDockWindow("Chat", dock_right_id);
        ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        ImGui::DockBuilderFinish(dockspace_id);
    }
    ImGui::End();

    // ⚠️ No hagas Render aquí.
}
