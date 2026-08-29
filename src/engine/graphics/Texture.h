#pragma once
#include <cstdint>
#include <memory>
#include "engine/rhi/RHITexture.h"

namespace engine::graphics {

enum class TextureFormat {
    RGBA8,
    RGBA8UI,
};

enum class TextureFilter {
    Nearest,
    Linear,
};

enum class TextureWrap {
    ClampToEdge,
    Repeat,
};

enum class ImageAccess {
    ReadOnly,
    WriteOnly,
    ReadWrite,
};

/// Texture wrapper that delegates to RHI
/// @note For new code, consider using engine::rhi::RHITexture directly
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    bool create_2d(int width, int height, TextureFormat format,
                   TextureFilter filter = TextureFilter::Nearest,
                   TextureWrap wrap = TextureWrap::ClampToEdge,
                   const void* initial_data = nullptr,
                   rhi::TextureUsageFlags usage = rhi::TextureUsageFlags::Default);

    bool create_1d(int width, TextureFormat format,
                   TextureFilter filter = TextureFilter::Nearest,
                   TextureWrap wrap = TextureWrap::ClampToEdge,
                   const void* initial_data = nullptr);

    void destroy();

    void upload_sub_2d(int x, int y, int w, int h, const void* data);
    void readback_sub_2d(int x, int y, int w, int h, void* dst, int dst_size) const;
    void bind(int unit) const;
    void bind_as_image(int unit, ImageAccess access) const;

    /// Get native handle as void* for ImGui texture rendering.
    /// This is backend-independent and works with any RHI backend.
    void* imgui_texture_id() const;

    /// Get native handle (for legacy code that needs direct GL access)
    /// @deprecated Use imgui_texture_id() for ImGui or rhi_texture()->native_handle() for direct access
    uint32_t handle() const;

    int width() const;
    int height() const;
    TextureFormat format() const { return m_format; }
    bool valid() const;

    /// Get the underlying RHI texture
    rhi::RHITexture* rhi_texture() { return m_texture.get(); }
    const rhi::RHITexture* rhi_texture() const { return m_texture.get(); }

private:
    void invalidate_imgui_texture_id();

    std::unique_ptr<rhi::RHITexture> m_texture;
    TextureFormat m_format = TextureFormat::RGBA8;
    mutable void* m_imgui_texture_id = nullptr;
};

} // namespace engine::graphics
