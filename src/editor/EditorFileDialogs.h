#pragma once

#include "engine/platform/PlatformUtils.h"
#include <filesystem>
#include <string>
#include <vector>

namespace editor {

// Pre-defined file filters for common asset types
namespace FileFilters {

inline const std::vector<engine::platform::FileFilter> Animation = {
    {"Animation Clip (*.anim)", "*.anim"}
};

inline const std::vector<engine::platform::FileFilter> AnimatorController = {
    {"Animator Controller (*.animstate)", "*.animstate"}
};

inline const std::vector<engine::platform::FileFilter> PixelGrid = {
    {"Pixel Grid (*.pxg)", "*.pxg"}
};

inline const std::vector<engine::platform::FileFilter> Prefab = {
    {"Prefab Files (*.prefab)", "*.prefab"}
};

inline const std::vector<engine::platform::FileFilter> Scene = {
    {"Scene Files (*.scene)", "*.scene"},
    {"All Files", "*.*"}
};

inline const std::vector<engine::platform::FileFilter> Image = {
    {"Image Files (*.png;*.jpg;*.jpeg)", "*.png;*.jpg;*.jpeg"}
};

inline const std::vector<engine::platform::FileFilter> JSON = {
    {"JSON Files (*.json)", "*.json"}
};

}

// Get the Assets directory for a project
inline std::string get_assets_directory(const std::string& project_path) {
    if (project_path.empty()) return "";
    return (std::filesystem::path(project_path) / "Assets").string();
}

// Animation Clip dialogs
inline std::string open_animation_clip(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Open Animation Clip", FileFilters::Animation);
}

inline std::string save_animation_clip(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Save Animation Clip", FileFilters::Animation, ".anim", initial_dir);
}

inline std::string save_animation_clip_as(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Save Animation Clip As", FileFilters::Animation, ".anim", initial_dir);
}

// Animator Controller dialogs
inline std::string open_animator_controller(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Open Animator Controller", FileFilters::AnimatorController);
}

inline std::string save_animator_controller(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Save Animator Controller", FileFilters::AnimatorController, ".animstate", initial_dir);
}

inline std::string save_animator_controller_as(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Save Animator Controller As", FileFilters::AnimatorController, ".animstate", initial_dir);
}

// Pixel Grid dialogs
inline std::string open_pixel_grid(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Open Pixel Grid", FileFilters::PixelGrid);
}

inline std::string save_pixel_grid(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Save Pixel Grid", FileFilters::PixelGrid, ".pxg", initial_dir);
}

inline std::string import_pixel_grid_layer(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Import Pixel Grid as Layer", FileFilters::PixelGrid);
}

// Image dialogs
inline std::string import_image_layer(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Import Image as Layer", FileFilters::Image);
}

// Prefab dialogs
inline std::string select_prefab(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Select Prefab", FileFilters::Prefab);
}

// Scene dialogs
inline std::string save_scene_as(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Save Scene As", FileFilters::Scene, ".scene", initial_dir);
}

// Profiler dialogs
inline std::string export_profiler_capture(const std::string& initial_dir = "") {
    return engine::platform::save_file_dialog("Export Profiler Capture", FileFilters::JSON, ".json", initial_dir);
}

inline std::string import_profiler_capture(const std::string& initial_dir = "") {
    return engine::platform::open_file_dialog("Import Profiler Capture", FileFilters::JSON);
}

}
