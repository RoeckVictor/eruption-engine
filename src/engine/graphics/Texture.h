#pragma once
#include <cstdint>

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
                   const void* initial_data = nullptr);

    bool create_1d(int width, TextureFormat format,
                   TextureFilter filter = TextureFilter::Nearest,
                   TextureWrap wrap = TextureWrap::ClampToEdge,
                   const void* initial_data = nullptr);

    void destroy();

    void upload_sub_2d(int x, int y, int w, int h, const void* data);
    void readback_sub_2d(int x, int y, int w, int h, void* dst, int dst_size) const;
    void bind(int unit) const;
    void bind_as_image(int unit, ImageAccess access) const;

    uint32_t handle() const { return m_handle; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    TextureFormat format() const { return m_format; }
    bool valid() const { return m_handle != 0; }

private:
    uint32_t m_handle = 0;
    int m_width = 0;
    int m_height = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
    bool m_is_1d = false;
};

} // namespace engine::graphics
