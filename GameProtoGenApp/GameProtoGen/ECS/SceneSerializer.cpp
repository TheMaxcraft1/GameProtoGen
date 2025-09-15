#include "SceneSerializer.h"
#include "Scene.h"
#include "Components.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static json color_to_json(const sf::Color& c) {
    return { {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
}
static sf::Color color_from_json(const json& j) {
    return sf::Color(j.value("r", 255), j.value("g", 255), j.value("b", 255), j.value("a", 255));
}

static json dump_impl(const Scene& scene) {
    json j;
    j["entities"] = json::array();

    for (auto& e : scene.Entities()) {
        const auto id = e.id;
        json je;
        je["id"] = id;

        if (auto it = scene.transforms.find(id); it != scene.transforms.end()) {
            const auto& t = it->second;
            je["Transform"] = {
                {"position", {t.position.x, t.position.y}},
                {"scale",    {t.scale.x, t.scale.y}},
                {"rotation", t.rotationDeg}
            };
        }
        if (auto it = scene.sprites.find(id); it != scene.sprites.end()) {
            const auto& s = it->second;
            je["Sprite"] = {
                {"size",  {s.size.x, s.size.y}},
                {"color", color_to_json(s.color)}
            };
        }
        if (auto it = scene.colliders.find(id); it != scene.colliders.end()) {
            const auto& c = it->second;
            je["Collider"] = {
                {"halfExtents", {c.halfExtents.x, c.halfExtents.y}},
                {"offset",      {c.offset.x, c.offset.y}}
            };
        }
        if (auto it = scene.physics.find(id); it != scene.physics.end()) {
            const auto& p = it->second;
            je["Physics2D"] = {
                {"velocity", {p.velocity.x, p.velocity.y}},
                {"gravity", p.gravity},
                {"gravityEnabled", p.gravityEnabled}
            };
        }
        if (auto it = scene.playerControllers.find(id); it != scene.playerControllers.end()) {
            const auto& pc = it->second;
            je["PlayerController"] = {
                {"moveSpeed", pc.moveSpeed},
                {"jumpSpeed", pc.jumpSpeed}
            };
        }
        j["entities"].push_back(je);
    }
    return j;
}

bool SceneSerializer::Save(const Scene& scene, const std::string& path) {
    json j = dump_impl(scene);
    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return true;
}

bool SceneSerializer::Load(Scene& scene, const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return false;
    json j; ifs >> j;

    // Reiniciar escena
    scene = Scene{};

    for (auto& je : j["entities"]) {
        Entity e;
        if (je.contains("id") && (je["id"].is_number_unsigned() || je["id"].is_number_integer())) {
            // Tomamos el id del archivo y lo preservamos
            const EntityID wanted = je["id"].get<EntityID>();
            if (wanted > 0) e = scene.CreateEntityWithId(wanted);
            else            e = scene.CreateEntity(); // fallback defensivo
        }
        else {
            e = scene.CreateEntity(); // si no había id en el JSON
        }
        const EntityID id = e.id;

        if (je.contains("Transform")) {
            auto& t = scene.transforms[id];
            auto jt = je["Transform"];
            t.position = { jt["position"][0].get<float>(), jt["position"][1].get<float>() };
            t.scale = { jt["scale"][0].get<float>(), jt["scale"][1].get<float>() };
            t.rotationDeg = jt["rotation"].get<float>();
        }
        if (je.contains("Sprite")) {
            auto& s = scene.sprites[id];
            auto js = je["Sprite"];
            s.size = { js["size"][0].get<float>(), js["size"][1].get<float>() };
            s.color = color_from_json(js["color"]);
        }
        if (je.contains("Collider")) {
            auto& c = scene.colliders[id];
            auto jc = je["Collider"];
            c.halfExtents = { jc["halfExtents"][0].get<float>(), jc["halfExtents"][1].get<float>() };
            c.offset = { jc["offset"][0].get<float>(), jc["offset"][1].get<float>() };
        }
        if (je.contains("Physics2D")) {
            auto& p = scene.physics[id];
            auto jp = je["Physics2D"];
            p.velocity = { jp["velocity"][0].get<float>(), jp["velocity"][1].get<float>() };
            p.gravity = jp.value("gravity", 2000.f);
            p.gravityEnabled = jp.value("gravityEnabled", true);
            p.onGround = false;
        }
        if (je.contains("PlayerController")) {
            auto& pc = scene.playerControllers[id];
            auto jpc = je["PlayerController"];
            pc.moveSpeed = jpc.value("moveSpeed", 500.f);
            pc.jumpSpeed = jpc.value("jumpSpeed", 900.f);
        }
    }
    return true;
}

nlohmann::json SceneSerializer::Dump(const Scene& scene) {
    return dump_impl(scene);
}

bool SceneSerializer::LoadFromJson(Scene& scene, const nlohmann::json& j) {
    if (!j.contains("entities") || !j["entities"].is_array()) return false;
    // Versión en memoria equivalente a Load()
    scene = Scene{};
    for (auto& je : j["entities"]) {
        Entity e;
        if (je.contains("id") && (je["id"].is_number_unsigned() || je["id"].is_number_integer())) {
            // Tomamos el id del archivo y lo preservamos
            const EntityID wanted = je["id"].get<EntityID>();
            if (wanted > 0) e = scene.CreateEntityWithId(wanted);
            else            e = scene.CreateEntity(); // fallback defensivo
        }
        else {
            e = scene.CreateEntity(); // si no había id en el JSON
        }
        const EntityID id = e.id;


        if (je.contains("Transform")) {
            auto& t = scene.transforms[id];
            auto jt = je["Transform"];
            t.position = { jt["position"][0].get<float>(), jt["position"][1].get<float>() };
            t.scale = { jt["scale"][0].get<float>(), jt["scale"][1].get<float>() };
            t.rotationDeg = jt["rotation"].get<float>();
        }
        if (je.contains("Sprite")) {
            auto& s = scene.sprites[id];
            auto js = je["Sprite"];
            s.size = { js["size"][0].get<float>(), js["size"][1].get<float>() };
            s.color = color_from_json(js["color"]);
        }
        if (je.contains("Collider")) {
            auto& c = scene.colliders[id];
            auto jc = je["Collider"];
            c.halfExtents = { jc["halfExtents"][0].get<float>(), jc["halfExtents"][1].get<float>() };
            c.offset = { jc["offset"][0].get<float>(), jc["offset"][1].get<float>() };
        }
        if (je.contains("Physics2D")) {
            auto& p = scene.physics[id];
            auto jp = je["Physics2D"];
            p.velocity = { jp["velocity"][0].get<float>(), jp["velocity"][1].get<float>() };
            p.gravity = jp.value("gravity", 2000.f);
            p.gravityEnabled = jp.value("gravityEnabled", true);
            p.onGround = false;
        }
        if (je.contains("PlayerController")) {
            auto& pc = scene.playerControllers[id];
            auto jpc = je["PlayerController"];
            pc.moveSpeed = jpc.value("moveSpeed", 500.f);
            pc.jumpSpeed = jpc.value("jumpSpeed", 900.f);
        }
    }
    return true;
}
