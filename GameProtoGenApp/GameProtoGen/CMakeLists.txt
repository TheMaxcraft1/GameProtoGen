cmake_minimum_required(VERSION 3.25)
project(GameProtoGen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Salidas predecibles
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

# --- Lua 5.4 ---
FetchContent_Declare(lua
  GIT_REPOSITORY https://github.com/lua/lua.git
  GIT_TAG v5.4.7
)
FetchContent_MakeAvailable(lua)
add_library(lua_static ${lua_SOURCE_DIR}/lapi.c ${lua_SOURCE_DIR}/lauxlib.c
    ${lua_SOURCE_DIR}/lbaselib.c ${lua_SOURCE_DIR}/lcode.c ${lua_SOURCE_DIR}/lcorolib.c
    ${lua_SOURCE_DIR}/lctype.c ${lua_SOURCE_DIR}/ldblib.c ${lua_SOURCE_DIR}/ldebug.c
    ${lua_SOURCE_DIR}/ldo.c ${lua_SOURCE_DIR}/ldump.c ${lua_SOURCE_DIR}/lfunc.c
    ${lua_SOURCE_DIR}/lgc.c ${lua_SOURCE_DIR}/linit.c ${lua_SOURCE_DIR}/liolib.c
    ${lua_SOURCE_DIR}/llex.c ${lua_SOURCE_DIR}/lmathlib.c ${lua_SOURCE_DIR}/lmem.c
    ${lua_SOURCE_DIR}/loadlib.c ${lua_SOURCE_DIR}/lobject.c ${lua_SOURCE_DIR}/lopcodes.c
    ${lua_SOURCE_DIR}/loslib.c ${lua_SOURCE_DIR}/lparser.c ${lua_SOURCE_DIR}/lstate.c
    ${lua_SOURCE_DIR}/lstring.c ${lua_SOURCE_DIR}/lstrlib.c ${lua_SOURCE_DIR}/ltable.c
    ${lua_SOURCE_DIR}/ltablib.c ${lua_SOURCE_DIR}/ltm.c ${lua_SOURCE_DIR}/lundump.c
    ${lua_SOURCE_DIR}/lutf8lib.c ${lua_SOURCE_DIR}/lvm.c ${lua_SOURCE_DIR}/lzio.c)
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR})

# --- sol2 ---
FetchContent_Declare(sol2
  GIT_REPOSITORY https://github.com/ThePhD/sol2.git
  GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

# --- SFML 3 ---
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sfml
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.0
)
FetchContent_MakeAvailable(sfml)

# --- nlohmann/json ---
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# --- tinyfiledialogs ---
FetchContent_Declare(tinyfiledialogs
  GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git
  GIT_TAG master
)
FetchContent_MakeAvailable(tinyfiledialogs)
add_library(tinyfiledialogs_lib ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs_lib PUBLIC ${tinyfiledialogs_SOURCE_DIR})

# --- cpr ---
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE) # tu setting que no falla
FetchContent_Declare(cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.11.1
)
FetchContent_MakeAvailable(cpr)

# --- ImGui ---
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9-docking
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# --- ImGui-SFML ---
set(IMGUI_SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(IMGUI_SFML_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
FetchContent_Declare(imgui-sfml
  GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
  GIT_TAG master
)
FetchContent_MakeAvailable(imgui-sfml)

# ----------- AUTH deps -----------
# cpp-httplib
FetchContent_Declare(cpp-httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(cpp-httplib) # target: httplib::httplib

# --- cppcodec (INTERFACE) ---
FetchContent_Declare(cppcodec
  GIT_REPOSITORY https://github.com/tplgy/cppcodec.git
  GIT_TAG v0.2
)
FetchContent_GetProperties(cppcodec)
if (NOT cppcodec_POPULATED)
  FetchContent_Populate(cppcodec)
  if (NOT TARGET cppcodec)
    add_library(cppcodec INTERFACE)
    target_include_directories(cppcodec INTERFACE ${cppcodec_SOURCE_DIR})
  endif()
endif()
if (NOT EXISTS "${cppcodec_SOURCE_DIR}/cppcodec/base64_url_unpadded.hpp")
  message(FATAL_ERROR "No se encontró ${cppcodec_SOURCE_DIR}/cppcodec/base64_url_unpadded.hpp")
endif()

# picosha2 (header-only)
FetchContent_Declare(picosha2
  GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
  GIT_TAG master
)
FetchContent_MakeAvailable(picosha2)

# ===============================
#   LIBRERÍA RUNTIME (sin ImGui)
# ===============================
add_library(gp_runtime STATIC
    # Core (sin Application.cpp)
    Core/SFMLWindow.cpp
    Core/Log.h
    Core/Log.cpp

    # ECS
    ECS/Scene.cpp
    ECS/SceneSerializer.cpp

    # Systems
    Systems/Renderer2D.cpp
    Systems/PhysicsSystem.cpp
    Systems/ScriptVM.cpp
    Systems/ScriptSystem.cpp

    # Runtime
    Runtime/GameRunner.cpp
    Runtime/GameRunner.h
    Runtime/EditorContext.h
    Runtime/SceneContext.h
)

target_include_directories(gp_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${sol2_SOURCE_DIR}/include
    ${picosha2_SOURCE_DIR}
)

target_link_libraries(gp_runtime PUBLIC
  SFML::Graphics
  SFML::Window
  SFML::System
  nlohmann_json::nlohmann_json
  lua_static
)

# ======================================
#   EJECUTABLE EDITOR: GameProtoGen
# ======================================
add_executable(GameProtoGen
    # Core del Editor (con ImGui)
    Core/Application.cpp

    # Editor main
    Editor/EditorApp.cpp
    
    # Panels y capas
    Editor/Panels/ViewportPanel.cpp
    Editor/Panels/InspectorPanel.cpp
    Editor/Panels/ChatPanel.cpp
    Editor/LauncherLayer.h
    Editor/LauncherLayer.cpp
    Editor/ImGuiCoreLayer.h
    Editor/ImGuiCoreLayer.cpp
    Editor/EditorDockLayer.h
    Editor/EditorDockLayer.cpp
    Editor/EditorLogSink.h

    # Net/Auth (solo Editor)
    Net/ApiClient.cpp
    Auth/PKCE.h
    Auth/PKCE.cpp
    Auth/OidcClient.h
    Auth/OidcClient.cpp
    Auth/LoopbackServer.h
    Auth/LoopbackServer.cpp
    Auth/TokenManager.h

    # ImGui extra: std::string helpers (solo Editor)
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_link_libraries(GameProtoGen PRIVATE
  gp_runtime
  ImGui-SFML::ImGui-SFML
  nlohmann_json::nlohmann_json
  cpr::cpr
  tinyfiledialogs_lib
  httplib::httplib
  cppcodec
)

target_include_directories(GameProtoGen PRIVATE
    ${IMGUI_DIR}                 # para imgui.h
    ${IMGUI_DIR}/misc/cpp        # para imgui_stdlib.h
    ${cppcodec_SOURCE_DIR}
)

# Copiar Assets junto al .exe (Editor)
add_custom_command(TARGET GameProtoGen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets
            $<TARGET_FILE_DIR:GameProtoGen>/Assets
)

if (WIN32)
  target_link_libraries(GameProtoGen PRIVATE comdlg32)
endif()

if (MSVC)
  add_compile_options("/utf-8")
endif()
