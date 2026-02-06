#pragma once

#include "Panel.h"
#include <string>
#include <glad/gl.h>

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
    GLuint m_preview_texture = 0;
    int m_texture_width = 0;
    int m_texture_height = 0;
};

} // namespace editor
