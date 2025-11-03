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
        void SetMaximized(bool on);
        bool IsMaximized() const { return m_Maximized; }
        void SetWindowedSize(unsigned w, unsigned h, int xCenter = -1, int yCenter = -1);
        sf::RenderWindow& Native() { return m_Window; }

    private:
        sf::RenderWindow m_Window;
        EventCallback m_Callback;
        RawEventCallback m_RawCb;
        bool m_Maximized = false;
        sf::Vector2u m_LastWndSize{ 1600, 900 };
        sf::Vector2i m_LastWndPos{ 100, 100 };
    };

} // namespace gp
