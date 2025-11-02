#include "EditorDockLayer.h"

#include "Runtime/SceneContext.h"
#include "Runtime/EditorContext.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/ChatPanel.h"
#include "ECS/SceneSerializer.h"
#include "ECS/Components.h"
#include "Systems/Renderer2D.h"
#include "Auth/OidcClient.h"
#include "Net/ApiClient.h"
#include "Auth/TokenManager.h"
#include "Core/Log.h"
#include "Core/Application.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <optional>
#include <sstream>
#include <iomanip>

#include <imgui.h>
#include <imgui_internal.h>
#include <SFML/Graphics/Color.hpp>

#include <nlohmann/json.hpp>

// ======================== Helpers ========================
namespace {
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // MAX_PATH, GetModuleFileNameA
#endif

#if __has_include(<tinyfiledialogs.h>)
#include <tinyfiledialogs.h>
#define GP_HAS_TINYFD 1
#else
#define GP_HAS_TINYFD 0
#endif

    // Ruta del exe del Editor (carpeta bin actual)
    static std::string GetExeDir() {
#ifdef _WIN32
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::filesystem::path p(buf);
        return p.parent_path().string();
#else
        return std::filesystem::current_path().string();
#endif
    }

    // Busca la carpeta Assets empezando al lado del .exe y subiendo hasta 3 niveles
    static std::filesystem::path FindAssetsRoot() {
        std::error_code ec;

        // 1) Al lado del exe
        std::filesystem::path bin = GetExeDir();
        std::filesystem::path p = bin / "Assets";
        if (std::filesystem::exists(p, ec)) return p;

        // 2) Subir hasta 3 niveles (útil si estás en out/build/.../bin)
        std::filesystem::path cur = bin;
        for (int i = 0; i < 3; ++i) {
            cur = cur.parent_path();
            if (cur.empty()) break;
            p = cur / "Assets";
            if (std::filesystem::exists(p, ec)) return p;
        }

        // 3) CWD/Assets último intento
        p = std::filesystem::current_path() / "Assets";
        if (std::filesystem::exists(p, ec)) return p;

        // 4) Si nada, devolvemos al lado del exe (aunque no exista)
        return std::filesystem::path(GetExeDir()) / "Assets";
    }

    // Cachea la ruta detectada de Assets
    static std::filesystem::path GetAssetsRoot() {
        static std::filesystem::path cached = FindAssetsRoot();
        return cached;
    }

    static void LogAssetsRoot() {
        auto r = GetAssetsRoot();
        Log::Info(std::string("[EXPORT] AssetsRoot = ") + r.string());
    }

    // Dada una ruta raw (relativa/absoluta), intenta resolver a una absoluta existente
    static std::optional<std::filesystem::path> ResolveAssetPath(const std::filesystem::path& raw) {
        std::error_code ec;
        if (raw.empty()) return std::nullopt;

        // 1) Si ya es absoluta y existe
        if (raw.is_absolute() && std::filesystem::exists(raw, ec)) return raw;

        // 2) Assets/<raw>
        {
            auto abs = GetAssetsRoot() / raw;
            if (std::filesystem::exists(abs, ec)) return abs;
        }

        // 3) CWD/<raw>
        {
            auto abs = std::filesystem::current_path() / raw;
            if (std::filesystem::exists(abs, ec)) return abs;
        }

        return std::nullopt;
    }

    // Normaliza y devuelve el subpath relativo a Assets si corresponde.
    // Si el archivo no está dentro de Assets, retorna std::nullopt.
    static std::optional<std::filesystem::path> RelToAssets(const std::filesystem::path& pAbs) {
        std::error_code ec;
        auto assets = std::filesystem::weakly_canonical(GetAssetsRoot(), ec);
        auto abs = std::filesystem::weakly_canonical(pAbs, ec);
        if (ec) return std::nullopt;

        // ¿abs comienza con assets?
        auto mismatch = std::mismatch(assets.begin(), assets.end(), abs.begin(), abs.end());
        if (mismatch.first == assets.end()) {
            return std::filesystem::relative(abs, assets, ec);
        }
        return std::nullopt;
    }

    struct AssetRef {
        std::filesystem::path abs;     // ruta absoluta real
        std::filesystem::path rel;     // ruta relativa dentro de Assets (ej: Textures/foo.png)
        std::string reason;            // "texture" | "script" | "sprite"
    };

    // Recolecta texturas/scripts (y opcionalmente sprites con imagePath) referenciados en la escena, sólo si están bajo Assets/.
    static std::vector<AssetRef> CollectUsedAssets(const Scene& scene) {
        std::vector<AssetRef> out;
        std::unordered_set<std::string> dedup;

        auto try_add = [&](const std::filesystem::path& raw, const std::string& reason) {
            auto absOpt = ResolveAssetPath(raw);
            if (!absOpt) {
                Log::Info(std::string("[EXPORT] skip (no existe): ") + raw.string());
                return;
            }
            const auto& abs = *absOpt;

            if (auto rel = RelToAssets(abs)) {
                auto key = (rel->generic_string() + "|" + reason);
                if (!dedup.insert(key).second) return;
                Log::Info(std::string("[EXPORT] + ") + reason + "  " + rel->generic_string());
                out.push_back(AssetRef{ abs, *rel, reason });
            }
            else {
                Log::Info(std::string("[EXPORT] skip (fuera de Assets): ") + abs.string());
            }
            };

        // 1) Texturas
        for (const auto& [id, tex] : scene.textures) {
            (void)id;
            if (tex.path.empty()) continue;
            try_add(tex.path, "texture");
        }

        // 2) Scripts
        for (const auto& [id, sc] : scene.scripts) {
            (void)id;
            if (sc.path.empty()) continue;
            try_add(sc.path, "script");
        }

        // 3) (Opcional) Si tu Sprite tiene un campo con ruta de imagen (descomenta si aplica)
        // for (const auto& [id, spr] : scene.sprites) {
        //     if (!spr.imagePath.empty()) try_add(spr.imagePath, "sprite");
        // }

        return out;
    }

    // Exporta sólo los assets usados preservando subcarpetas bajo Assets/.
    // Devuelve una lista de faltantes para log/manifest.
    static std::vector<std::filesystem::path> CopyUsedAssetsOnly(
        const Scene& scene, const std::filesystem::path& outDir) {

        std::vector<std::filesystem::path> missing;
        const auto dstRoot = outDir / "Assets";

        std::error_code mkec;
        std::filesystem::create_directories(dstRoot, mkec);

        auto used = CollectUsedAssets(scene);

        // Fallback opcional: si no detectamos nada y querés evitar carpeta vacía
        bool kExportCopyAllIfEmpty = true;
        if (used.empty() && kExportCopyAllIfEmpty) {
            std::error_code ec;
            std::filesystem::copy(
                GetAssetsRoot(), dstRoot,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) Log::Error(std::string("[EXPORT] Fallback copy all Assets/ error: ") + ec.message());
            else    Log::Info("[EXPORT] Fallback: no refs → se copió Assets/ completo.");
            return {};
        }

        nlohmann::json manifest = nlohmann::json::object();
        manifest["copied"] = nlohmann::json::array();
        manifest["missing"] = nlohmann::json::array();

        for (const auto& a : used) {
            std::error_code ec;
            const auto dst = dstRoot / a.rel; // preserva estructura relativa
            std::filesystem::create_directories(dst.parent_path(), ec);
            if (!ec) {
                std::filesystem::copy_file(a.abs, dst,
                    std::filesystem::copy_options::overwrite_existing, ec);
            }

            nlohmann::json row;
            row["source"] = a.abs.generic_string();
            row["dest"] = dst.generic_string();
            row["reason"] = a.reason;

            if (ec) {
                row["status"] = std::string("error: ") + ec.message();
                manifest["missing"].push_back(row);
                missing.push_back(a.abs);
            }
            else {
                row["status"] = "ok";
                manifest["copied"].push_back(row);
            }
        }

        // dump manifest
        try {
            std::ofstream mf(outDir / "assets_manifest.json");
            mf << manifest.dump(2);
        }
        catch (...) {
            // ignoramos errores del manifest
        }

        return missing;
    }

    // ====== SAVE/LOAD ======
    const char* kSavesDir = "Saves";

    static void EnsureSavesDir() {
        std::filesystem::path p(kSavesDir);
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        if (ec) Log::Error(std::string("[SAVE] ERROR creando carpeta: ") + ec.message());
    }

    // ---- AUTH: ahora usa EditorContext ----
    static void DoLoginInteractive() {
        auto& edx = EditorContext::Get();
        if (!edx.apiClient) {
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
        if (!edx.tokenManager) edx.tokenManager = std::make_shared<TokenManager>(cfg);
        edx.tokenManager->OnInteractiveLogin(*tokens);

        edx.apiClient->SetAccessToken(tokens->access_token);
        edx.apiClient->SetTokenRefresher([mgr = edx.tokenManager]() -> std::optional<std::string> {
            return mgr->Refresh();
            });
        edx.apiClient->SetPreflight([mgr = edx.tokenManager]() { (void)mgr->EnsureFresh(); });

        Log::Info("[AUTH] Login OK. access_token seteado en ApiClient.");
        if (!tokens->refresh_token.empty())
            Log::Info("[AUTH] refresh_token presente (persistido en Saves/tokens.json).");
    }

    // ---- Post-load fixes: SceneContext para escena; EditorContext para selección ----
    static void FixSceneAfterLoad() {
        auto& scx = SceneContext::Get();
        auto& edx = EditorContext::Get();
        if (!scx.scene) return;
        auto& sc = *scx.scene;

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
        edx.selected = Entity{ playerId };
    }

    static void DoSave() {
        using namespace std::chrono;
        EnsureSavesDir();
        auto& scx = SceneContext::Get();
        auto& edx = EditorContext::Get();
        if (!scx.scene) {
            Log::Error("[SAVE] ERROR  escena nula");
            return;
        }

        std::string projPath = edx.projectPath;
        if (projPath.empty()) projPath = (std::filesystem::path(kSavesDir) / "scene.json").string();
        bool ok = SceneSerializer::Save(*scx.scene, projPath);

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
            << "  " << projPath << "  "
            << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        Log::Info(oss.str());
    }

    static void DoLoad() {
        using namespace std::chrono;
        auto& scx = SceneContext::Get();
        auto& edx = EditorContext::Get();
        if (!scx.scene) {
            Log::Error("[LOAD] ERROR  escena nula");
            return;
        }
        std::string projPath = edx.projectPath;
        if (projPath.empty()) projPath = (std::filesystem::path(kSavesDir) / "scene.json").string();

        bool ok = SceneSerializer::Load(*scx.scene, projPath);
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
            << "  " << projPath << "  "
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
        if (IsPlayer(scene, src)) return {}; // no duplicar al jugador

        Entity dst = scene.CreateEntity();

        // Transform
        if (auto it = scene.transforms.find(src.id); it != scene.transforms.end()) {
            Transform t = it->second;
            t.position += offset;
            scene.transforms[dst.id] = t;
        }

        // Sprite (geom/color etc.)
        if (auto it = scene.sprites.find(src.id); it != scene.sprites.end()) {
            scene.sprites[dst.id] = it->second;
        }

        // Collider
        if (auto it = scene.colliders.find(src.id); it != scene.colliders.end()) {
            scene.colliders[dst.id] = it->second;
        }

        // Physics (reset estado volátil)
        if (auto it = scene.physics.find(src.id); it != scene.physics.end()) {
            Physics2D p = it->second;
            p.velocity = { 0.f, 0.f };
            p.onGround = false;
            scene.physics[dst.id] = p;
        }

        // 🔹 Texture (conserva el path para que apunte al mismo asset)
        //    Esto NO copia archivos; solo duplica el componente con su ruta.
        if (auto it = scene.textures.find(src.id); it != scene.textures.end()) {
            scene.textures[dst.id] = it->second;
            // opcional: si tu struct tiene campos de handle/cached, podrías invalidarlos aquí
            // scene.textures[dst.id].handle = nullptr; // si aplica a tu implementación
        }

        // 🔹 Script (respeta path o inlineCode; fuerza reload)
        if (auto it = scene.scripts.find(src.id); it != scene.scripts.end()) {
            Script sc = it->second;      // copia completa (path e inlineCode)
            sc.loaded = false;           // asegura que el runtime/editor lo recargue
            scene.scripts[dst.id] = std::move(sc);
        }

        return dst;
    }

    // Devuelve ruta al ejecutable del Player junto al Editor
    static std::filesystem::path FindPlayerExe() {
        std::filesystem::path binDir = GetExeDir();
#ifdef _WIN32
        std::filesystem::path candidate = binDir / "GameProtoGenPlayer.exe";
#else
        std::filesystem::path candidate = binDir / "GameProtoGenPlayer";
#endif
        if (std::filesystem::exists(candidate)) return candidate;
        return {};
    }

    static void DoExportExecutable() {
        auto& scx = SceneContext::Get();
        if (!scx.scene) {
            Log::Error("[EXPORT] ERROR: escena nula.");
            return;
        }

        // Elegir carpeta destino
        std::filesystem::path outDir;
#if GP_HAS_TINYFD
        if (const char* dst = tinyfd_selectFolderDialog("Elegí carpeta de exportación", nullptr)) {
            outDir = dst;
        }
        else {
            Log::Info("[EXPORT] Cancelado por el usuario.");
            return;
        }
#else
        // Fallback: Export/ junto al .exe si no está tinyfd
        outDir = std::filesystem::path(GetExeDir()) / "Export";
        {
            std::error_code mkec;
            std::filesystem::create_directories(outDir, mkec);
            if (mkec) {
                Log::Error(std::string("[EXPORT] ERROR creando carpeta de exportación: ") + mkec.message());
                return;
            }
            Log::Info(std::string("[EXPORT] tinyfiledialogs no disponible. Exportando en: ") + outDir.string());
        }
#endif

        // 1) Player junto al Editor
        std::filesystem::path player = FindPlayerExe();
        if (player.empty() || !std::filesystem::exists(player)) {
            Log::Error("[EXPORT] No se encontró GameProtoGenPlayer junto al Editor. Compilá el target GameProtoGenPlayer.");
            return;
        }

        // 2) Guardar escena
        {
            std::filesystem::path sceneOut = outDir / "scene.json";
            bool ok = SceneSerializer::Save(*scx.scene, sceneOut.string());
            if (!ok) {
                Log::Error("[EXPORT] ERROR guardando scene.json");
                return;
            }
        }

        // 3) Copiar ejecutable del Player
#ifdef _WIN32
        std::filesystem::path playerOut = outDir / "GameProtoGenPlayer.exe";
#else
        std::filesystem::path playerOut = outDir / "GameProtoGenPlayer";
#endif
        {
            std::error_code ec;
            std::filesystem::copy_file(player, playerOut, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                Log::Error(std::string("[EXPORT] ERROR copiando Player: ") + ec.message());
                return;
            }
#ifndef _WIN32
            // Permisos en *nix
            std::filesystem::permissions(playerOut,
                std::filesystem::perms::owner_exec | std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
                std::filesystem::perms::group_exec | std::filesystem::perms::group_read |
                std::filesystem::perms::others_exec | std::filesystem::perms::others_read,
                std::filesystem::perm_options::add, ec);
            if (ec) {
                Log::Error(std::string("[EXPORT] ERROR set perms: ") + ec.message());
            }
#endif
        }

#ifdef _WIN32
        // 4) Copiar DLLs excepto lo que sabemos que NO necesita el Player (blacklist)
        try {
            // Prefijos a EXCLUIR (case-insensitive)
            static const char* kDenyPrefixes[] = {
                "ImGui-SFML",      // Editor UI
                "imgui",           // Cualquier variante de imgui*.dll
                "tinyfiledialogs", // Diálogos del editor
                "cpr",             // HTTP del editor
                "libcurl",         // Dependencia de cpr
                "lua_static",      // El Player no debería depender de esto
                "gtest"            // Librerías de test
            };
            // Nombres EXACTOS a EXCLUIR (por si aparecen con sufijos debug)
            static const char* kDenyExact[] = {
                "ImGui-SFML.dll", "ImGui-SFML_d.dll",
                "tinyfiledialogs_lib.dll", "tinyfiledialogs_libd.dll",
                "cpr.dll", "cprd.dll",
                "libcurl.dll", "libcurl-d.dll",
                "lua_static.dll", "lua_staticd.dll",
                "gtest.dll", "gtest_main.dll"
            };

            auto iequals = [](char a, char b) { return ::tolower(a) == ::tolower(b); };
            auto starts_with_ci = [&](const std::string& s, const char* pfx) {
                size_t n = std::strlen(pfx);
                if (s.size() < n) return false;
                for (size_t i = 0; i < n; ++i) if (!iequals(s[i], pfx[i])) return false;
                return true;
                };
            auto equals_ci = [&](const std::string& a, const char* b) {
                size_t n = std::strlen(b);
                if (a.size() != n) return false;
                for (size_t i = 0; i < n; ++i) if (!iequals(a[i], b[i])) return false;
                return true;
                };
            auto is_denied = [&](const std::string& fname) {
                for (auto* e : kDenyExact)    if (equals_ci(fname, e)) return true;
                for (auto* p : kDenyPrefixes) if (starts_with_ci(fname, p)) return true;
                return false;
                };

            const std::filesystem::path editorBin = GetExeDir();
            for (auto& entry : std::filesystem::directory_iterator(editorBin)) {
                if (!entry.is_regular_file()) continue;
                const auto& p = entry.path();
                const auto ext = p.extension().string();
                if (_stricmp(ext.c_str(), ".dll") != 0) continue;

                const auto fname = p.filename().string();
                if (is_denied(fname)) continue; // saltar DLLs de editor

                std::error_code ec;
                std::filesystem::copy_file(
                    p, outDir / p.filename(),
                    std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    Log::Error(std::string("[EXPORT] ERROR copiando DLL: ")
                        + fname + " -> " + ec.message());
                }
            }
        }
        catch (const std::exception& e) {
            Log::Error(std::string("[EXPORT] Excepción copiando DLLs: ") + e.what());
        }
#endif

        // 5) Copiar Assets (filtrados)
        {
            LogAssetsRoot();
            auto missing = CopyUsedAssetsOnly(*scx.scene, outDir);

            if (missing.empty()) {
                Log::Info("[EXPORT] Assets filtrados copiados correctamente.");
            }
            else {
                Log::Info("[EXPORT] Algunos assets referenciados no se copiaron (ver assets_manifest.json).");
                for (const auto& m : missing) {
                    Log::Error(std::string("[EXPORT] Missing/Outside-Assets: ") + m.string());
                }
            }
        }

        // 6) Mensaje final
        Log::Info(std::string("[EXPORT] OK: carpeta lista en  ") + outDir.string());
        Log::Info("           Para correr, ejecutá GameProtoGenPlayer (toma scene.json local).");
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

    const bool playing = EditorContext::Get().runtime.playing;

    // ── Menú superior ───────────────────────────────────────────────────────
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Proyecto", !playing)) {
            if (ImGui::MenuItem("Guardar", "Ctrl+S")) DoSave();
            if (ImGui::MenuItem("Cargar", "Ctrl+O")) DoLoad();
            ImGui::Separator();
            if (ImGui::MenuItem("Exportar ejecutable…")) {
                DoExportExecutable();
            }
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
            auto& scx = SceneContext::Get();
            auto& edx = EditorContext::Get();

            if (ImGui::MenuItem("Crear cuadrado", "Ctrl+N")) {
                if (scx.scene) {
                    const sf::Vector2f spawnPos = scx.cameraCenter;
                    Entity e = SpawnBox(*scx.scene, spawnPos, { 100.f, 100.f });
                    edx.selected = e;
                }
            }
            if (ImGui::MenuItem("Crear plataforma", "Ctrl+N")) {
                if (scx.scene) {
                    const sf::Vector2f spawnPos = scx.cameraCenter;
                    Entity e = SpawnPlatform(*scx.scene, spawnPos, { 200.f, 50.f });
                    edx.selected = e;
                }
            }
            {
                const bool canDup = (scx.scene && edx.selected && !IsPlayer(*scx.scene, edx.selected));
                ImGui::BeginDisabled(!canDup);
                if (ImGui::MenuItem("Duplicar seleccionado", "Ctrl+D")) {
                    Entity newE = DuplicateEntity(*scx.scene, edx.selected, { 16.f,16.f });
                    if (newE) edx.selected = newE;
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
        auto& scx = SceneContext::Get();
        auto& edx = EditorContext::Get();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) DoSave();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) DoLoad();

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
            if (scx.scene) {
                Entity e = SpawnBox(*scx.scene, scx.cameraCenter, { 100.f, 100.f });
                edx.selected = e;
            }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
            if (scx.scene && edx.selected && !IsPlayer(*scx.scene, edx.selected)) {
                Entity newE = DuplicateEntity(*scx.scene, edx.selected, { 16.f,16.f });
                if (newE) edx.selected = newE;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            if (scx.scene && edx.selected && !IsPlayer(*scx.scene, edx.selected)) {
                scx.scene->DestroyEntity(edx.selected);
                edx.selected = {};
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
