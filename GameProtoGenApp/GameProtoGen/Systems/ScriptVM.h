#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <SFML/System/Vector2.hpp>
#include "ECS/Scene.h"

#include <sol/sol.hpp>

class ScriptVM {
public:
    ScriptVM();
    ~ScriptVM();

    bool RunFor(EntityID id, const std::string& code, const std::string& pathHint, std::string& err);
    bool CallOnSpawn(EntityID id, std::string& err);
    bool CallOnUpdate(EntityID id, float dt, std::string& err);

    void BindScene(Scene& scene);

private:
    struct PerEntity {
        sol::environment env;
    };

    std::unique_ptr<sol::state> m_L;
    std::unordered_map<EntityID, PerEntity> m_envs;
    Scene* m_scene = nullptr;

    void RegisterApi();
    bool EnsureEnv(EntityID id);
};
