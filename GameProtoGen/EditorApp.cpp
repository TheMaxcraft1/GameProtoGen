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

        // --- Semilla de escena (reemplaza el círculo demo) ---
        auto& ctx = SceneContext::Get();
        ctx.scene = std::make_shared<Scene>();
        auto e = ctx.scene->CreateEntity();
        ctx.scene->transforms[e.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
        ctx.scene->sprites[e.id] = Sprite{ {150.f,150.f}, sf::Color(0,255,0,255) };
        ctx.selected = e;
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