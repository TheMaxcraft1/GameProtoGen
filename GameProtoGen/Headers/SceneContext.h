#pragma once
#include <imgui.h>

// Estado compartido temporal (hasta que metamos ECS/Scene).
struct SceneContext {
    float radius = 50.f;
    ImVec4 color = ImVec4(0, 1, 0, 1);

    // Singleton simple para este MVP
    static SceneContext& Get() {
        static SceneContext ctx;
        return ctx;
    }
};
