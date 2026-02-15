cmake_minimum_required(VERSION 3.20)
project(Eruption LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /wd4100 /wd4201)  # /wd4100: unreferenced param, /wd4201: nameless struct
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Include dependencies
include(cmake/Dependencies.cmake)

# =============================================================================
# Helper: Configure Windows subsystem (console + Shell API)
# =============================================================================
function(configure_win32_target TARGET)
    if(WIN32)
        target_link_libraries(${TARGET} PRIVATE ole32 uuid shell32)
        if(MSVC)
            target_link_options(${TARGET} PRIVATE /ENTRY:mainCRTStartup)
            target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endif()
endfunction()

# =============================================================================
# Engine Library
# =============================================================================
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp"
)

add_library(eruption_engine STATIC ${ENGINE_SOURCES})

target_include_directories(eruption_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(eruption_engine
    PUBLIC
        EnTT::EnTT
        nlohmann_json::nlohmann_json
        box2d          # Public headers (Rigidbody.h etc.) expose box2d types
    PRIVATE
        glfw
        glad
        lz4
        stb
)

# =============================================================================
# Shared Runtime Library
# =============================================================================
# Code shared between the editor and the standalone game runtime.
# This eliminates compiling the same source files into two separate targets.
set(RUNTIME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/core/EditorComponents.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/core/RuntimeContext.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/core/PhysicsPlayback.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/core/SimulationPlayback.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/serialization/SceneSerializer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/scripting/DLLManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/scripting/ScriptManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/scripting/ScriptCompiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/scripting/ScriptWatcher.cpp"
)

add_library(eruption_runtime STATIC ${RUNTIME_SOURCES})

target_include_directories(eruption_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(eruption_runtime
    PUBLIC
        eruption_engine
    PRIVATE
        imgui          # DLLManager.cpp includes imgui.h
)

# =============================================================================
# ImGui Library
# =============================================================================
add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)
target_link_libraries(imgui PUBLIC glfw glad)

# =============================================================================
# PixArt Tool
# =============================================================================
file(GLOB PIXART_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pixart/*.cpp"
)

add_executable(pixart WIN32 ${PIXART_SOURCES})

target_link_libraries(pixart PRIVATE
    eruption_engine
    imgui
)

configure_win32_target(pixart)

# =============================================================================
# Editor Executable
# =============================================================================
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/*.cpp"
)

# Remove sources already compiled in eruption_runtime to avoid duplicate symbols
list(REMOVE_ITEM EDITOR_SOURCES ${RUNTIME_SOURCES})

add_executable(eruption_editor WIN32 ${EDITOR_SOURCES})

target_link_libraries(eruption_editor PRIVATE
    eruption_runtime
    imgui
    stb            # AssetPreviewPanel.cpp includes stb_image.h directly
)

configure_win32_target(eruption_editor)

# =============================================================================
# Asset Copy Helper
# =============================================================================
# copy_runtime_assets(<target_name> <exe_target> [EXTRA_DIRS dir1 dir2 ...])
# Copies shaders, materials, and engine_config.json to the exe's output directory.
# EXTRA_DIRS are additional asset/ subdirectories to copy.
function(copy_runtime_assets COPY_TARGET EXE_TARGET)
    cmake_parse_arguments(ARG "" "" "EXTRA_DIRS" ${ARGN})
    set(COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:${EXE_TARGET}>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/materials"
            "$<TARGET_FILE_DIR:${EXE_TARGET}>/assets/materials"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/engine_config.json"
            "$<TARGET_FILE_DIR:${EXE_TARGET}>/engine_config.json"
    )
    foreach(DIR ${ARG_EXTRA_DIRS})
        list(APPEND COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/${DIR}"
                "$<TARGET_FILE_DIR:${EXE_TARGET}>/assets/${DIR}"
        )
    endforeach()
    add_custom_target(${COPY_TARGET} ALL ${COMMANDS} DEPENDS ${EXE_TARGET})
endfunction()

# Copy assets for editor (includes defaults and fonts)
copy_runtime_assets(copy_editor_assets eruption_editor EXTRA_DIRS defaults fonts)

# =============================================================================
# Game Runtime Executable
# =============================================================================
add_executable(eruption_game WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/GameApplication.cpp"
)

target_link_libraries(eruption_game PRIVATE
    eruption_runtime
    imgui
)

configure_win32_target(eruption_game)

# Copy assets for game runtime (shaders, materials, config only)
copy_runtime_assets(copy_game_assets eruption_game)

# =============================================================================
# Bundle CMake with Editor (Windows only)
# =============================================================================
if(WIN32)
    add_custom_target(bundle_cmake ALL
        COMMAND ${CMAKE_COMMAND}
            -DOUTPUT_DIR="$<TARGET_FILE_DIR:eruption_editor>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundleCMake.cmake"
        COMMENT "Bundling CMake with the editor..."
        DEPENDS eruption_editor
    )
endif()
