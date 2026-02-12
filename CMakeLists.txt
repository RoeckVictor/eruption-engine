cmake_minimum_required(VERSION 3.20)
project(Eruption LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include dependencies
include(cmake/Dependencies.cmake)

# =============================================================================
# Engine Library
# =============================================================================
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.h"
)

add_library(eruption_engine STATIC ${ENGINE_SOURCES})

target_include_directories(eruption_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(eruption_engine PUBLIC
    glfw
    glad
    EnTT::EnTT
    nlohmann_json::nlohmann_json
    box2d
    lz4
    stb
)

# =============================================================================
# PixArt Tool
# =============================================================================
file(GLOB PIXART_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pixart/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/pixart/*.h"
)

# PixArt needs PixelGridFile and VFS from engine for .pxg format support
# Logger is also needed since these files use the logging system
list(APPEND PIXART_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/asset/PixelGridFile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/asset/VFS.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/core/Logger.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/platform/PlatformUtils.cpp"
)

# ImGui library for PixArt
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

add_executable(pixart WIN32 ${PIXART_SOURCES})

target_include_directories(pixart PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(pixart PRIVATE
    glfw
    glad
    imgui
    lz4
    stb
    nlohmann_json::nlohmann_json
)

# Link Windows Shell API for file/folder dialogs
if(WIN32)
    target_link_libraries(pixart PRIVATE ole32 uuid shell32)
endif()

# Use main() as entry point despite WIN32 subsystem
if(MSVC)
    set_target_properties(pixart PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# =============================================================================
# Editor Executable
# =============================================================================
file(GLOB_RECURSE EDITOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor/*.h"
)

add_executable(eruption_editor WIN32 ${EDITOR_SOURCES})

target_include_directories(eruption_editor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(eruption_editor PRIVATE
    eruption_engine
    imgui
)

# Link Windows Shell API for folder dialogs
if(WIN32)
    target_link_libraries(eruption_editor PRIVATE ole32 uuid shell32)
endif()

# Use main() as entry point despite WIN32 subsystem
if(MSVC)
    set_target_properties(eruption_editor PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

# =============================================================================
# Copy Shaders, Materials, Default Assets, and Config (for editor)
# =============================================================================
add_custom_target(copy_editor_assets ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Copying shaders to editor build directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:eruption_editor>/shaders"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying materials to editor build directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/materials"
        "$<TARGET_FILE_DIR:eruption_editor>/assets/materials"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying default assets to editor build directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/defaults"
        "$<TARGET_FILE_DIR:eruption_editor>/assets/defaults"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying fonts to editor build directory"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
        "$<TARGET_FILE_DIR:eruption_editor>/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/engine_config.json"
        "$<TARGET_FILE_DIR:eruption_editor>/engine_config.json"
    DEPENDS eruption_editor
)

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
