#include "SFMLWindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace gp {

    SFMLWindow::SFMLWindow(const WindowProps& props) {
        m_Window.create(sf::VideoMode({ props.width, props.height }),
            props.title, sf::Style::Default);
        m_Window.setFramerateLimit(60);
#ifdef _WIN32
        if (props.startMaximized) {
            auto handle = m_Window.getNativeHandle();
            ShowWindow(handle, SW_MAXIMIZE);
            m_Maximized = true;
        }
        else
#endif
        {
            m_Window.setPosition({ 100, 100 });
            m_LastWndSize = { props.width, props.height };
            m_LastWndPos = { 100, 100 };
        }
    }

    void SFMLWindow::PollEvents() {
        while (auto ev = m_Window.pollEvent()) {

            if (m_RawCb) m_RawCb(&*ev);

            if (ev->is<sf::Event::Closed>()) {
                if (m_Callback) m_Callback(Event{ Event::Type::Closed });
            }
            else if (auto* r = ev->getIf<sf::Event::Resized>()) {
                if (m_Callback) m_Callback(Event{ Event::Type::Resized, r->size.x, r->size.y });
            }
        }
    }

    void SFMLWindow::SwapBuffers() {
        // El render de ImGui se hace fuera; aquí sólo mostramos y limpiamos para el siguiente frame
        m_Window.display();
        m_Window.clear(sf::Color::Black);
    }

    IWindow* CreateAppWindow(const WindowProps& props) {
        return new SFMLWindow(props);
    }

    void SFMLWindow::SetMaximized(bool on) {
        if (m_Maximized == on) return;
#ifdef _WIN32
        auto handle = m_Window.getNativeHandle();
        if (on) {
            // Guardar tamaño/pos de ventana actual antes de maximizar
            m_LastWndSize = m_Window.getSize();
            m_LastWndPos = m_Window.getPosition();
            ShowWindow(handle, SW_MAXIMIZE);
        }
        else {
            ShowWindow(handle, SW_RESTORE);
            // Restaurar tamaño/pos previos
            m_Window.setSize(m_LastWndSize);
            m_Window.setPosition(m_LastWndPos);
        }
        m_Maximized = on;
#else
        // Fallback multiplataforma (no “true maximize”, pero visualmente ok)
        if (on) {
            m_LastWndSize = m_Window.getSize();
            m_LastWndPos = m_Window.getPosition();
            auto dm = sf::VideoMode::getDesktopMode();
            m_Window.setSize({ dm.width, dm.height });
            m_Window.setPosition({ 0, 0 });
        }
        else {
            m_Window.setSize(m_LastWndSize);
            m_Window.setPosition(m_LastWndPos);
        }
        m_Maximized = on;
#endif
    }

    void SFMLWindow::SetWindowedSize(unsigned w, unsigned h, int xCenter, int yCenter) {
        // Forzá modo ventana “chico” (útil para Hub)
        m_Window.setSize({ w, h });
        auto dm = sf::VideoMode::getDesktopMode();
        int x = (xCenter >= 0) ? xCenter : (int(dm.size.x) - (int)w) / 2;
        int y = (yCenter >= 0) ? yCenter : (int(dm.size.y) - (int)h) / 2;
        m_Window.setPosition({ x, y });
        m_Maximized = false;
        m_LastWndSize = { w, h };
        m_LastWndPos = { x, y };
    }

}
