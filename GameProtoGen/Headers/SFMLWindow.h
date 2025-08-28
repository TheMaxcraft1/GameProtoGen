#pragma once
#include "Application.h"
#include <SFML/Graphics.hpp>

namespace gp {

    class SFMLWindow : public IWindow {
    public:
        explicit SFMLWindow(const WindowProps& props);
        ~SFMLWindow() override = default;
        void SetRawEventCallback(RawEventCallback cb) override { m_RawCb = std::move(cb); }
        void PollEvents() override;
        void SwapBuffers() override;
        void SetEventCallback(EventCallback cb) override { m_Callback = std::move(cb); }
        void* GetNativeHandle() override { return m_Window.getNativeHandle(); }
        uint32_t GetWidth()  const override { return m_Window.getSize().x; }
        uint32_t GetHeight() const override { return m_Window.getSize().y; }

        sf::RenderWindow& Native() { return m_Window; }

    private:
        sf::RenderWindow m_Window;
        EventCallback m_Callback;
        RawEventCallback m_RawCb;
    };

} // namespace gp
