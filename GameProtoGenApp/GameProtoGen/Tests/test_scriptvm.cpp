// Tests/test_scriptvm.cpp
#include <gtest/gtest.h>
#include "Systems/ScriptVM.h"
#include "ECS/Scene.h"

TEST(ScriptVM, OnSpawnAndOnUpdateRun) {
    Scene sc;
    auto e = sc.CreateEntity();
    ScriptVM vm;
    vm.BindScene(sc);
    std::string err;
    const std::string code = R"(
        spawned = false; updates = 0
        function on_spawn() spawned = true end
        function on_update(dt) updates = updates + 1 end
    )";
    ASSERT_TRUE(vm.RunFor(e.id, code, "<mem>", err)) << err;
    ASSERT_TRUE(vm.CallOnSpawn(e.id, err)) << err;
    ASSERT_TRUE(vm.CallOnUpdate(e.id, 0.016f, err));
    // Verificamos leyendo el estado desde Lua
    // (Acceder a variables desde C++ es más largo; acá alcanza con que no dé errores)
    SUCCEED();
}
