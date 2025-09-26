#pragma once
#include "ECS/Scene.h"
#include "ScriptVM.h"

namespace Systems {
    class ScriptSystem {
    public:
        static void Update(Scene& scene, float dt);
    private:
        static ScriptVM& VM(); // singleton simple
    };
}
