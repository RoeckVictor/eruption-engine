include(FetchContent)

# GLFW - windowing and input
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# EnTT - Entity Component System
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.14.0
    GIT_SHALLOW TRUE
)

# nlohmann/json - JSON parsing (for prefabs, animation clips, asset manifests)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

# Box2D - 2D physics engine (v3.x, C API)
FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG v3.1.1
    GIT_SHALLOW TRUE
)
set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_VALIDATE OFF CACHE BOOL "" FORCE)

# Dear ImGui - immediate mode GUI (for PixArt editor tool)
# Using the docking branch for dockable windows support.
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)

FetchContent_MakeAvailable(glfw entt nlohmann_json box2d imgui)

# glad - pre-generated OpenGL 4.5 core loader (in external/glad/)
add_library(glad STATIC
    "${PROJECT_SOURCE_DIR}/external/glad/src/gl.c"
)
target_include_directories(glad PUBLIC
    "${PROJECT_SOURCE_DIR}/external/glad/include"
)

# LZ4 - fast compression (for .pxg pixel grid file format)
add_library(lz4 STATIC
    "${PROJECT_SOURCE_DIR}/external/lz4/lz4.c"
)
target_include_directories(lz4 PUBLIC
    "${PROJECT_SOURCE_DIR}/external/lz4"
)

# stb - header-only utility libraries (stb_image, stb_perlin)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE
    "${PROJECT_SOURCE_DIR}/external/stb"
)
