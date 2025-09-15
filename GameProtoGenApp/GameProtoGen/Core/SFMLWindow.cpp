#include "SFMLWindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace gp {

    SFMLWindow::SFMLWindow(const WindowProps& props) {
        m_Window.create(sf::VideoMode({ props.width, props.height }), props.title, sf::Style::Default);
        m_Window.setFramerateLimit(60);
#ifdef _WIN32
        if (props.startMaximized) {
            auto handle = m_Window.getNativeHandle();
            ShowWindow(handle, SW_MAXIMIZE);
        }
#endif
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

} // namespace gp
