#include "Headers/SceneSerializer.h"
#include "Headers/Scene.h"
#include "Headers/Components.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static json color_to_json(const sf::Color& c) {
    return { {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
}
static sf::Color color_from_json(const json& j) {
    return sf::Color(j.value("r", 255), j.value("g", 255), j.value("b", 255), j.value("a", 255));
}

bool SceneSerializer::Save(const Scene& scene, const std::string& path) {
    json j;
    j["entities"] = json::array();

    for (auto& e : scene.Entities()) {
        json je;
        je["id"] = e.id;

        if (auto it = scene.transforms.find(e.id); it != scene.transforms.end()) {
            const auto& t = it->second;
            je["Transform"] = {
                {"position", {t.position.x, t.position.y}},
                {"scale",    {t.scale.x, t.scale.y}},
                {"rotation", t.rotationDeg}
            };
        }
        if (auto it = scene.sprites.find(e.id); it != scene.sprites.end()) {
            const auto& s = it->second;
            je["Sprite"] = {
                {"size",  {s.size.x, s.size.y}},
                {"color", color_to_json(s.color)}
            };
        }
        if (auto it = scene.colliders.find(e.id); it != scene.colliders.end()) {
            const auto& c = it->second;
            je["Collider"] = {
                {"halfExtents", {c.halfExtents.x, c.halfExtents.y}},
                {"offset",      {c.offset.x, c.offset.y}}
            };
        }

        j["entities"].push_back(je);
    }

    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return true;
}

bool SceneSerializer::Load(Scene& scene, const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    json j; ifs >> j;

    // Reinicia escena simple (MVP): destruye y vuelve a crear
    // (podrías limpiar mejor si lo necesitás)
    for (auto& e : scene.Entities()) { /* no-op, MVP */ }
    scene = Scene{};

    for (auto& je : j["entities"]) {
        Entity e = scene.CreateEntity();
        // respetar ID guardado (MVP: no re-map; si querés, ajustá m_Next)
        if (je.contains("id")) {
            e.id = je["id"].get<EntityID>();
        }

        if (je.contains("Transform")) {
            auto& t = scene.transforms[e.id];
            auto jt = je["Transform"];
            t.position = { jt["position"][0].get<float>(), jt["position"][1].get<float>() };
            t.scale = { jt["scale"][0].get<float>(), jt["scale"][1].get<float>() };
            t.rotationDeg = jt["rotation"].get<float>();
        }
        if (je.contains("Sprite")) {
            auto& s = scene.sprites[e.id];
            auto js = je["Sprite"];
            s.size = { js["size"][0].get<float>(), js["size"][1].get<float>() };
            s.color = color_from_json(js["color"]);
        }
        if (je.contains("Collider")) {
            auto& c = scene.colliders[e.id];
            auto jc = je["Collider"];
            c.halfExtents = { jc["halfExtents"][0].get<float>(), jc["halfExtents"][1].get<float>() };
            c.offset = { jc["offset"][0].get<float>(), jc["offset"][1].get<float>() };
        }
    }
    return true;
}
