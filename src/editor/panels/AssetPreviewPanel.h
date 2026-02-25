#pragma once

#include "Panel.h"
#include "engine/graphics/Texture.h"
#include <string>

namespace editor {

/// Panel for previewing selected assets (textures, etc.)
class AssetPreviewPanel : public Panel {
public:
    AssetPreviewPanel();
    ~AssetPreviewPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;

    /// Set the asset to preview.
    void set_asset(const std::string& path);

    /// Clear the current preview.
    void clear_preview();

private:
    void load_texture(const std::string& path);
    void unload_texture();

    std::string m_current_path;
    std::string m_asset_type;  // "texture", "scene", "prefab", etc.

    // For texture preview
    engine::graphics::Texture m_preview_texture;
};

} // namespace editor
