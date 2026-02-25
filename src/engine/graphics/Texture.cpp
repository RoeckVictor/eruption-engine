#include "engine/graphics/Texture.h"
#include "engine/rhi/RHI.h"
#include "engine/core/Log.h"

namespace engine::graphics {

namespace {

// Convert graphics TextureFormat to RHI TextureFormat
rhi::TextureFormat to_rhi_format(TextureFormat fmt) {
    switch (fmt) {
        case TextureFormat::RGBA8:   return rhi::TextureFormat::RGBA8;
        case TextureFormat::RGBA8UI: return rhi::TextureFormat::RGBA8UI;
        default:                     return rhi::TextureFormat::RGBA8;
    }
}

// Convert graphics TextureFilter to RHI TextureFilter
rhi::TextureFilter to_rhi_filter(TextureFilter f) {
    switch (f) {
        case TextureFilter::Nearest: return rhi::TextureFilter::Nearest;
        case TextureFilter::Linear:  return rhi::TextureFilter::Linear;
        default:                     return rhi::TextureFilter::Nearest;
    }
}

// Convert graphics TextureWrap to RHI TextureWrap
rhi::TextureWrap to_rhi_wrap(TextureWrap w) {
    switch (w) {
        case TextureWrap::ClampToEdge: return rhi::TextureWrap::Clamp;
        case TextureWrap::Repeat:      return rhi::TextureWrap::Repeat;
        default:                       return rhi::TextureWrap::Clamp;
    }
}

// Convert graphics ImageAccess to RHI ImageAccess
rhi::ImageAccess to_rhi_access(ImageAccess a) {
    switch (a) {
        case ImageAccess::ReadOnly:  return rhi::ImageAccess::ReadOnly;
        case ImageAccess::WriteOnly: return rhi::ImageAccess::WriteOnly;
        case ImageAccess::ReadWrite: return rhi::ImageAccess::ReadWrite;
        default:                     return rhi::ImageAccess::ReadOnly;
    }
}

} // anonymous namespace

Texture::~Texture() {
    destroy();
}

Texture::Texture(Texture&& other) noexcept
    : m_texture(std::move(other.m_texture))
    , m_format(other.m_format)
{
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        m_texture = std::move(other.m_texture);
        m_format = other.m_format;
    }
    return *this;
}

bool Texture::create_2d(int width, int height, TextureFormat format,
                        TextureFilter filter, TextureWrap wrap,
                        const void* initial_data)
{
    if (width <= 0 || height <= 0) {
        ENGINE_ERR("Invalid texture dimensions: %dx%d", width, height);
        return false;
    }

    destroy();

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("No RHI device available for texture creation");
        return false;
    }

    rhi::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.dimension = rhi::TextureDimension::Tex2D;
    desc.format = to_rhi_format(format);
    desc.min_filter = to_rhi_filter(filter);
    desc.mag_filter = to_rhi_filter(filter);
    desc.wrap_u = to_rhi_wrap(wrap);
    desc.wrap_v = to_rhi_wrap(wrap);
    desc.initial_data = initial_data;

    m_texture = device->create_texture(desc);
    if (!m_texture || !m_texture->valid()) {
        ENGINE_ERR("Failed to create 2D texture (%dx%d)", width, height);
        return false;
    }

    m_format = format;
    return true;
}

bool Texture::create_1d(int width, TextureFormat format,
                        TextureFilter filter, TextureWrap wrap,
                        const void* initial_data)
{
    if (width <= 0) {
        ENGINE_ERR("Invalid 1D texture width: %d", width);
        return false;
    }

    destroy();

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("No RHI device available for texture creation");
        return false;
    }

    rhi::TextureDesc desc;
    desc.width = width;
    desc.height = 1;
    desc.depth = 1;
    desc.dimension = rhi::TextureDimension::Tex1D;
    desc.format = to_rhi_format(format);
    desc.min_filter = to_rhi_filter(filter);
    desc.mag_filter = to_rhi_filter(filter);
    desc.wrap_u = to_rhi_wrap(wrap);
    desc.initial_data = initial_data;

    m_texture = device->create_texture(desc);
    if (!m_texture || !m_texture->valid()) {
        ENGINE_ERR("Failed to create 1D texture (width=%d)", width);
        return false;
    }

    m_format = format;
    return true;
}

void Texture::destroy() {
    m_texture.reset();
}

void Texture::upload_sub_2d(int x, int y, int w, int h, const void* data) {
    if (m_texture) {
        m_texture->upload(x, y, w, h, data);
    }
}

void Texture::readback_sub_2d(int x, int y, int w, int h, void* dst, int dst_size) const {
    if (m_texture) {
        m_texture->readback(x, y, w, h, dst, static_cast<size_t>(dst_size));
    }
}

void Texture::bind(int unit) const {
    if (m_texture) {
        m_texture->bind(static_cast<uint32_t>(unit));
    }
}

void Texture::bind_as_image(int unit, ImageAccess access) const {
    if (m_texture) {
        m_texture->bind_as_image(static_cast<uint32_t>(unit), to_rhi_access(access));
    }
}

void* Texture::imgui_texture_id() const {
    if (!m_texture) return nullptr;
    return m_texture->native_handle();
}

uint32_t Texture::handle() const {
    if (!m_texture) return 0;
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(m_texture->native_handle()));
}

int Texture::width() const {
    return m_texture ? m_texture->width() : 0;
}

int Texture::height() const {
    return m_texture ? m_texture->height() : 0;
}

bool Texture::valid() const {
    return m_texture != nullptr && m_texture->valid();
}

} // namespace engine::graphics
