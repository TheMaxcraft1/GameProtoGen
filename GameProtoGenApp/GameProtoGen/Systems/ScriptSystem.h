#pragma once
#include "ECS/Scene.h"
#include "ScriptVM.h"

namespace Systems {
    class ScriptSystem {
    public:
        static void Update(Scene& scene, float dt);
        static void ResetVM();

        static void OnTriggerEnter(Scene& scene, EntityID self, EntityID other);
    private:
        static ScriptVM& VM(); // singleton simple
    };
}
