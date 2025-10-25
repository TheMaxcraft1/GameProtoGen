// Editor/ImGuiCoreLayer.cpp
#include "ImGuiCoreLayer.h"
#include "Core/SFMLWindow.h"
#include "EditorFonts.h"
#include <imgui.h>
#include <imgui-SFML.h>

ImGuiCoreLayer::ImGuiCoreLayer(gp::SFMLWindow& window) : m_Window(window) {}

void ImGuiCoreLayer::OnAttach() {
    ImGui::SFML::Init(m_Window.Native());
    m_Window.SetRawEventCallback([this](const void* e) {
        ImGui::SFML::ProcessEvent(m_Window.Native(), *static_cast<const sf::Event*>(e));
        });
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;               // docking habilitado globalmente
    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();
    EditorFonts::Regular = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Regular.ttf", 20.f, nullptr, ranges);
    EditorFonts::H2 = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Bold.ttf", 22.f, nullptr, ranges);
    EditorFonts::H1 = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Bold.ttf", 26.f, nullptr, ranges);
    io.FontDefault = EditorFonts::Regular;
    ImGui::SFML::UpdateFontTexture();
    ImGui::StyleColorsLight();
    m_Initialized = true;
}
void ImGuiCoreLayer::OnDetach() { ImGui::SFML::Shutdown(); }
void ImGuiCoreLayer::OnUpdate(const gp::Timestep&) {
    ImGui::SFML::Update(m_Window.Native(), m_Clock.restart());
}
