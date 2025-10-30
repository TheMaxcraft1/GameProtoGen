#include "EditorDockLayer.h"

#include "Runtime/SceneContext.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ChatPanel.h"
#include "ECS/SceneSerializer.h"
#include "ECS/Components.h"
#include "Systems/Renderer2D.h"
#include "Auth/OidcClient.h"
#include "Net/ApiClient.h"
#include "Auth/TokenManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <SFML/Graphics/Color.hpp>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "Core/Log.h"

// ======================== Helpers (copiados de tu ImGuiLayer.cpp) ========================
namespace {
    const char* kSavesDir = "Saves";

    static void EnsureSavesDir() {
        std::filesystem::path p(kSavesDir);
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        if (ec) {
            Log::Error(std::string("[SAVE] ERROR creando carpeta: ") + ec.message());
        }
    }

    static void DoLoginInteractive() {
        auto& ctx = SceneContext::Get();
        if (!ctx.apiClient) {
            Log::Info("[AUTH] ApiClient no inicializado");
            return;
        }
        OidcConfig cfg;
        cfg.client_id = "2041dbc5-c266-43aa-af66-765b1440f34a";
        cfg.authorize_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/authorize";
        cfg.token_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/token";
        cfg.scopes = { "openid","profile","offline_access","api://gameprotogen/access_as_user" };

        OidcClient oidc(cfg);
        std::string err;
        auto tokens = oidc.AcquireTokenInteractive(&err);
        if (!tokens) {
            Log::Error(std::string("[AUTH] Error: ") + err);
            return;
        }
        if (!ctx.tokenManager) ctx.tokenManager = std::make_shared<TokenManager>(cfg);
        ctx.tokenManager->OnInteractiveLogin(*tokens);

        ctx.apiClient->SetAccessToken(tokens->access_token);
        ctx.apiClient->SetTokenRefresher([mgr = ctx.tokenManager]() -> std::optional<std::string> {
            return mgr->Refresh();
            });
        ctx.apiClient->SetPreflight([mgr = ctx.tokenManager]() { (void)mgr->EnsureFresh(); });

        Log::Info("[AUTH] Login OK. access_token seteado en ApiClient.");
        if (!tokens->refresh_token.empty())
            Log::Info("[AUTH] refresh_token presente (persistido en Saves/tokens.json).");
    }

    static void FixSceneAfterLoad() {
        auto& ctx = SceneContext::Get();
        if (!ctx.scene) return;
        auto& sc = *ctx.scene;

        bool hasStaticCollider = false;
        for (auto& [id, _] : sc.colliders) {
            if (!sc.physics.contains(id)) { hasStaticCollider = true; break; }
        }
        if (!hasStaticCollider) {
            auto ground = sc.CreateEntity();
            sc.transforms[ground.id] = Transform{ {800.f, 820.f}, {1.f,1.f}, 0.f };
            sc.sprites[ground.id] = Sprite{ {1600.f, 160.f}, sf::Color(60,60,70,255) };
            sc.colliders[ground.id] = Collider{ {800.f, 80.f}, {0.f,0.f} };
        }

        EntityID playerId = 0;
        if (!sc.playerControllers.empty())
            playerId = sc.playerControllers.begin()->first;

        if (!playerId) {
            Entity chosen{};
            for (auto& e : sc.Entities()) {
                if (sc.transforms.contains(e.id)) { chosen = e; break; }
            }
            if (!chosen) {
                chosen = sc.CreateEntity();
                sc.transforms[chosen.id] = Transform{ {800.f, 450.f}, {1.f,1.f}, 0.f };
                sc.sprites[chosen.id] = Sprite{ {80.f,120.f}, sf::Color(0,255,0,255) };
                sc.colliders[chosen.id] = Collider{ {40.f,60.f}, {0.f,0.f} };
            }
            if (!sc.physics.contains(chosen.id)) sc.physics[chosen.id] = Physics2D{};
            sc.playerControllers[chosen.id] = PlayerController{ 500.f, 900.f };
            playerId = chosen.id;
        }
        ctx.selected = Entity{ playerId };
    }

    static void DoSave() {
        using namespace std::chrono;
        EnsureSavesDir();
        auto& ctx = SceneContext::Get();
        if (!ctx.scene) {
            Log::Error("[SAVE] ERROR  escena nula");
            return;
        }
        auto path = std::filesystem::path(kSavesDir) / "scene.json";
        bool ok = SceneSerializer::Save(*ctx.scene, path.string());

        auto now = system_clock::now();
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << "[SAVE] " << (ok ? "OK" : "ERROR")
            << "  " << path.string() << "  "
            << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        Log::Info(oss.str());
    }

    static void DoLoad() {
        using namespace std::chrono;
        auto& ctx = SceneContext::Get();
        if (!ctx.scene) {
            Log::Error("[LOAD] ERROR  escena nula");
            return;
        }
        auto path = std::filesystem::path(kSavesDir) / "scene.json";
        bool ok = SceneSerializer::Load(*ctx.scene, path.string());
        FixSceneAfterLoad();
        Renderer2D::ClearTextureCache();

        auto now = system_clock::now();
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << "[LOAD] " << (ok ? "OK" : "ERROR")
            << "  " << path.string() << "  "
            << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        Log::Info(oss.str());
    }

    static Entity SpawnBox(Scene& scene,
        const sf::Vector2f& pos,
        const sf::Vector2f& size,
        const sf::Color& color = sf::Color(255, 255, 255, 255)) {
        Entity e = scene.CreateEntity();
        scene.transforms[e.id] = Transform{ pos, {1.f,1.f}, 0.f };
        scene.sprites[e.id] = Sprite{ size, color };
        scene.colliders[e.id] = Collider{ { size.x * 0.5f, size.y * 0.5f }, {0.f,0.f} };
        return e;
    }

    static Entity SpawnPlatform(Scene& scene,
        const sf::Vector2f& pos,
        const sf::Vector2f& size,
        const sf::Color& color = sf::Color(255, 255, 255, 255)) {
        Entity e = scene.CreateEntity();
        scene.transforms[e.id] = Transform{ pos, {1.f,1.f}, 0.f };
        scene.sprites[e.id] = Sprite{ size, color };
        scene.colliders[e.id] = Collider{ { size.x * 0.5f, size.y * 0.5f }, {0.f,0.f} };
        return e;
    }

    static bool IsPlayer(const Scene& scene, Entity e) {
        return e && scene.playerControllers.contains(e.id);
    }

    static Entity DuplicateEntity(Scene& scene, Entity src, const sf::Vector2f& offset = { 16.f, 16.f }) {
        if (!src) return {};
        if (IsPlayer(scene, src)) return {};
        Entity dst = scene.CreateEntity();

        if (auto it = scene.transforms.find(src.id); it != scene.transforms.end()) {
            Transform t = it->second;
            t.position += offset;
            scene.transforms[dst.id] = t;
        }
        if (auto it = scene.sprites.find(src.id); it != scene.sprites.end()) {
            scene.sprites[dst.id] = it->second;
        }
        if (auto it = scene.colliders.find(src.id); it != scene.colliders.end()) {
            scene.colliders[dst.id] = it->second;
        }
        if (auto it = scene.physics.find(src.id); it != scene.physics.end()) {
            Physics2D p = it->second;
            p.velocity = { 0.f, 0.f };
            p.onGround = false;
            scene.physics[dst.id] = p;
        }
        return dst;
    }
} // namespace
// ====================== Fin Helpers ======================

void EditorDockLayer::OnGuiRender() {
    // Host que ocupa toda la viewport con menú y dockspace
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("###MainDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(2);

    const bool playing = SceneContext::Get().runtime.playing;

    // ── Menú superior ───────────────────────────────────────────────────────
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Proyecto", !playing)) {
            if (ImGui::MenuItem("Guardar", "Ctrl+S")) DoSave();
            if (ImGui::MenuItem("Cargar", "Ctrl+O")) DoLoad();
            ImGui::Separator();
            if (ImGui::MenuItem("Iniciar sesión…")) {
                DoLoginInteractive();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Salir", "Alt+F4")) {
                gp::Application::Get().RequestClose();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GameObjects", !playing)) {
            if (ImGui::MenuItem("Crear cuadrado", "Ctrl+N")) {
                auto& ctx = SceneContext::Get();
                if (ctx.scene) {
                    const sf::Vector2f spawnPos = ctx.cameraCenter;
                    Entity e = SpawnBox(*ctx.scene, spawnPos, { 100.f, 100.f });
                    ctx.selected = e;
                }
            }
            if (ImGui::MenuItem("Crear plataforma", "Ctrl+N")) {
                auto& ctx = SceneContext::Get();
                if (ctx.scene) {
                    const sf::Vector2f spawnPos = ctx.cameraCenter;
                    Entity e = SpawnPlatform(*ctx.scene, spawnPos, { 200.f, 50.f });
                    ctx.selected = e;
                }
            }
            {
                auto& ctx = SceneContext::Get();
                const bool canDup = (ctx.scene && ctx.selected && !IsPlayer(*ctx.scene, ctx.selected));
                ImGui::BeginDisabled(!canDup);
                if (ImGui::MenuItem("Duplicar seleccionado", "Ctrl+D")) {
                    Entity newE = DuplicateEntity(*ctx.scene, ctx.selected, { 16.f,16.f });
                    if (newE) ctx.selected = newE;
                }
                ImGui::EndDisabled();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // ── Popup de “Guardar antes de salir” ──────────────────────────────────
    if (gp::Application::Get().WantsClose()) {
        ImGui::OpenPopup("Guardar antes de salir");
    }
    if (ImGui::BeginPopupModal("Guardar antes de salir", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("¿Querés guardar los cambios antes de salir?");
        ImGui::Separator();
        if (ImGui::Button("Guardar y salir")) {
            DoSave();
            ImGui::CloseCurrentPopup();
            gp::Application::Get().QuitNow();
        }
        ImGui::SameLine();
        if (ImGui::Button("Salir sin guardar")) {
            ImGui::CloseCurrentPopup();
            gp::Application::Get().QuitNow();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) {
            ImGui::CloseCurrentPopup();
            gp::Application::Get().CancelClose();
        }
        ImGui::EndPopup();
    }

    // ── Atajos de teclado (solo si no está jugando) ────────────────────────
    ImGuiIO& io = ImGui::GetIO();
    if (!playing) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) DoSave();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) DoLoad();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
            auto& ctx = SceneContext::Get();
            if (ctx.scene) {
                Entity e = SpawnBox(*ctx.scene, ctx.cameraCenter, { 100.f, 100.f });
                ctx.selected = e;
            }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            auto& ctx = SceneContext::Get();
            if (ctx.scene && ctx.selected && !IsPlayer(*ctx.scene, ctx.selected)) {
                Entity newE = DuplicateEntity(*ctx.scene, ctx.selected, { 16.f,16.f });
                if (newE) ctx.selected = newE;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            auto& ctx = SceneContext::Get();
            if (ctx.scene && ctx.selected && !IsPlayer(*ctx.scene, ctx.selected)) {
                ctx.scene->DestroyEntity(ctx.selected);
                ctx.selected = {};
            }
        }
    }

    // ── DockSpace + layout inicial ─────────────────────────────────────────
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0, 0), 0);

    if (!m_BuiltDock) {
        m_BuiltDock = true;
        ImGuiIO& io2 = ImGui::GetIO();
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, 0);
        ImGui::DockBuilderSetNodeSize(dockspace_id, io2.DisplaySize);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
            dock_main_id, ImGuiDir_Right, 0.30f, nullptr, &dock_main_id);

        ImGui::DockBuilderDockWindow("Inspector", dock_right_id);
        ImGui::DockBuilderDockWindow("Chat", dock_right_id);
        ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End(); // ###MainDockHost
}
