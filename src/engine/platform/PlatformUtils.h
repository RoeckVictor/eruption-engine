#pragma once

#include <string>
#include <vector>

namespace engine::platform {

/// A single filter entry for file dialogs.
struct FileFilter {
    std::string description;  // "Pixel Grid Files (*.pxg)"
    std::string pattern;      // "*.pxg"
};

/// Show a native "Open File" dialog.
/// @return Selected file path, or empty string if cancelled.
std::string open_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters = {});

/// Show a native "Save File" dialog.
/// @return Selected file path, or empty string if cancelled.
std::string save_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters = {},
    const std::string& default_ext = "",
    const std::string& initial_dir = "");

/// Show a native "Select Folder" dialog.
/// @return Selected folder path, or empty string if cancelled.
std::string folder_dialog(const std::string& title = "Select Folder");

/// Launch a detached process (fire-and-forget).
/// @return true if the process was spawned successfully.
bool launch_detached(
    const std::string& exe_path,
    const std::string& args = "");

/// Open the OS file manager showing the given folder.
void open_folder_in_file_manager(const std::string& folder_path);

/// Open the OS file manager and highlight/select a specific file.
void reveal_in_file_manager(const std::string& file_path);

} // namespace engine::platform
