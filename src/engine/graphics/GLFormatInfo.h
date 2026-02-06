#pragma once

// Internal shared header for OpenGL format mapping.
// Used by Texture.cpp and PixelReadback.cpp.

#include <glad/gl.h>

namespace engine::graphics {

enum class TextureFormat;

namespace detail {

struct GLFormatInfo {
    GLenum internal_format;
    GLenum pixel_format;
    GLenum pixel_type;
};

inline GLFormatInfo gl_format(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:   return { GL_RGBA8,   GL_RGBA,         GL_UNSIGNED_BYTE };
        case TextureFormat::RGBA8UI: return { GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE };
    }
    return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
}

} // namespace detail
} // namespace engine::graphics
