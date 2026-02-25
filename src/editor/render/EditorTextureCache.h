#pragma once

#include "engine/graphics/Texture.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace editor {

/// Cached texture entry with dimensions.
struct CachedTexture {
    std::unique_ptr<engine::graphics::Texture> texture;
    int width = 0;
    int height = 0;
};

/// Texture cache for editor panels rendering Image components.
/// Manages loading, caching, and cleanup of textures from image files.
/// Each panel should own its own instance (not a singleton).
class EditorTextureCache {
public:
    EditorTextureCache() = default;
    ~EditorTextureCache() = default;

    // Non-copyable
    EditorTextureCache(const EditorTextureCache&) = delete;
    EditorTextureCache& operator=(const EditorTextureCache&) = delete;

    // Movable
    EditorTextureCache(EditorTextureCache&& other) noexcept = default;
    EditorTextureCache& operator=(EditorTextureCache&& other) noexcept = default;

    /// Get or load a texture by path.
    /// Returns the fallback white texture handle for empty paths or load failures.
    /// @param path Asset path relative to assets/ or absolute path
    /// @param out_width Receives texture width
    /// @param out_height Receives texture height
    /// @return Texture handle for use with ImGui (cast to ImTextureID with uintptr_t)
    void* get(const std::string& path, int& out_width, int& out_height);

    /// Get the 1x1 white fallback texture handle, creating it if needed.
    void* white_texture();

    /// Clear all cached textures.
    void clear();

    /// Invalidate a specific texture (e.g., when asset is modified).
    void invalidate(const std::string& path);

private:
    void ensure_white_texture();

    std::unordered_map<std::string, CachedTexture> m_cache;
    std::unique_ptr<engine::graphics::Texture> m_white_texture;
};

} // namespace editor
