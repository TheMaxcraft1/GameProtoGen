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
#include <algorithm>
#include <cctype>
#include <cstdio>
#include "Core/SFMLWindow.h"
#include <imgui-SFML.h>
#include <SFML/Graphics.hpp>   
#include <Editor/EditorFonts.h>

using std::filesystem::exists;
using std::filesystem::directory_iterator;

static void EnsureSavesDir() {
    std::error_code ec;
    std::filesystem::create_directories("Saves", ec);
    (void)ec;
}

static std::string NormalizeProjectName(std::string name, std::string& err) {
    // Trim
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();
    while (!name.empty() && (name.front() == ' ')) name.erase(name.begin());

    if (name.empty()) { err = "El nombre no puede estar vacío."; return {}; }

    auto validChar = [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == ' ';
        };
    if (!std::all_of(name.begin(), name.end(), [&](char ch) { return validChar((unsigned char)ch); })) {
        err = "Usá solo letras, números, espacio, guion o guion bajo.";
        return {};
    }
    // Reemplazar espacios por guión bajo
    for (auto& ch : name) if (ch == ' ') ch = '_';
    return name;
}

bool LauncherLayer::TryAutoLogin() {
    auto& edx = EditorContext::Get();

    if (!edx.tokenManager) {
        OidcConfig cfg;
        cfg.client_id = "2041dbc5-c266-43aa-af66-765b1440f34a";
        cfg.authorize_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/authorize";
        cfg.token_endpoint = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/oauth2/v2.0/token";
        cfg.scopes = { "openid","profile","offline_access","api://gameprotogen/access_as_user" };
        edx.tokenManager = std::make_shared<TokenManager>(cfg);
    }

    if (!edx.tokenManager->Load()) return false;

    if (edx.apiClient) {
        edx.apiClient->SetTokenRefresher([mgr = edx.tokenManager]() -> std::optional<std::string> {
            return mgr->Refresh();
            });
        edx.apiClient->SetPreflight([mgr = edx.tokenManager]() { (void)mgr->EnsureFresh(); });
    }

    if (!edx.tokenManager->EnsureFresh()) return false;

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

    m_LogoOK = m_LogoTex.loadFromFile("Internal/Brand/logo.png");
    if (m_LogoOK) m_LogoTex.setSmooth(true);
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

    // Juntamos y ordenamos por nombre (sin extensión), case-insensitive
    std::vector<std::filesystem::path> files;
    for (auto& p : directory_iterator("Saves")) {
        if (p.is_regular_file() && p.path().extension() == ".json") {
            files.push_back(p.path());
        }
    }
    if (files.empty()) {
        ImGui::TextDisabled("No hay proyectos todavía.");
        return;
    }

    auto ci_less = [](const std::filesystem::path& a, const std::filesystem::path& b) {
        std::string sa = a.stem().string();
        std::string sb = b.stem().string();
        std::transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
        std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
        return sa < sb;
        };
    std::sort(files.begin(), files.end(), ci_less);

    bool any = false;
    for (const auto& p : files) {
        any = true;
        const std::string full = p.string();              // con .json
        const std::string name = p.stem().string();       // sin .json

        bool selected = (m_selected == full);
        if (ImGui::Selectable(name.c_str(), selected)) {
            m_selected = full; // guardamos la ruta completa
        }
        // Tooltip con el nombre real por si querés ver la extensión
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", p.filename().string().c_str());
        }
        // Doble click para abrir
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            EnterEditor();
        }
    }
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

}

void LauncherLayer::EnterEditor() {
    auto& app = gp::Application::Get();
    auto& scx = SceneContext::Get();
    auto& edx = EditorContext::Get();

    // cargar existente o sembrar nuevo
    if (!m_selected.empty() && exists(m_selected)) {
        if (!scx.scene) scx.scene = std::make_shared<Scene>();
        SceneSerializer::Load(*scx.scene, m_selected);
        Renderer2D::ClearTextureCache();
        edx.projectPath = m_selected; // usar este archivo como proyecto actual
    }
    else {
        // Nuevo proyecto: si no hay ruta aún, usar una por defecto (safety)
        if (edx.projectPath.empty()) {
            edx.projectPath = (std::filesystem::path("Saves") / "scene.json").string();
        }
        SeedNewScene();

        // Guardado inicial del nuevo proyecto para que aparezca el archivo
        std::error_code ec;
        std::filesystem::create_directories("Saves", ec);
        SceneSerializer::Save(*scx.scene, edx.projectPath);
    }

    app.SetMode(gp::Application::Mode::Editor);
    auto& win = static_cast<gp::SFMLWindow&>(app.Window());
    win.SetMaximized(true);

    // montar editor
    app.PushLayer(new EditorDockLayer());
    app.PushLayer(new ViewportPanel());
    app.PushLayer(new InspectorPanel());
    app.PushLayer(new ChatPanel(edx.apiClient));

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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
    ImGui::Begin("##HubRoot", nullptr, flags);
    ImGui::PopStyleVar(3);

    // Centrar un panel de ancho fijo
    const float contentW = 720.f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const float x = (availW > contentW) ? (availW - contentW) * 0.5f : 0.f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);

    ImGui::BeginChild("##HubContent", ImVec2(contentW, 0), false, ImGuiWindowFlags_NoScrollbar);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 0));

        const float logoH = 108.f;

        // Calcular alto de textos (usando las fuentes si existen)
        float titleH = EditorFonts::Title ? EditorFonts::Title->FontSize : ImGui::GetFontSize();
        float h2H = EditorFonts::H2 ? EditorFonts::H2->FontSize : ImGui::GetFontSize();
        float textBlockH = titleH + h2H + 4.0f; // +4 de respiro entre líneas

        const float headerH = std::max(logoH, textBlockH) + 16.0f; // +padding

        // Child sin scroll y con alto suficiente
        ImGui::BeginChild("##Header",
            ImVec2(0, headerH),
            false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // ---- Logo a la izquierda
        if (m_LogoOK) {
            const auto sz = m_LogoTex.getSize();
            ImVec2 imgSize(logoH, logoH);
            if (sz.y != 0) {
                float scale = logoH / static_cast<float>(sz.y);
                imgSize.x = sz.x * scale;
            }
            ImGui::Image(m_LogoTex, imgSize);
            ImGui::SameLine();
        }

        ImGui::BeginGroup();
        {
            // Offset vertical para centrar el bloque de textos con el logo
            float yOff = (headerH - textBlockH) * 0.5f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOff);

            // Título (negro)
            if (EditorFonts::Title) ImGui::PushFont(EditorFonts::Title);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            ImGui::TextUnformatted("Game Protogen");
            ImGui::PopStyleColor();
            if (EditorFonts::Title) ImGui::PopFont();

            // Subtítulo (negro 75%)
            if (EditorFonts::H2) ImGui::PushFont(EditorFonts::H2);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0.75f));
            ImGui::TextUnformatted("Hub");
            ImGui::PopStyleColor();
            if (EditorFonts::H2) ImGui::PopFont();
        }
        ImGui::EndGroup();

        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        ImGui::Separator();
    }


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
    DrawProjectPicker();

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();

    // Acciones
    ImGui::BeginDisabled(!m_loggedIn);
    if (ImGui::Button("Nuevo proyecto")) {
        // Abrir modal para pedir nombre
        m_selected.clear();
        m_newProjModal = true;
        std::snprintf(m_newProjName, sizeof(m_newProjName), "Nuevo Proyecto");
        m_newProjError.clear();
        ImGui::OpenPopup("Crear nuevo proyecto");
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

    // --- Modal "Nuevo proyecto" ---
    if (ImGui::BeginPopupModal("Crear nuevo proyecto", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Nombre del proyecto:");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);       
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);        
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.50f, 0.90f, 1.00f)); 
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.00f, 1.00f, 1.00f, 1.00f)); 
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.96f, 0.98f, 1.00f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.92f, 0.96f, 1.00f, 1.00f)); 
        ImGui::InputText("##projname", m_newProjName, IM_ARRAYSIZE(m_newProjName));
        ImGui::PopStyleColor(3); 
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Se guardará en Saves/<nombre>.json");

        if (!m_newProjError.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.1f, 0.1f, 1.f), "%s", m_newProjError.c_str());
        }

        ImGui::Separator();
        if (ImGui::Button("Crear")) {
            std::string err;
            std::string clean = NormalizeProjectName(m_newProjName, err);
            if (clean.empty()) {
                m_newProjError = err;
            }
            else {
                std::filesystem::path projPath = std::filesystem::path("Saves") / (clean + ".json");

                // Asegurar carpeta
                EnsureSavesDir();

                // Setear selección y editor context
                m_selected = projPath.string();
                auto& edx = EditorContext::Get();
                edx.projectPath = m_selected;

                // Entrar al editor (esto crea la escena y guarda inicial)
                EnterEditor();

                ImGui::CloseCurrentPopup();
                m_newProjModal = false;
                m_newProjError.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) {
            ImGui::CloseCurrentPopup();
            m_newProjModal = false;
            m_newProjError.clear();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
}
