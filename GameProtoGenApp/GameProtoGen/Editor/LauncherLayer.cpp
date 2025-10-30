#include "LauncherLayer.h"
#include "Runtime/SceneContext.h"
#include "Runtime/EditorContext.h"
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
#include "Core/SFMLWindow.h"

using std::filesystem::exists;
using std::filesystem::directory_iterator;

static void EnsureSavesDir() {
    std::error_code ec;
    std::filesystem::create_directories("Saves", ec);
    (void)ec;
}

bool LauncherLayer::TryAutoLogin() {
    auto& edx = EditorContext::Get();

    // Asegurar que haya TokenManager (por si alguien llega al launcher “frío”)
    if (!edx.tokenManager) {
        OidcConfig cfg;
        cfg.client_id = "2041dbc5-c266-43aa-af66-765b1440f34a";
        cfg.authorize_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/authorize";
        cfg.token_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/token";
        cfg.scopes = { "openid","profile","offline_access","api://gameprotogen/access_as_user" };
        edx.tokenManager = std::make_shared<TokenManager>(cfg);
    }

    // 1) Intentar cargar tokens persistidos
    if (!edx.tokenManager->Load()) return false;

    // 2) Conectar refresco/preflight al ApiClient (si existe)
    if (edx.apiClient) {
        edx.apiClient->SetTokenRefresher([mgr = edx.tokenManager]() -> std::optional<std::string> {
            return mgr->Refresh();
            });
        edx.apiClient->SetPreflight([mgr = edx.tokenManager]() { (void)mgr->EnsureFresh(); });
    }

    // 3) Asegurar que el access_token esté fresco
    if (!edx.tokenManager->EnsureFresh()) return false;

    // 4) Si hay access_token, setearlo en el ApiClient
    const std::string& at = edx.tokenManager->AccessToken();
    if (at.empty()) return false;

    if (edx.apiClient) {
        edx.apiClient->SetAccessToken(at);
    }
    return true;
}

void LauncherLayer::OnAttach() {
    EnsureSavesDir();
    m_loggedIn = TryAutoLogin();
}

void LauncherLayer::DoLoginInteractive() {
    auto& edx = EditorContext::Get();
    if (!edx.apiClient) return;

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

    if (!edx.tokenManager) edx.tokenManager = std::make_shared<TokenManager>(cfg);
    edx.tokenManager->OnInteractiveLogin(*tokens);

    edx.apiClient->SetAccessToken(tokens->access_token);
    edx.apiClient->SetTokenRefresher([mgr = edx.tokenManager]() -> std::optional<std::string> {
        return mgr->Refresh();
        });
    edx.apiClient->SetPreflight([mgr = edx.tokenManager]() { (void)mgr->EnsureFresh(); });

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
    auto& scx = SceneContext::Get();
    scx.scene = std::make_shared<Scene>();
    auto& s = *scx.scene;

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
    auto& scx = SceneContext::Get();
    auto& edx = EditorContext::Get();

    // cargar o sembrar
    if (!m_selected.empty() && exists(m_selected)) {
        if (!scx.scene) scx.scene = std::make_shared<Scene>();
        SceneSerializer::Load(*scx.scene, m_selected);
        Renderer2D::ClearTextureCache();
    }
    else {
        SeedNewScene();
    }

    app.SetMode(gp::Application::Mode::Editor);
    auto& win = static_cast<gp::SFMLWindow&>(app.Window());
    win.SetMaximized(true);

    // montar editor
    app.PushLayer(new EditorDockLayer());
    app.PushLayer(new ViewportPanel());
    app.PushLayer(new InspectorPanel());
    app.PushLayer(new ChatPanel(edx.apiClient)); // << usa EditorContext

    // cerrar launcher
    app.PopLayer(this);
}

void LauncherLayer::OnGuiRender() {
    // Ventana raíz a pantalla completa (dentro del window actual)
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24)); // margen amplio
    ImGui::Begin("##HubRoot", nullptr, flags);
    ImGui::PopStyleVar(3);

    // Centrar un panel de ancho fijo
    const float contentW = 720.f; // el “panel” del hub
    const float availW = ImGui::GetContentRegionAvail().x;
    const float x = (availW > contentW) ? (availW - contentW) * 0.5f : 0.f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);

    ImGui::BeginChild("##HubContent", ImVec2(contentW, 0), true, ImGuiWindowFlags_NoScrollbar);

    // --- Cabecera ---
    ImGui::TextUnformatted("GameProtoGen — Hub");
    ImGui::Separator();

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
        ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.1f, 1.f), "Estás logueado.");
    }

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();

    // --- Proyectos ---
    ImGui::TextUnformatted("Elegí un proyecto local (Saves/*.json):");
    DrawProjectPicker();

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();

    // Acciones
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
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextDisabled("Para continuar, iniciá sesión.");
    }

    ImGui::EndChild(); // ##HubContent
    ImGui::End();      // ##HubRoot
}
