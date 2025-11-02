// GameProtoGenApp/GameProtoGen/Editor/EditorApp.cpp
#include "Core/Application.h"
#include "Core/SFMLWindow.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Panels/ChatPanel.h"
#include "Runtime/SceneContext.h"
#include "Runtime/EditorContext.h"
#include "ECS/Scene.h"
#include "ECS/Components.h"
#include "ECS/SceneSerializer.h"
#include "Net/ApiClient.h"
#include <filesystem>
#include <Systems/Renderer2D.h>
#include <Auth/TokenManager.h>
#include "Auth/OidcClient.h"
#include "Editor/LauncherLayer.h"
#include "Editor/ImGuiCoreLayer.h"
#include "Core/Log.h"
#include "Editor/EditorLogSink.h"

// Helper: asegurar que hay un jugador “jugable”
static void EnsurePlayable(Scene& scene, Entity& outSelected) {
    EntityID playerId = 0;

    // ¿Ya hay alguno?
    if (!scene.playerControllers.empty()) {
        playerId = scene.playerControllers.begin()->first;
        Renderer2D::ClearTextureCache();
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
        PushLayer(new ImGuiCoreLayer(win));  // <— en Hub también, pero sin dock

        static ImGuiConsoleSink s_sink;
        Log::SetSink(&s_sink);

        // ApiClient (HTTPS con tu backend en 7223)
        auto client = std::make_shared<ApiClient>("https://localhost:7223");
        client->SetVerifySsl(true);
        client->UseHttps(true);
        client->SetTimeouts(10, 180, 30);

        // Split de contextos:
        // - EditorContext: auth, API, selección, flags de play
        // - SceneContext: solo escena/cámara (runtime-friendly)
        auto& edx = EditorContext::Get();
        auto& scx = SceneContext::Get();

        edx.apiClient = client;
        scx.scene.reset(); // la escena se decide en el launcher

        // TokenManager precargado (si ya hay tokens guardados)
        OidcConfig cfg;
        cfg.client_id = "2041dbc5-c266-43aa-af66-765b1440f34a";
        cfg.authorize_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/authorize";
        cfg.token_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/token";
        cfg.scopes = { "openid","profile","offline_access","api://gameprotogen/access_as_user" };

        edx.tokenManager = std::make_shared<TokenManager>(cfg);
        if (edx.tokenManager->Load()) {
            edx.apiClient->SetTokenRefresher([mgr = edx.tokenManager]() -> std::optional<std::string> {
                return mgr->Refresh();
                });
            edx.apiClient->SetPreflight([mgr = edx.tokenManager]() { (void)mgr->EnsureFresh(); });
            if (!edx.tokenManager->AccessToken().empty())
                edx.apiClient->SetAccessToken(edx.tokenManager->AccessToken());
            edx.tokenManager->EnsureFresh();
        }

        // Importante: arrancamos con el Launcher (no montes Viewport/Inspector/Chat acá)
        gp::Application::Get().SetMode(gp::Application::Mode::Hub);
        PushLayer(new LauncherLayer());
        win.SetMaximized(false);
        win.SetWindowedSize(1024, 640); // centrado por defecto
    }
};

int main() {
    gp::WindowProps props{ "Game Protogen", 1600, 900, true };
    EditorApp app(props);
    app.Setup();
    app.Run();
}

// // Si querés WinMain:
// // int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return main(); }
