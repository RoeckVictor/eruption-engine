#include "engine/graphics/Texture.h"
#include "engine/rhi/RHI.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
#include "engine/platform/IImGuiBackend.h"
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
    , m_imgui_texture_id(other.m_imgui_texture_id)
{
    other.m_imgui_texture_id = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        invalidate_imgui_texture_id();
        m_texture = std::move(other.m_texture);
        m_format = other.m_format;
        m_imgui_texture_id = other.m_imgui_texture_id;
        other.m_imgui_texture_id = nullptr;
    }
    return *this;
}

bool Texture::create_2d(int width, int height, TextureFormat format,
                        TextureFilter filter, TextureWrap wrap,
                        const void* initial_data,
                        rhi::TextureUsageFlags usage)
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
    desc.usage = usage;

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
    invalidate_imgui_texture_id();
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
        auto* ctx = rhi::get_current_context();
        if (ctx) {
            ctx->bind_image(m_texture.get(), static_cast<uint32_t>(unit), to_rhi_access(access));
        }
        // Note: we do NOT invalidate the cached ImGui descriptor set here.
        // The descriptor was registered with layout SHADER_READ_ONLY_OPTIMAL,
        // which is the layout the image will be in when ImGui actually renders
        // (after the compute barrier transitions it back). The descriptor set
        // remains valid — only the runtime layout changes temporarily.
    }
}

void* Texture::imgui_texture_id() const {
    if (!m_texture) return nullptr;
    if (m_imgui_texture_id) return m_imgui_texture_id;

    auto* backend = platform::get_current_imgui_backend();
    if (backend) {
        m_imgui_texture_id = backend->register_texture(m_texture.get());
        return m_imgui_texture_id;
    }
    return m_texture->native_handle();
}

void Texture::invalidate_imgui_texture_id() {
    if (m_imgui_texture_id) {
        auto* backend = platform::get_current_imgui_backend();
        if (backend) backend->unregister_texture(m_imgui_texture_id);
        m_imgui_texture_id = nullptr;
    }
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
