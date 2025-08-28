#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <windows.h>
#include <SFML/Window/Window.hpp>

#include <cstdint>
#include <algorithm>
#include <memory>

#include <imgui.h>
#include <imgui-SFML.h>   // 👈 backend ImGui para SFML
#include <imgui_internal.h>

int main() {
    // Ventana "maximizada" (tamaño del escritorio, con bordes)
    sf::RenderWindow window(sf::VideoMode::getDesktopMode(), "GameProtoGen + ImGui", sf::Style::Default);

#ifdef _WIN32
    // Solo en Windows: forzar ventana maximizada
	sf::WindowHandle handle = window.getNativeHandle();
    ShowWindow(handle, SW_MAXIMIZE);
#endif

    window.setFramerateLimit(60);

    // Init ImGui
    ImGui::SFML::Init(window);
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Rango de caracteres: incluye acentos, ñ, etc.
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesDefault();

    ImFont* roboto = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Regular.ttf", 22.0f, nullptr, glyphRanges);
    io.FontDefault = roboto;
	ImGui::SFML::UpdateFontTexture();
    //io.FontGlobalScale = 1.6f;
    ImGui::StyleColorsLight();

    // Estado del "juego"
    float radius = 50.f;
    ImVec4 color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

    sf::CircleShape circle(radius);
    circle.setOrigin({ radius, radius });
    circle.setFillColor(sf::Color::Green);

    // RenderTexture para el Viewport (usar puntero porque no hay .create())
    std::unique_ptr<sf::RenderTexture> gameRT;

    auto ensureRT = [&](unsigned w, unsigned h) {
        if (w == 0 || h == 0) return;
        if (!gameRT || gameRT->getSize().x != w || gameRT->getSize().y != h) {
            gameRT = std::make_unique<sf::RenderTexture>(sf::Vector2u{ w, h });
            gameRT->setSmooth(true);
        }
        };

    bool builtDock = false;

    sf::Clock deltaClock;
    while (window.isOpen()) {
        while (auto ev = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *ev);
            if (ev->is<sf::Event::Closed>()) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // ---------- DockSpace fullscreen ----------
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::SetNextWindowViewport(vp->ID);

            ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking
                | ImGuiWindowFlags_NoTitleBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin("###MainDockHost", nullptr, hostFlags);
            ImGui::PopStyleVar(2);

            ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
            ImGuiDockNodeFlags dockFlags = 0; // sin flags internos
            ImGui::DockSpace(dockspace_id, ImVec2(0, 0), dockFlags);

            if (!builtDock) {
                builtDock = true;
                // Builder API (necesita imgui_internal.h)
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, 0);
                ImGui::DockBuilderSetNodeSize(dockspace_id, io.DisplaySize);

                ImGuiID dock_main_id = dockspace_id;
                ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
                    dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);

                // Dockear ventanas por nombre
                ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
                ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
                ImGui::DockBuilderFinish(dockspace_id);
            }

            ImGui::End(); // ###MainDockHost
        }

        // ---------- Inspector (derecha) ----------
        ImGui::Begin("Inspector", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::Text("Controles del círculo");

        if (ImGui::SliderFloat("Radio", &radius, 10.f, 200.f)) {
            circle.setRadius(radius);
            circle.setOrigin({ radius, radius });
        }

        if (ImGui::ColorEdit3("Color", (float*)&color)) {
            circle.setFillColor(sf::Color(
                static_cast<int>(color.x * 255),
                static_cast<int>(color.y * 255),
                static_cast<int>(color.z * 255)
            ));
        }

        if (gameRT) {
            ImGui::Separator();
            ImGui::Text("RT size: %ux%u", gameRT->getSize().x, gameRT->getSize().y);
        }
        ImGui::End();

        // ---------- Viewport (centro) ----------
        ImGui::Begin("Viewport", nullptr,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImVec2 avail = ImGui::GetContentRegionAvail();
        unsigned vw = (unsigned)std::max(1.0f, avail.x);
        unsigned vh = (unsigned)std::max(1.0f, avail.y);
        ensureRT(vw, vh);

        // Dibujar en el RenderTexture (si existe)
        if (gameRT) {
            gameRT->clear(sf::Color(30, 30, 35));
            circle.setPosition({ vw * 0.5f, vh * 0.5f });
            gameRT->draw(circle);
            gameRT->display();

            // imgui-SFML permite pasar sf::Texture directamente
            ImGui::Image(gameRT->getTexture(), { avail.x, avail.y });
        }
        else {
            ImGui::TextUnformatted("Creando RenderTexture...");
        }

        ImGui::End(); // Viewport

        // ---------- Presentación ----------
        window.clear(sf::Color::Black);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}

// TODO: Uncomment the following lines to use WinMain instead of main. This makes the application a Windows GUI app instead of a console app.
//int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
//    return main();
//}