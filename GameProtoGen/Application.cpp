#include "Headers/Application.h"
#include <chrono>

namespace gp {

    Application::Application(const WindowProps& props) {
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

            for (auto* l : m_Layers) l->OnUpdate(Timestep{ dt });
            for (auto* l : m_Layers) l->OnGuiRender();

            m_Window->SwapBuffers();
        }
    }

    void Application::PushLayer(Layer* layer) { m_Layers.emplace_back(layer); }

    void Application::OnEvent(const Event& e) {
        if (e.type == Event::Type::Closed) m_Running = false;
    }

} // namespace gp
