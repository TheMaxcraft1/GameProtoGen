#include "Headers/Application.h"
#include "Headers/SFMLWindow.h"
#include "Headers/ImGuiLayer.h"
#include "Headers/InspectorPanel.h"
#include "Headers/ViewportPanel.h"
#include "Headers/SceneContext.h"
#include "Headers/Scene.h"
#include "Headers/Components.h"
#include "Headers/ApiClient.h"
#include "Headers/ChatPanel.h"
#include "Headers/SceneSerializer.h"
#include <filesystem>

// Helper: asegurar que hay un jugador “jugable”
static void EnsurePlayable(Scene& scene, Entity& outSelected) {
    EntityID playerId = 0;

    // ¿Ya hay alguno?
    if (!scene.playerControllers.empty()) {
        playerId = scene.playerControllers.begin()->first;
    }

    // Si no hay ninguno, elegimos una entidad existente o creamos una nueva
    if (!playerId) {
        Entity chosen{};
        // Tomar la primera entidad que tenga Transform (si existe)
        for (auto& e : scene.Entities()) {
            if (scene.transforms.contains(e.id)) {
                chosen = e;
                break;
            }
        }
        // Si no había ninguna entidad útil, crear una
        if (!chosen) {
            chosen = scene.CreateEntity();
            scene.transforms[chosen.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
            scene.sprites[chosen.id] = Sprite{ {80.f,120.f}, sf::Color(0,255,0,255) };
            scene.colliders[chosen.id] = Collider{ {40.f,60.f}, {0.f,0.f} };
        }

        // Asegurar componentes de juego
        if (!scene.physics.contains(chosen.id)) {
            scene.physics[chosen.id] = Physics2D{};
        }
        if (!scene.playerControllers.contains(chosen.id)) {
            scene.playerControllers[chosen.id] = PlayerController{ 500.f, 900.f };
        }
        playerId = chosen.id;
    }

    outSelected = Entity{ playerId };
}

// Helper: si no hay “suelo”, creamos una plataforma base
static void EnsureGround(Scene& scene) {
    bool hasStaticCollider = false;
    for (auto& [id, _] : scene.colliders) {
        if (!scene.physics.contains(id)) { hasStaticCollider = true; break; }
    }
    if (!hasStaticCollider) {
        auto ground = scene.CreateEntity();
        scene.transforms[ground.id] = Transform{ {800.f, 820.f}, {1.f,1.f}, 0.f };
        scene.sprites[ground.id] = Sprite{ {1600.f, 160.f}, sf::Color(60,60,70,255) };
        scene.colliders[ground.id] = Collider{ {800.f, 80.f}, {0.f,0.f} }; // estático (sin Physics2D)
    }
}

class EditorApp : public gp::Application {
public:
    using gp::Application::Application;
    void Setup() {
        auto& win = static_cast<gp::SFMLWindow&>(Window());
        PushLayer(new ImGuiLayer(win));
        PushLayer(new ViewportPanel());
        PushLayer(new InspectorPanel());
        auto client = std::make_shared<ApiClient>("127.0.0.1", 5199);
        client->SetTimeouts(2, 5, 5);
        PushLayer(new ChatPanel(client));

        // Semilla / Carga de proyecto
        auto& ctx = SceneContext::Get();
        ctx.scene = std::make_shared<Scene>();

        bool loaded = false;
        const char* kProjPath = "Saves/project.json";
        if (std::filesystem::exists(kProjPath)) {
            loaded = SceneSerializer::Load(*ctx.scene, kProjPath);
        }

        if (!loaded) {
            // Escena base si no hay archivo
            auto e = ctx.scene->CreateEntity();
            ctx.scene->transforms[e.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
            ctx.scene->sprites[e.id] = Sprite{ {80.f,120.f}, sf::Color(0,255,0,255) };
            ctx.scene->colliders[e.id] = Collider{ {40.f,60.f}, {0.f,0.f} };
            ctx.scene->physics[e.id] = Physics2D{};
            ctx.scene->playerControllers[e.id] = PlayerController{ 500.f, 900.f };
            ctx.selected = e;

            // Plataforma “suelo”
            auto ground = ctx.scene->CreateEntity();
            ctx.scene->transforms[ground.id] = Transform{ {800.f, 820.f}, {1.f,1.f}, 0.f };
            ctx.scene->sprites[ground.id] = Sprite{ {1600.f, 160.f}, sf::Color(60,60,70,255) };
            ctx.scene->colliders[ground.id] = Collider{ {800.f, 80.f}, {0.f,0.f} };
        }
        else {
            // Auto-curación: asegurar jugador y suelo aunque el JSON sea viejo
            EnsureGround(*ctx.scene);
            EnsurePlayable(*ctx.scene, ctx.selected);
        }
    }
};

int main() {
    gp::WindowProps props{ "GameProtoGen + ImGui", 1600, 900, true };
    EditorApp app(props);
    app.Setup();
    app.Run();
}

// // Si querés WinMain:
// // int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return main(); }
