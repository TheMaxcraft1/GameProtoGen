cmake_minimum_required(VERSION 3.25)
project(GameProtoGen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Recomendado: si usás Ninja, que los libs salgan a un lugar predecible
# set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

# ---------- SFML 3 ----------
# (Desactiva tests/ejemplos para acelerar)
set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_TESTING  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(sfml
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.0
)
FetchContent_MakeAvailable(sfml)
# Targets que exporta: SFML::System SFML::Window SFML::Graphics ...

# ---------- Dear ImGui (rama docking) ----------
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9-docking
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
# Esto crea un target 'imgui' (lib) + incluye headers <imgui.h>

# ---------- ImGui-SFML 3.0 ----------
# IMPORTANTE:
# - Esta versión soporta SFML 3.
# - Como ya tenemos SFML e ImGui en el mismo árbol, no necesitamos que los "encuentre" fuera.
# - Por defecto ImGui-SFML intentará find_package(SFML 3) y 'imgui' target, que ya existen.

# (Opcional) apaga su build de ejemplo/tests para acelerar
set(IMGUI_SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(IMGUI_SFML_BUILD_TESTING  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(imgui-sfml
  GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
  GIT_TAG master
)
FetchContent_MakeAvailable(imgui-sfml)
# Target: ImGui-SFML::ImGui-SFML



# ---------- Tu ejecutable ----------
add_executable(GameProtoGen
  GameProtoGen.cpp
  GameProtoGen.h
)

target_link_libraries(GameProtoGen PRIVATE
  SFML::Graphics
  SFML::Window
  SFML::System
  ImGui-SFML::ImGui-SFML
)

target_include_directories(GameProtoGen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
