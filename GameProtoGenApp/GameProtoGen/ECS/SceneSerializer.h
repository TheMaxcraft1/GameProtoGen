#pragma once
#include <string>
#include <nlohmann/json.hpp>
class Scene;

class SceneSerializer {
public:
    // Persistencia a disco (ya existente)
    static bool Save(const Scene& scene, const std::string& path);
    static bool Load(Scene& scene, const std::string& path);

    // (de)serialización en memoria
    static nlohmann::json Dump(const Scene& scene);
    static bool LoadFromJson(Scene& scene, const nlohmann::json& j);
};
