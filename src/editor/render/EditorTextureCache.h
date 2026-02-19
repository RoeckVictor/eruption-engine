#pragma once

#include <glad/gl.h>
#include <string>
#include <unordered_map>

namespace editor {

/// Cached texture entry with dimensions.
struct CachedTexture {
    GLuint handle = 0;
    int width = 0;
    int height = 0;
};

/// Texture cache for editor panels rendering Image components.
/// Manages loading, caching, and cleanup of GL textures from image files.
/// Each panel should own its own instance (not a singleton).
class EditorTextureCache {
public:
    EditorTextureCache() = default;
    ~EditorTextureCache();

    // Non-copyable
    EditorTextureCache(const EditorTextureCache&) = delete;
    EditorTextureCache& operator=(const EditorTextureCache&) = delete;

    // Movable
    EditorTextureCache(EditorTextureCache&& other) noexcept;
    EditorTextureCache& operator=(EditorTextureCache&& other) noexcept;

    /// Get or load a texture by path.
    /// Returns the fallback white texture for empty paths or load failures.
    /// @param path Asset path relative to assets/ or absolute path
    /// @param out_width Receives texture width
    /// @param out_height Receives texture height
    /// @return OpenGL texture handle
    GLuint get(const std::string& path, int& out_width, int& out_height);

    /// Get the 1x1 white fallback texture, creating it if needed.
    GLuint white_texture();

    /// Clear all cached textures, freeing GL resources.
    void clear();

    /// Invalidate a specific texture (e.g., when asset is modified).
    void invalidate(const std::string& path);

private:
    void ensure_white_texture();

    std::unordered_map<std::string, CachedTexture> m_cache;
    GLuint m_white_texture = 0;
};

} // namespace editor
