#pragma once

namespace engine::simulation { struct PixelGridComponent; }

namespace editor {

/// Custom inspector for PixelGridComponent with asset picker for .pxg files
class PixelGridComponentInspector {
public:
    /// Draw the inspector UI for PixelGridComponent
    /// Returns true if the component was modified
    static bool draw(engine::simulation::PixelGridComponent& component);

private:
    /// Show asset picker popup for .pxg files
    static void show_asset_picker(engine::simulation::PixelGridComponent& component);
};

} // namespace editor
