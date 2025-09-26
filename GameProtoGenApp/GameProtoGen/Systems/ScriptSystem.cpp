#include "ScriptSystem.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Editor/Panels/ViewportPanel.h"

using Systems::ScriptSystem;

static ScriptVM* g_vm = nullptr;
ScriptVM& ScriptSystem::VM() {
    if (!g_vm) g_vm = new ScriptVM();
    return *g_vm;
}

static std::string LoadFileUtf8(const std::string& p) {
    std::ifstream ifs(p);
    if (!ifs) return {};
    std::ostringstream ss; ss << ifs.rdbuf();
    return ss.str();
}

void ScriptSystem::ResetVM() {
    if (g_vm) g_vm->Reset();
}

void ScriptSystem::Update(Scene& scene, float dt) {
    auto& vm = VM();
    vm.BindScene(scene);

    for (auto& [id, sc] : scene.scripts) {
        if (!scene.transforms.contains(id)) continue; // sólo sobre entidades válidas

        std::string code;
        if (!sc.inlineCode.empty())      code = sc.inlineCode;
        else if (!sc.path.empty())       code = LoadFileUtf8(sc.path);
        if (code.empty()) continue;

        std::string err;
        if (!sc.loaded) {
            if (!vm.RunFor(id, code, sc.path.empty() ? "<inline>" : sc.path, err)) {
                ViewportPanel::AppendLog(std::string("[SCRIPT] Error run: ") + err);
                continue;
            }
            if (!vm.CallOnSpawn(id, err)) {
                ViewportPanel::AppendLog(std::string("[SCRIPT] Error on_spawn: ") + err);
            }
            else {
                ViewportPanel::AppendLog("[SCRIPT] on_spawn OK id=" + std::to_string(id));
            }
            sc.loaded = true;
        }

        if (!vm.CallOnUpdate(id, dt, err)) {
            ViewportPanel::AppendLog(std::string("[SCRIPT] Error on_update: ") + err);
        }
    }
}
