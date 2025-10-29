#include "ExportPlayable.h"
#include <filesystem>
#include <fstream>
#include "Runtime/SceneContext.h"
#include "ECS/SceneSerializer.h"
#include "Editor/Panels/ViewportPanel.h"

#if defined(_WIN32)
#include <windows.h>
static std::filesystem::path GetExeDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/stat.h> // chmod
static std::filesystem::path GetExeDir() {
    char buf[4096]; uint32_t sz = sizeof(buf);
    if (_NSGetExecutablePath(buf, &sz) == 0) return std::filesystem::path(buf).parent_path();
    return std::filesystem::current_path();
}
#else
#include <unistd.h>
#include <sys/stat.h> // chmod
static std::filesystem::path GetExeDir() {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) { buf[len] = '\0'; return std::filesystem::path(buf).parent_path(); }
    return std::filesystem::current_path();
}
#endif

namespace fs = std::filesystem;

static bool CopyTree(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::create_directories(dst, ec);
    for (auto& p : fs::recursive_directory_iterator(src, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) continue;
        auto rel = fs::relative(p.path(), src, ec);
        if (ec) continue;
        auto out = dst / rel;
        if (p.is_directory()) {
            fs::create_directories(out, ec);
        }
        else if (p.is_regular_file()) {
            fs::create_directories(out.parent_path(), ec);
            fs::copy_file(p.path(), out, fs::copy_options::overwrite_existing, ec);
        }
    }
    return true;
}

namespace Exporter {

    bool ExportPlayable(const std::string& outDirStr) {
        std::error_code ec;
        fs::path outDir(outDirStr);

        if (outDir.empty()) {
            ViewportPanel::AppendLog("[EXPORT] ERROR: carpeta destino vacía");
            return false;
        }
        fs::create_directories(outDir, ec);
        if (ec) {
            ViewportPanel::AppendLog(std::string("[EXPORT] ERROR creando carpeta: ") + ec.message());
            return false;
        }

        auto& ctx = SceneContext::Get();
        if (!ctx.scene) {
            ViewportPanel::AppendLog("[EXPORT] ERROR: escena nula");
            return false;
        }

        // 1) Guardar escena
        const fs::path sceneOut = outDir / "scene.json";
        if (!SceneSerializer::Save(*ctx.scene, sceneOut.string())) {
            ViewportPanel::AppendLog("[EXPORT] ERROR al serializar escena");
            return false;
        }

        // 2) Copiar player
#if defined(_WIN32)
        const char* playerName = "GameProtoGenPlayer.exe";
#else
        const char* playerName = "GameProtoGenPlayer";
#endif
        fs::path exeDir = GetExeDir();
        fs::path playerSrc = exeDir / playerName;
        if (!fs::exists(playerSrc)) {
            ViewportPanel::AppendLog("[EXPORT] WARNING: no encontré GameProtoGenPlayer junto al editor, probando ../");
            playerSrc = exeDir.parent_path() / playerName;
        }
        if (!fs::exists(playerSrc)) {
            ViewportPanel::AppendLog(std::string("[EXPORT] ERROR: no se encontró ") + playerSrc.string());
            return false;
        }
        fs::copy_file(playerSrc, outDir / playerName, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            ViewportPanel::AppendLog(std::string("[EXPORT] ERROR copiando player: ") + ec.message());
            return false;
        }

        // 3) Copiar Assets
        fs::path assetsSrc = exeDir / "Assets";
        if (!fs::exists(assetsSrc)) assetsSrc = exeDir.parent_path() / "Assets";
        if (fs::exists(assetsSrc)) {
            CopyTree(assetsSrc, outDir / "Assets");
        }
        else {
            ViewportPanel::AppendLog("[EXPORT] WARNING: no se encontró carpeta Assets para copiar");
        }

        // 4) Script de ejecución
#if defined(_WIN32)
        {
            std::ofstream bat(outDir / "Run.bat");
            bat << "@echo off\r\n";
            bat << "start \"\" \"" << playerName << "\" scene.json\r\n";
        }
#else
        {
            std::ofstream sh(outDir / "Run.sh");
            sh << "#!/usr/bin/env bash\n";
            sh << "DIR=\"$(cd \"$(dirname \"$0\")\" && pwd)\"\n";
            sh << "\"$DIR/" << playerName << "\" \"$DIR/scene.json\"\n";
            sh.close();
            ::chmod((outDir / "Run.sh").string().c_str(), 0755);
        }
#endif

        ViewportPanel::AppendLog(std::string("[EXPORT] OK -> ") + outDir.string());
        ViewportPanel::AppendLog("[EXPORT] Tips: ejecutá GameProtoGenPlayer (o Run.bat/Run.sh).");
        return true;
    }

} // namespace Exporter
