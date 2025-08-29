#include "Headers/Application.h"
#include "Headers/SFMLWindow.h"
#include "Headers/ImGuiLayer.h"
#include "Headers/InspectorPanel.h"
#include "Headers/ViewportPanel.h"

class EditorApp : public gp::Application {
public:
    using gp::Application::Application;
    void Setup() {
        auto& win = static_cast<gp::SFMLWindow&>(Window());
        PushLayer(new ImGuiLayer(win));
        PushLayer(new ViewportPanel());
        PushLayer(new InspectorPanel());
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