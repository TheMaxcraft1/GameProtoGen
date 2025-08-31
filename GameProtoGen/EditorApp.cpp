#include "Headers/Application.h"
#include "Headers/SFMLWindow.h"
#include "Headers/ImGuiLayer.h"
#include "Headers/InspectorPanel.h"
#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Scene.h"
#include "Headers/Components.h"

class EditorApp : public gp::Application {
public:
    using gp::Application::Application;
    void Setup() {
        auto& win = static_cast<gp::SFMLWindow&>(Window());
        PushLayer(new ImGuiLayer(win));
        PushLayer(new ViewportPanel());
        PushLayer(new InspectorPanel());

        // Semilla
        auto& ctx = SceneContext::Get();
        ctx.scene = std::make_shared<Scene>();
        auto e = ctx.scene->CreateEntity();

        // Transform + Sprite
        ctx.scene->transforms[e.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
        ctx.scene->sprites[e.id] = Sprite{ {80.f,120.f}, sf::Color(0,255,0,255) };

        // Collider (caja aprox al sprite)
        ctx.scene->colliders[e.id] = Collider{ {40.f,60.f}, {0.f,0.f} };

        // Physics + PlayerController
        ctx.scene->physics[e.id] = Physics2D{};
        ctx.scene->playerControllers[e.id] = PlayerController{ 500.f, 900.f };

        ctx.selected = e;

        // (Opcional) Agregá una “plataforma” estática:
        auto ground = ctx.scene->CreateEntity();
        ctx.scene->transforms[ground.id] = Transform{ {800.f, 820.f}, {1.f,1.f}, 0.f };
        ctx.scene->sprites[ground.id] = Sprite{ {1600.f, 160.f}, sf::Color(60,60,70,255) };
        ctx.scene->colliders[ground.id] = Collider{ {800.f, 80.f}, {0.f,0.f} };
    }
};

int main() {
    gp::WindowProps props{ "GameProtoGen + ImGui", 1600, 900, true };
    EditorApp app(props);
    app.Setup();
    app.Run();
}


// TODO: Uncomment the following lines to use WinMain instead of main. This makes the application a Windows GUI app instead of a console app.
//int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
//    return main();
//}