#include "LauncherLayer.h"
#include "Runtime/SceneContext.h"
#include "ECS/Scene.h"
#include "ECS/SceneSerializer.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ChatPanel.h"
#include "Systems/Renderer2D.h"
#include "Auth/OidcClient.h"
#include "Auth/TokenManager.h"
#include "Net/ApiClient.h"
#include "Editor/EditorDockLayer.h"

#include <imgui.h>
#include <filesystem>

using std::filesystem::exists;
using std::filesystem::directory_iterator;

static void EnsureSavesDir() {
    std::error_code ec;
    std::filesystem::create_directories("Saves", ec);
    (void)ec;
}

void LauncherLayer::OnAttach() {
    EnsureSavesDir();
}

void LauncherLayer::DoLoginInteractive() {
    auto& ctx = SceneContext::Get();
    if (!ctx.apiClient) return;

    OidcConfig cfg;
    cfg.client_id = "2041dbc5-c266-43aa-af66-765b1440f34a";
    cfg.authorize_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/authorize";
    cfg.token_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/token";
    cfg.scopes = { "openid","profile","offline_access","api://gameprotogen/access_as_user" };

    OidcClient oidc(cfg);
    std::string err;
    auto tokens = oidc.AcquireTokenInteractive(&err);
    if (!tokens) {
        // Podrías mostrar un popup si querés
        return;
    }

    if (!ctx.tokenManager) ctx.tokenManager = std::make_shared<TokenManager>(cfg);
    ctx.tokenManager->OnInteractiveLogin(*tokens);

    ctx.apiClient->SetAccessToken(tokens->access_token);
    ctx.apiClient->SetTokenRefresher([mgr = ctx.tokenManager]() -> std::optional<std::string> {
        return mgr->Refresh();
        });
    ctx.apiClient->SetPreflight([mgr = ctx.tokenManager]() { (void)mgr->EnsureFresh(); });

    m_loggedIn = true;
}

void LauncherLayer::DrawProjectPicker() {
    ImGui::TextUnformatted("Elegí un proyecto local (Saves/*.json):");
    if (!exists("Saves")) {
        ImGui::TextDisabled("No existe la carpeta Saves.");
        return;
    }
    bool any = false;
    for (auto& p : directory_iterator("Saves")) {
        if (p.is_regular_file() && p.path().extension() == ".json") {
            any = true;
            bool selected = (m_selected == p.path().string());
            if (ImGui::Selectable(p.path().filename().string().c_str(), selected)) {
                m_selected = p.path().string();
            }
        }
    }
    if (!any) ImGui::TextDisabled("No hay .json en Saves todavía.");
}

void LauncherLayer::SeedNewScene() {
    auto& ctx = SceneContext::Get();
    ctx.scene = std::make_shared<Scene>();
    auto& s = *ctx.scene;

    // jugador base
    auto e = s.CreateEntity();
    s.transforms[e.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
    s.sprites[e.id] = Sprite{ {80.f,120.f}, sf::Color(0,255,0,255) };
    s.colliders[e.id] = Collider{ {40.f,60.f}, {0.f,0.f} };
    s.physics[e.id] = Physics2D{};
    s.playerControllers[e.id] = PlayerController{ 500.f, 900.f };

    // suelo
    auto ground = s.CreateEntity();
    s.transforms[ground.id] = Transform{ {800.f, 820.f}, {1.f,1.f}, 0.f };
    s.sprites[ground.id] = Sprite{ {1600.f, 160.f}, sf::Color(60,60,70,255) };
    s.colliders[ground.id] = Collider{ {800.f, 80.f}, {0.f,0.f} };

    // opcional: script en entidad 3
    {
        EntityID target = 3;
        bool exists3 = false;
        for (auto& ent : s.Entities()) if (ent.id == target) { exists3 = true; break; }
        if (!exists3) {
            Entity e3 = s.CreateEntityWithId(target);
            s.transforms[e3.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
            s.sprites[e3.id] = Sprite{ {80.f,80.f}, sf::Color(255,128,0,255) };
            s.colliders[e3.id] = Collider{ {40.f,40.f}, {0.f,0.f} };
        }
        auto& sc = s.scripts[target];
        sc.path = "Assets/Scripts/mover.lua";
        sc.inlineCode.clear();
        sc.loaded = false;
    }
}

void LauncherLayer::EnterEditor() {
    auto& app = gp::Application::Get();
    auto& ctx = SceneContext::Get();

    // cargar o sembrar
    if (!m_selected.empty() && exists(m_selected)) {
        if (!ctx.scene) ctx.scene = std::make_shared<Scene>();
        SceneSerializer::Load(*ctx.scene, m_selected);
        Renderer2D::ClearTextureCache();
    }
    else {
        SeedNewScene();
    }

    app.SetMode(gp::Application::Mode::Editor);
    // montar editor
    app.PushLayer(new EditorDockLayer());
    app.PushLayer(new ViewportPanel());
    app.PushLayer(new InspectorPanel());
    app.PushLayer(new ChatPanel(ctx.apiClient));

    // cerrar launcher
    app.PopLayer(this);
}

void LauncherLayer::OnGuiRender() {
    ImGui::Begin("Launcher", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    // --- Sesión ---
    ImGui::TextUnformatted("Sesión:");
    if (!m_loggedIn) {
        if (ImGui::Button("Iniciar sesión")) {
            DoLoginInteractive();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(necesaria para usar el editor y el chat)");
    }
    else {
        ImGui::TextColored(ImVec4(0.1f, 0.5f, 0.1f, 1), "Estás logueado.");
    }

    ImGui::Separator();

    // --- Proyectos ---
    ImGui::TextUnformatted("Elegí un proyecto local (Saves/*.json):");
    DrawProjectPicker();

    ImGui::Separator();

    ImGui::BeginDisabled(!m_loggedIn);
    if (ImGui::Button("Nuevo proyecto")) {
        m_selected.clear();
        EnterEditor();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_selected.empty());
    if (ImGui::Button("Abrir seleccionado")) {
        EnterEditor();
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (!m_loggedIn) {
        ImGui::TextDisabled("Para continuar, iniciá sesión.");
    }

    ImGui::End();
}
