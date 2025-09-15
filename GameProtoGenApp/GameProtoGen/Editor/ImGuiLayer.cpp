#include "ImGuiLayer.h"
#include "Core/SFMLWindow.h"
#include "EditorFonts.h"
#include "ECS/Components.h"
#include "ECS/SceneSerializer.h"
#include "Runtime/SceneContext.h"

#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_internal.h>
#include <filesystem>

// Helpers Files/Proyecto
namespace {
    const char* kProjPath = "Saves/project.json";

    static void EnsureSavesDir() {
        try {
            std::filesystem::path p(kProjPath);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        }
        catch (...) {}
    }

    // Fallback post-load por si abrís un JSON viejo sin física/controlador
    static void FixSceneAfterLoad() {
        auto& ctx = SceneContext::Get();
        if (!ctx.scene) return;
        auto& sc = *ctx.scene;

        // Suelo estático
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

        // Jugador
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
        EnsureSavesDir();
        auto& ctx = SceneContext::Get();
        if (ctx.scene) SceneSerializer::Save(*ctx.scene, kProjPath);
    }

    static void DoLoad() {
        auto& ctx = SceneContext::Get();
        if (ctx.scene && std::filesystem::exists(kProjPath)) {
            if (SceneSerializer::Load(*ctx.scene, kProjPath)) {
                FixSceneAfterLoad();
            }
        }
    }

    // ----- Spawners ----------------------------------------------------
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

    // ¿Es Player?
    static bool IsPlayer(const Scene& scene, Entity e) {
        return e && scene.playerControllers.contains(e.id);
    }

    // ----- Duplicar entidad (bloquea Player) ---------------------------
    static Entity DuplicateEntity(Scene& scene, Entity src, const sf::Vector2f& offset = { 16.f, 16.f }) {
        if (!src) return {};
        if (IsPlayer(scene, src)) return {}; // ← no duplicar Player

        Entity dst = scene.CreateEntity();

        // Transform
        if (auto it = scene.transforms.find(src.id); it != scene.transforms.end()) {
            Transform t = it->second;
            t.position += offset; // que no quede exactamente encima
            scene.transforms[dst.id] = t;
        }
        // Sprite
        if (auto it = scene.sprites.find(src.id); it != scene.sprites.end()) {
            scene.sprites[dst.id] = it->second;
        }
        // Collider
        if (auto it = scene.colliders.find(src.id); it != scene.colliders.end()) {
            scene.colliders[dst.id] = it->second;
        }
        // Physics2D (velocidad reseteada)
        if (auto it = scene.physics.find(src.id); it != scene.physics.end()) {
            Physics2D p = it->second;
            p.velocity = { 0.f, 0.f };
            p.onGround = false;
            scene.physics[dst.id] = p;
        }
        // PlayerController NO se copia (si existiera, ya bloqueamos arriba)

        return dst;
    }
} // namespace

ImGuiLayer::ImGuiLayer(gp::SFMLWindow& window) : m_Window(window) {}

void ImGuiLayer::OnAttach() {
    auto& win = m_Window.Native();
    ImGui::SFML::Init(win);

    m_Window.SetRawEventCallback([this](const void* e) {
        const sf::Event& ev = *static_cast<const sf::Event*>(e);
        ImGui::SFML::ProcessEvent(m_Window.Native(), ev);
        });

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    const ImWchar* ranges = io.Fonts->GetGlyphRangesDefault();

    EditorFonts::Regular = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Regular.ttf", 20.0f, nullptr, ranges);
    EditorFonts::H2 = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Bold.ttf", 22.0f, nullptr, ranges);
    EditorFonts::H1 = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Roboto-Bold.ttf", 26.0f, nullptr, ranges);

    io.FontDefault = EditorFonts::Regular;
    ImGui::SFML::UpdateFontTexture();

    ImGui::StyleColorsLight();
}

void ImGuiLayer::OnDetach() { ImGui::SFML::Shutdown(); }

void ImGuiLayer::OnUpdate(const gp::Timestep&) {
    ImGui::SFML::Update(m_Window.Native(), m_Clock.restart());
}

void ImGuiLayer::OnGuiRender() {
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
            if (ImGui::MenuItem("Salir", "Alt+F4")) {
                gp::Application::Get().RequestClose();
            }
            ImGui::EndMenu();
        }

        // Menú GameObjects
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

            // Duplicar seleccionado (bloquea Player)
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

    // Atajos teclado globales (en pausa)
    ImGuiIO& io = ImGui::GetIO();
    if (!playing) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) DoSave();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) DoLoad();

        // Crear nuevo cuadrado (en centro de cámara)
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
            auto& ctx = SceneContext::Get();
            if (ctx.scene) {
                Entity e = SpawnBox(*ctx.scene, ctx.cameraCenter, { 100.f, 100.f });
                ctx.selected = e;
            }
        }

        // Duplicar seleccionado (Ctrl + D) — bloquea Player
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            auto& ctx = SceneContext::Get();
            if (ctx.scene && ctx.selected && !IsPlayer(*ctx.scene, ctx.selected)) {
                Entity newE = DuplicateEntity(*ctx.scene, ctx.selected, { 16.f,16.f });
                if (newE) ctx.selected = newE;
            }
        }

        // Eliminar seleccionado (Supr) — bloquea Player
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            auto& ctx = SceneContext::Get();
            if (ctx.scene && ctx.selected && !IsPlayer(*ctx.scene, ctx.selected)) {
                ctx.scene->DestroyEntity(ctx.selected);
                ctx.selected = {};
            }
        }
    }

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
    ImGui::End();
}
