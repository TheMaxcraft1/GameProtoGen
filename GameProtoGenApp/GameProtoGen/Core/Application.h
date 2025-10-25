#pragma once
#include <vector>
#include <functional>
#include <cstdint>

namespace gp {

    struct Timestep { float dt; };

    struct Event {
        enum class Type { Closed, Resized, Unknown } type = Type::Unknown;
        uint32_t w = 0, h = 0;
    };

    class Layer {
    public:
        virtual ~Layer() = default;
        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate(const Timestep&) {}
        virtual void OnGuiRender() {}
    };

    class IWindow {
    public:
        using EventCallback = std::function<void(const Event&)>;
        using RawEventCallback = std::function<void(const void* /*sf::Event*/)>;
        virtual ~IWindow() = default;

        // loop
        virtual void PollEvents() = 0;
        virtual void SwapBuffers() = 0;

        // callbacks
        virtual void SetEventCallback(EventCallback cb) = 0;
        virtual void SetRawEventCallback(RawEventCallback cb) = 0;

        // native
        virtual void* GetNativeHandle() = 0;
        virtual uint32_t GetWidth()  const = 0;
        virtual uint32_t GetHeight() const = 0;
    };

    struct WindowProps {
        const char* title = "GameProtoGen";
        uint32_t width = 1600, height = 900;
        bool startMaximized = true;
    };

    IWindow* CreateAppWindow(const WindowProps& props);

    class Application {
    public:
        explicit Application(const WindowProps& props);
        virtual ~Application();
        void Run();
        void PushLayer(Layer* layer);
        void PopLayer(Layer* layer);
        IWindow& Window() { return *m_Window; }

        // NUEVO: singleton simple para que las layers puedan pedir salir
        static Application& Get() { return *s_Instance; }

        // NUEVO: flujo de cierre con confirmación
        void RequestClose() { m_WantsClose = true; }
        void CancelClose() { m_WantsClose = false; }
        bool WantsClose() const { return m_WantsClose; }
        void QuitNow() { m_Running = false; } // cierra el main loop

    private:
        bool m_Running = true;
        bool m_WantsClose = false; // NUEVO
        IWindow* m_Window = nullptr;
        std::vector<Layer*> m_Layers;
        void OnEvent(const Event& e);
        void FlushPending();
        std::vector<Layer*> m_PendingRemove;

        static Application* s_Instance; // NUEVO
    };


} // namespace gp