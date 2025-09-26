#include "ScriptVM.h"
#include <sstream>

ScriptVM::ScriptVM() : m_L(std::make_unique<sol::state>()) {
    auto& L = *m_L;
    L.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);
    RegisterApi();
}

ScriptVM::~ScriptVM() = default;

void ScriptVM::BindScene(Scene& scene) {
    m_scene = &scene;
}

bool ScriptVM::EnsureEnv(EntityID id) {
    if (m_envs.find(id) != m_envs.end()) return true;

    auto& L = *m_L;
    // Environment nuevo que hereda de globals
    sol::environment env(L, sol::create, L.globals());
    env["this_id"] = id;

    m_envs.emplace(id, PerEntity{ std::move(env) });
    return true;
}

bool ScriptVM::RunFor(EntityID id, const std::string& code, const std::string& pathHint, std::string& err) {
    EnsureEnv(id);
    auto& L = *m_L;

    sol::protected_function_result r =
        L.safe_script(code, m_envs[id].env, sol::script_pass_on_error, pathHint.c_str());
    if (!r.valid()) {
        sol::error e = r;
        err = e.what();
        return false;
    }
    return true;
}

bool ScriptVM::CallOnSpawn(EntityID id, std::string& err) {
    auto it = m_envs.find(id);
    if (it == m_envs.end()) return true;

    sol::object f = it->second.env["on_spawn"];
    if (f.is<sol::protected_function>()) {
        sol::protected_function pf = f.as<sol::protected_function>();
        auto res = pf();
        if (!res.valid()) { sol::error e = res; err = e.what(); return false; }
    }
    return true;
}

bool ScriptVM::CallOnUpdate(EntityID id, float dt, std::string& err) {
    auto it = m_envs.find(id);
    if (it == m_envs.end()) return true;

    sol::object f = it->second.env["on_update"];
    if (f.is<sol::protected_function>()) {
        sol::protected_function pf = f.as<sol::protected_function>();
        auto res = pf(dt);
        if (!res.valid()) { sol::error e = res; err = e.what(); return false; }
    }
    return true;
}

void ScriptVM::RegisterApi() {
    auto& L = *m_L;
    L["ecs"] = L.create_table();

    // ecs.create() -> id
    L["ecs"]["create"] = [this]() -> EntityID {
        Scene* s = m_scene;
        if (s) { auto e = s->CreateEntity(); return e.id; }
        return 0;
        };

    // ecs.destroy(id)
    L["ecs"]["destroy"] = [this](EntityID id) {
        Scene* s = m_scene;
        if (s) s->DestroyEntity(Entity{ id });
        };

    // ecs.first_with("Component")
    L["ecs"]["first_with"] = [this](const std::string& comp) -> EntityID {
        Scene* s = m_scene;
        if (!s) return 0;
        auto has = [&](auto& map) { return !map.empty() ? map.begin()->first : 0u; };
        if (comp == "Transform")         return has(s->transforms);
        if (comp == "Sprite")            return has(s->sprites);
        if (comp == "Collider")          return has(s->colliders);
        if (comp == "Physics2D")         return has(s->physics);
        if (comp == "PlayerController")  return has(s->playerControllers);
        return 0;
        };

    // ecs.get(id,"Component") -> table|nil
    L["ecs"]["get"] = [this](EntityID id, const std::string& comp) -> sol::object {
        Scene* s = m_scene;
        if (!s) return sol::nil;

        auto& L = *m_L;
        auto to_table_vec2 = [&](sf::Vector2f v) {
            sol::table t = L.create_table();
            t["x"] = v.x; t["y"] = v.y;
            return t;
            };
        auto to_table_color = [&](sf::Color c) {
            sol::table t = L.create_table();
            t["r"] = c.r; t["g"] = c.g; t["b"] = c.b; t["a"] = c.a;
            return t;
            };

        if (comp == "Transform" && s->transforms.contains(id)) {
            auto& t = s->transforms[id];
            sol::table out = L.create_table();
            out["position"] = to_table_vec2(t.position);
            out["scale"] = to_table_vec2(t.scale);
            out["rotation"] = t.rotationDeg;
            return sol::make_object(L, out);
        }
        if (comp == "Sprite" && s->sprites.contains(id)) {
            auto& sp = s->sprites[id];
            sol::table out = L.create_table();
            out["size"] = to_table_vec2(sp.size);
            out["color"] = to_table_color(sp.color);
            return sol::make_object(L, out);
        }
        if (comp == "Collider" && s->colliders.contains(id)) {
            auto& c = s->colliders[id];
            sol::table out = L.create_table();
            out["halfExtents"] = to_table_vec2(c.halfExtents);
            out["offset"] = to_table_vec2(c.offset);
            return sol::make_object(L, out);
        }
        if (comp == "Physics2D" && s->physics.contains(id)) {
            auto& p = s->physics[id];
            sol::table out = L.create_table();
            out["velocity"] = to_table_vec2(p.velocity);
            out["gravity"] = p.gravity;
            out["gravityEnabled"] = p.gravityEnabled;
            out["onGround"] = p.onGround;
            return sol::make_object(L, out);
        }
        if (comp == "PlayerController" && s->playerControllers.contains(id)) {
            auto& pc = s->playerControllers[id];
            sol::table out = L.create_table();
            out["moveSpeed"] = pc.moveSpeed;
            out["jumpSpeed"] = pc.jumpSpeed;
            return sol::make_object(L, out);
        }
        return sol::nil;
        };

    // ecs.set(id,"Component", table)
    L["ecs"]["set"] = [this](EntityID id, const std::string& comp, sol::table v) {
        Scene* s = m_scene;
        if (!s) return;

        auto get_vec2 = [&](sol::object obj) -> sf::Vector2f {
            if (!obj.is<sol::table>()) return { 0.f, 0.f };
            sol::table t = obj.as<sol::table>();
            float x = t.get_or("x", 0.f);
            float y = t.get_or("y", 0.f);
            return { x, y };
            };

        if (comp == "Transform") {
            auto& t = s->transforms[id];
            sol::object p = v["position"];
            sol::object sc = v["scale"];
            sol::object r = v["rotation"];
            if (p.valid())  t.position = get_vec2(p);
            if (sc.valid()) t.scale = get_vec2(sc);
            if (r.valid())  t.rotationDeg = r.as<float>();
        }
        else if (comp == "Sprite") {
            auto& sp = s->sprites[id];
            sol::object sz = v["size"];
            sol::object col = v["color"];
            if (sz.valid()) sp.size = get_vec2(sz);
            if (col.valid() && col.is<sol::table>()) {
                sol::table c = col.as<sol::table>();
                // defaults como unsigned para castear a uint8_t sin warnings
                unsigned r = c.get_or("r", 255u);
                unsigned g = c.get_or("g", 255u);
                unsigned b = c.get_or("b", 255u);
                unsigned a = c.get_or("a", 255u);
                sp.color = sf::Color(
                    static_cast<std::uint8_t>(r),
                    static_cast<std::uint8_t>(g),
                    static_cast<std::uint8_t>(b),
                    static_cast<std::uint8_t>(a)
                );
            }
        }
        else if (comp == "Collider") {
            auto& c = s->colliders[id];
            sol::object he = v["halfExtents"];
            sol::object off = v["offset"];
            if (he.valid())  c.halfExtents = get_vec2(he);
            if (off.valid()) c.offset = get_vec2(off);
        }
        else if (comp == "Physics2D") {
            auto& p = s->physics[id];
            sol::object vel = v["velocity"];
            sol::object g = v["gravity"];
            sol::object ge = v["gravityEnabled"];
            sol::object og = v["onGround"];
            if (vel.valid()) p.velocity = get_vec2(vel);
            if (g.valid())   p.gravity = g.as<float>();
            if (ge.valid())  p.gravityEnabled = ge.as<bool>();
            if (og.valid())  p.onGround = og.as<bool>();
        }
        else if (comp == "PlayerController") {
            auto& pc = s->playerControllers[id];
            sol::object ms = v["moveSpeed"];
            sol::object js = v["jumpSpeed"];
            if (ms.valid()) pc.moveSpeed = ms.as<float>();
            if (js.valid()) pc.jumpSpeed = js.as<float>();
        }
        };
}
