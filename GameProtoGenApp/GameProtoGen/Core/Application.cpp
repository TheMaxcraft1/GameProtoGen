#include "Application.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include "SFMLWindow.h"
#include <chrono>
#include <algorithm>

namespace gp {

    Application* Application::s_Instance = nullptr;

    Application::Application(const WindowProps& props) {
        s_Instance = this;
        m_Window = CreateAppWindow(props);
        m_Window->SetEventCallback([this](const Event& e) { OnEvent(e); });
    }

    Application::~Application() {
        // Detach en orden inverso de lo que quede
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) (*it)->OnDetach();
        delete m_Window;
    }

    void Application::Run() {
        using clock = std::chrono::high_resolution_clock;
        auto last = clock::now();

        // Attach inicial
        for (auto* l : m_Layers) l->OnAttach();

        // Aseguramos contexto activo y hacemos 1er frame de ImGui-SFML
        auto& win = static_cast<gp::SFMLWindow&>(*m_Window).Native();
        win.setActive(true);

        while (m_Running) {
            m_Window->PollEvents();
            FlushPending();

            if (m_WantsClose && GetMode() == Mode::Hub) {
                QuitNow();
                break;
            }

            auto now = clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            // --- UPDATE ---
            for (auto* l : m_Layers) l->OnUpdate(Timestep{ dt });

            // ImGui necesita la ventana ACTIVA antes de Update/Render
            win.setActive(true);
            ImGui::SFML::Update(win, sf::seconds(dt));

            // --- GUI ---
            for (auto* l : m_Layers) l->OnGuiRender();

            // --- RENDER ImGui + Present ---
            ImGui::SFML::Render(win);
            m_Window->SwapBuffers();
        }

        // Shutdown ordenado
        win.setActive(true);
        ImGui::SFML::Shutdown(win);
        ImGui::DestroyContext();
    }

    void Application::PushLayer(Layer* layer) {
        m_Layers.emplace_back(layer);
        // Si querés OnAttach inmediato para las nuevas:
        layer->OnAttach();
    }

    void Application::PopLayer(Layer* layer) {   // <-- NUEVO
        // No removemos directo para no invalidar iteradores si se llama
        // desde OnGuiRender/OnUpdate. Lo encolamos.
        if (!layer) return;
        m_PendingRemove.push_back(layer);
    }

    void Application::FlushPending() {           // <-- NUEVO
        if (m_PendingRemove.empty()) return;

        for (Layer* doomed : m_PendingRemove) {
            auto it = std::find(m_Layers.begin(), m_Layers.end(), doomed);
            if (it != m_Layers.end()) {
                (*it)->OnDetach();
                m_Layers.erase(it);
            }
        }
        m_PendingRemove.clear();
    }

    void Application::OnEvent(const Event& e) {
        if (e.type == Event::Type::Closed) {
            m_WantsClose = true;
        }
    }

} // namespace gp
