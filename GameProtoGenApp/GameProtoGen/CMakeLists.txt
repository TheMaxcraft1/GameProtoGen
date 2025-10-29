cmake_minimum_required(VERSION 3.25)
project(GameProtoGen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Switches para elegir qué compilar
option(BUILD_EDITOR "Build the Editor executable" ON)
option(BUILD_PLAYER "Build the Player executable" ON)

# Salidas predecibles
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

# --- Lua 5.4 ---
FetchContent_Declare(lua GIT_REPOSITORY https://github.com/lua/lua.git GIT_TAG v5.4.7)
FetchContent_MakeAvailable(lua)
add_library(lua_static
    ${lua_SOURCE_DIR}/lapi.c ${lua_SOURCE_DIR}/lauxlib.c
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
FetchContent_Declare(sol2 GIT_REPOSITORY https://github.com/ThePhD/sol2.git GIT_TAG v3.3.0)
FetchContent_MakeAvailable(sol2)

# --- SFML 3 ---
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sfml GIT_REPOSITORY https://github.com/SFML/SFML.git GIT_TAG 3.0.0)
FetchContent_MakeAvailable(sfml)

# --- nlohmann/json ---
FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

# --- tinyfiledialogs ---
FetchContent_Declare(tinyfiledialogs GIT_REPOSITORY https://github.com/native-toolkit/tinyfiledialogs.git GIT_TAG master)
FetchContent_MakeAvailable(tinyfiledialogs)
add_library(tinyfiledialogs_lib ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c)
target_include_directories(tinyfiledialogs_lib PUBLIC ${tinyfiledialogs_SOURCE_DIR})
if (MSVC)
  # Evitar que tinyfiledialogs sufra por LEAN_AND_MEAN (necesita OPENFILENAME, CHOOSECOLOR, etc.)
  target_compile_options(tinyfiledialogs_lib PRIVATE /UWIN32_LEAN_AND_MEAN)
endif()

# --- cpr ---
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 1.11.1)
FetchContent_MakeAvailable(cpr)

# --- ImGui + ImGui-SFML (solo si compila el editor) ---
if (BUILD_EDITOR)
  FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.91.9-docking)
  FetchContent_MakeAvailable(imgui)
  set(IMGUI_DIR ${imgui_SOURCE_DIR})

  set(IMGUI_SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(IMGUI_SFML_BUILD_TESTING  OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(imgui-sfml GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git GIT_TAG master)
  FetchContent_MakeAvailable(imgui-sfml)
endif()

# --- cpp-httplib (header-only) ---
FetchContent_Declare(cpp-httplib GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git GIT_TAG v0.15.3)
FetchContent_MakeAvailable(cpp-httplib)

# --- cppcodec (header-only) ---
FetchContent_Declare(cppcodec GIT_REPOSITORY https://github.com/tplgy/cppcodec.git GIT_TAG v0.2)
FetchContent_GetProperties(cppcodec)
if (NOT cppcodec_POPULATED)
  FetchContent_Populate(cppcodec)
  if (NOT TARGET cppcodec)
    add_library(cppcodec INTERFACE)
    target_include_directories(cppcodec INTERFACE ${cppcodec_SOURCE_DIR})
  endif()
endif()

# --- picosha2 (header-only) ---
FetchContent_Declare(picosha2 GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git GIT_TAG master)
FetchContent_MakeAvailable(picosha2)

# =========================
#   LIB: gp_core (engine)
# =========================
add_library(gp_core STATIC
    Core/SFMLWindow.cpp
    ECS/Scene.cpp
    ECS/SceneSerializer.cpp
    Systems/Renderer2D.cpp
    Systems/PhysicsSystem.cpp
    Systems/ScriptVM.cpp
    Systems/ScriptSystem.cpp
)
target_link_libraries(gp_core PUBLIC
  SFML::Graphics SFML::Window SFML::System
  lua_static
  nlohmann_json::nlohmann_json
)
target_include_directories(gp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${sol2_SOURCE_DIR}/include
)

if (MSVC)
  target_compile_options(gp_core PRIVATE "/utf-8")
  target_compile_definitions(gp_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# =========================
#   LIB: gp_editor (UI) — solo si BUILD_EDITOR
# =========================
if (BUILD_EDITOR)
  add_library(gp_editor STATIC
      Editor/ImGuiCoreLayer.cpp
      Editor/EditorDockLayer.cpp
      Editor/LauncherLayer.cpp

      Editor/Panels/ViewportPanel.cpp
      Editor/Panels/InspectorPanel.cpp
      Editor/Panels/ChatPanel.cpp

      Editor/ExportPlayable.cpp

      Net/ApiClient.cpp
      Auth/PKCE.cpp
      Auth/OidcClient.cpp
      Auth/LoopbackServer.cpp

      ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
  )

  target_link_libraries(gp_editor PUBLIC
    gp_core
    ImGui-SFML::ImGui-SFML
    cpr::cpr
    httplib::httplib
    tinyfiledialogs_lib
    cppcodec
  )

  target_include_directories(gp_editor PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${IMGUI_DIR}
      ${IMGUI_DIR}/misc/cpp
      ${picosha2_SOURCE_DIR}
      ${cppcodec_SOURCE_DIR}
  )

  if (TARGET ImGui-SFML::ImGui-SFML)
    get_target_property(IMGUI_SFML_INC ImGui-SFML::ImGui-SFML INTERFACE_INCLUDE_DIRECTORIES)
    if (IMGUI_SFML_INC)
      target_include_directories(gp_editor PUBLIC ${IMGUI_SFML_INC})
    endif()
  endif()

  if (TARGET httplib::httplib)
    get_target_property(HTTPLIB_INC httplib::httplib INTERFACE_INCLUDE_DIRECTORIES)
    if (HTTPLIB_INC)
      target_include_directories(gp_editor PUBLIC ${HTTPLIB_INC})
    endif()
  endif()
  target_include_directories(gp_editor PUBLIC ${cpp-httplib_SOURCE_DIR})

  if (MSVC)
    target_compile_options(gp_editor PRIVATE "/utf-8")
    target_compile_definitions(gp_editor PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
  endif()

  # CLAVE: esta define llega a todo lo que consuma gp_editor (EditorFonts.h)
  target_compile_definitions(gp_editor PUBLIC GP_BUILD_EDITOR=1)
  if (WIN32)
    target_link_libraries(gp_editor PUBLIC comdlg32)
  endif()
endif()

# ===================================
#   EXE: GameProtoGen (EDITOR APP)
# ===================================
if (BUILD_EDITOR)
  add_executable(GameProtoGen
      Core/Application.cpp
      Editor/EditorApp.cpp
  )
  target_link_libraries(GameProtoGen PRIVATE gp_editor)
  target_include_directories(GameProtoGen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(GameProtoGen PRIVATE GP_BUILD_EDITOR=1)
  if (WIN32)
    target_link_libraries(GameProtoGen PRIVATE comdlg32)
  endif()
  add_custom_command(TARGET GameProtoGen POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_SOURCE_DIR}/Assets
              $<TARGET_FILE_DIR:GameProtoGen>/Assets
  )
endif()

# ===================================
#   EXE: GameProtoGenPlayer (PLAYER)
# ===================================
if (BUILD_PLAYER)
  add_executable(GameProtoGenPlayer
      PlayerMain.cpp
  )
  target_link_libraries(GameProtoGenPlayer PRIVATE
    gp_core
    SFML::Graphics SFML::Window SFML::System
  )
  target_include_directories(GameProtoGenPlayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(GameProtoGenPlayer PRIVATE GP_BUILD_EDITOR=0)
  add_custom_command(TARGET GameProtoGenPlayer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_SOURCE_DIR}/Assets
              $<TARGET_FILE_DIR:GameProtoGenPlayer>/Assets
  )
endif()

# Salidas por config
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
  if (BUILD_EDITOR)
    set_target_properties(GameProtoGen PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin
      LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin
      ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib
    )
  endif()
  if (BUILD_PLAYER)
    set_target_properties(GameProtoGenPlayer PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin
      LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin
      ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib
    )
  endif()
endforeach()
