#pragma once

#include <string>
#include <vector>
#include <functional>

namespace editor {

/// Configuration for asset picker popups
struct AssetPickerConfig {
    /// Unique popup ID (e.g., "SpriteAssetPicker", "FontAssetPicker")
    const char* popup_id;

    /// Title shown at top of popup (e.g., "Select Image Asset")
    const char* title;

    /// File extensions to filter (e.g., {".png", ".jpg", ".PNG", ".JPG"})
    std::vector<std::string> extensions;

    /// Additional search directories beyond project Assets/ and engine assets/
    std::vector<std::string> extra_search_dirs;

    /// Width of the popup child window (default 400)
    float popup_width = 400.0f;

    /// Height of the popup child window (default 300)
    float popup_height = 300.0f;

    /// Message shown when no matching files found
    const char* empty_message = "No matching files found in Assets folder";

    /// Label for the clear button (nullptr to hide)
    const char* clear_button_label = "Clear";

    /// Tooltip for the main button
    const char* button_tooltip = nullptr;
};

/// Result of showing the asset picker
struct AssetPickerResult {
    bool changed = false;          ///< True if selection changed
    std::string selected_path;     ///< Path of selected asset (empty if cleared)
};

/// Reusable asset picker UI component
/// Reduces code duplication across ImageInspector, TextInspector, etc.
class AssetPicker {
public:
    /// Draw a styled button that opens the asset picker popup
    /// @param current_path Current selected path (shown on button)
    /// @param project_path Project root path for scanning Assets/
    /// @param config Configuration for the picker
    /// @return Result with changed flag and new path if selection changed
    static AssetPickerResult draw_button(
        const std::string& current_path,
        const std::string& project_path,
        const AssetPickerConfig& config);

    /// Check if a file extension matches any in the list (case-insensitive)
    static bool matches_extension(const std::string& ext, const std::vector<std::string>& extensions);

    /// Scan directories for files matching extensions
    static std::vector<std::string> scan_for_assets(
        const std::string& project_path,
        const std::vector<std::string>& extensions,
        const std::vector<std::string>& extra_dirs = {});

private:
    /// Draw the popup content
    static AssetPickerResult draw_popup(
        const std::string& current_path,
        const std::vector<std::string>& files,
        const AssetPickerConfig& config);
};

/// Helper to check if drag-drop payload is an asset matching extensions
/// Call between BeginDragDropTarget() and EndDragDropTarget()
/// @return Path of dropped asset, or empty string if no valid drop
std::string accept_asset_drag_drop(const std::vector<std::string>& extensions);

} // namespace editor
