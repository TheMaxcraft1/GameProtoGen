#include "Application.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include "SFMLWindow.h"
#include <chrono>

namespace gp {

    Application* Application::s_Instance = nullptr;

    Application::Application(const WindowProps& props) {
        s_Instance = this;
        m_Window = CreateAppWindow(props);
        m_Window->SetEventCallback([this](const Event& e) { OnEvent(e); });
    }

    Application::~Application() {
        for (auto it = m_Layers.rbegin(); it != m_Layers.rend(); ++it) (*it)->OnDetach();
        delete m_Window;
    }

    void Application::Run() {
        using clock = std::chrono::high_resolution_clock;
        auto last = clock::now();

        for (auto* l : m_Layers) l->OnAttach();

        while (m_Running) {
            m_Window->PollEvents();

            auto now = clock::now();
            float dt = std::chrono::duration<float>(now - last).count();
            last = now;

            // 1) Update (abre el frame)
            for (auto* l : m_Layers) l->OnUpdate(Timestep{ dt });

            // 2) Dibujar ventanas
            for (auto* l : m_Layers) l->OnGuiRender();

            // 3) Renderizar ImGui al final del frame
            ImGui::SFML::Render(static_cast<gp::SFMLWindow&>(*m_Window).Native());

            // 4) Presentar
            m_Window->SwapBuffers();
        }
    }

    void Application::PushLayer(Layer* layer) {
        m_Layers.emplace_back(layer);
    }

    void Application::OnEvent(const Event& e) {
        if (e.type == Event::Type::Closed) {
            // En vez de salir ya, pedimos confirmación
            m_WantsClose = true;
        }
        // más adelante podés propagar eventos a las layers si querés
    }

} // namespace gp
