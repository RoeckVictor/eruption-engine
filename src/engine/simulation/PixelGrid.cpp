#include "engine/simulation/PixelGrid.h"
#include "engine/graphics/RenderContext.h"
#include "engine/rhi/RHITypes.h"
#include "engine/core/Log.h"
#include <cstring>

namespace engine::simulation {

// Copy shader workgroup size (must match ssbo_to_texture.comp)
static constexpr int COPY_WORKGROUP_SIZE = 16;

bool PixelGrid::init(int width, int height, size_t pixel_size) {
    if (pixel_size < 8) {
        ENGINE_LOG_WARN("Pixel size %zu < 8 bytes (no per-pixel color support)", pixel_size);
        if (pixel_size < 4) {
            ENGINE_ERR("Pixel size must be at least 4 bytes");
            return false;
        }
    }

    m_width = width;
    m_height = height;
    m_pixel_size = pixel_size;

    size_t ssbo_size = static_cast<size_t>(width) * height * pixel_size;

    // Create ping-pong SSBOs for pixel data
    // Use DynamicDraw for frequent updates from both CPU and GPU
    if (!m_pixel_ssbos[0].create(ssbo_size, nullptr, graphics::BufferUsage::DynamicDraw)) {
        ENGINE_ERR("Failed to create pixel grid SSBO 0");
        return false;
    }
    if (!m_pixel_ssbos[1].create(ssbo_size, nullptr, graphics::BufferUsage::DynamicDraw)) {
        ENGINE_ERR("Failed to create pixel grid SSBO 1");
        return false;
    }

    // Create render texture (RGBA8UI for compatibility with existing render shader)
    if (!m_render_texture.create_2d(width, height, graphics::TextureFormat::RGBA8UI)) {
        ENGINE_ERR("Failed to create pixel grid render texture");
        return false;
    }

    // Load the SSBO-to-texture copy shader
    if (!m_copy_shader.load_compute("shaders/ssbo_to_texture.comp")) {
        return false;
    }

    // Set constant uniforms
    m_copy_shader.use();
    m_copy_shader.set_int("u_grid_width", width);
    m_copy_shader.set_int("u_grid_height", height);
    m_copy_shader.set_uint("u_pixel_size", static_cast<uint32_t>(pixel_size));

    // Initialize both SSBOs with empty pixels (material=0, category=0, temp=128, flags=0)
    m_work_buf.resize(ssbo_size);
    size_t total_pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < total_pixels; i++) {
        size_t base = i * pixel_size;
        m_work_buf[base + 0] = 0;   // material (air)
        m_work_buf[base + 1] = 0;   // category (CAT_EMPTY)
        m_work_buf[base + 2] = 128; // temperature
        m_work_buf[base + 3] = 0;   // flags
        // Zero any extra game-specific bytes
        for (size_t j = 4; j < pixel_size; j++) {
            m_work_buf[base + j] = 0;
        }
    }
    m_pixel_ssbos[0].update(0, ssbo_size, m_work_buf.data());
    m_pixel_ssbos[1].update(0, ssbo_size, m_work_buf.data());

    ENGINE_LOG("Pixel grid initialized: %dx%d, %zu bytes/pixel, SSBO mode",
               width, height, pixel_size);
    return true;
}

void PixelGrid::shutdown() {
    m_pixel_ssbos[0].destroy();
    m_pixel_ssbos[1].destroy();
    m_render_texture.destroy();
    m_copy_shader.destroy();
}

void PixelGrid::update_render_texture() {
    m_copy_shader.use();

    // Bind the read SSBO
    m_pixel_ssbos[m_read_idx].bind_base(0);

    // Bind render texture as image for writing
    m_render_texture.bind_as_image(0, graphics::ImageAccess::WriteOnly);

    // Dispatch compute shader
    int groups_x = (m_width + COPY_WORKGROUP_SIZE - 1) / COPY_WORKGROUP_SIZE;
    int groups_y = (m_height + COPY_WORKGROUP_SIZE - 1) / COPY_WORKGROUP_SIZE;

    graphics::RenderContext ctx;
    // Dispatch with image barrier (shader writes to texture, rendering will read it)
    ctx.dispatch_compute(groups_x, groups_y, 1, rhi::BarrierFlags::ImageAccess);
}

void PixelGrid::upload_both(int x, int y, int w, int h, const void* data) {
    // Bounds validation with overflow-safe checks
    if (x < 0 || y < 0 || w <= 0 || h <= 0) return;
    if (x > m_width - w || y > m_height - h) return;

    const uint8_t* src = static_cast<const uint8_t*>(data);

    // Upload row by row (SSBOs are linear, textures were 2D)
    for (int row = 0; row < h; row++) {
        size_t ssbo_offset = (static_cast<size_t>(y + row) * m_width + x) * m_pixel_size;
        size_t src_offset = static_cast<size_t>(row) * w * m_pixel_size;
        size_t row_bytes = static_cast<size_t>(w) * m_pixel_size;

        m_pixel_ssbos[0].update(ssbo_offset, row_bytes, src + src_offset);
        m_pixel_ssbos[1].update(ssbo_offset, row_bytes, src + src_offset);
    }
}

void PixelGrid::readback_region(int x, int y, int w, int h,
                                 void* dst, int dst_size) const {
    // === VALIDATION PHASE ===

    // 1. Validate region parameters
    if (x < 0 || y < 0 || w <= 0 || h <= 0) {
        ENGINE_ERR("Invalid readback region: origin=(%d,%d), size=%dx%d", x, y, w, h);
        return;
    }

    if (x + w > m_width || y + h > m_height) {
        ENGINE_ERR("Readback region out of grid bounds: (%d,%d) %dx%d exceeds grid %dx%d",
                   x, y, w, h, m_width, m_height);
        return;
    }

    // 2. Check for potential overflow in pixel count calculation
    if (w > INT_MAX / h) {
        ENGINE_ERR("Readback region too large: %dx%d would overflow", w, h);
        return;
    }
    size_t pixel_count = static_cast<size_t>(w) * h;

    // 3. Validate destination buffer size
    size_t required_size = pixel_count * m_pixel_size;
    if (required_size > static_cast<size_t>(dst_size)) {
        ENGINE_ERR("Destination buffer too small: required=%zu, provided=%d",
                   required_size, dst_size);
        return;
    }

    // 4. Validate SSBO calculations won't overflow
    size_t total_grid_pixels = static_cast<size_t>(m_width) * m_height;
    size_t first_pixel_idx = static_cast<size_t>(y) * m_width + x;

    // Safe calculation using pixel_count instead of potentially overflowing arithmetic
    size_t last_row_first_pixel = static_cast<size_t>(y + h - 1) * m_width;
    if (last_row_first_pixel >= total_grid_pixels) {
        ENGINE_ERR("SSBO readback calculation overflow: last_row_first_pixel=%zu, total=%zu",
                   last_row_first_pixel, total_grid_pixels);
        return;
    }
    size_t last_pixel_idx = last_row_first_pixel + (x + w - 1);

    // === READBACK PHASE ===

    uint8_t* dst_ptr = static_cast<uint8_t*>(dst);

    // Calculate contiguous byte range in SSBO that spans the entire bounding rectangle
    // IMPORTANT: Read the entire rect (including gaps) in ONE glMapBufferRange call
    // to avoid multiple GPU stalls. The old row-by-row approach was causing massive lag.
    size_t ssbo_offset = first_pixel_idx * m_pixel_size;
    size_t ssbo_length = (last_pixel_idx - first_pixel_idx + 1) * m_pixel_size;

    // Validate SSBO bounds
    size_t total_ssbo_size = total_grid_pixels * m_pixel_size;
    if (ssbo_offset + ssbo_length > total_ssbo_size) {
        ENGINE_ERR("SSBO readback out of bounds: offset=%zu, length=%zu, total=%zu",
                   ssbo_offset, ssbo_length, total_ssbo_size);
        return;
    }

    // Read entire bounding rectangle (includes gaps between rows) - ONE GPU stall
    m_work_buf.resize(ssbo_length);
    m_pixel_ssbos[m_read_idx].readback(ssbo_offset, ssbo_length, m_work_buf.data());

    // Extract actual rows (skip gaps)
    for (int row = 0; row < h; row++) {
        // Source: work_buf contains data starting from (x, y), with m_width stride
        size_t src_offset = static_cast<size_t>(row) * m_width * m_pixel_size;
        size_t dst_offset = static_cast<size_t>(row) * w * m_pixel_size;
        size_t row_bytes = static_cast<size_t>(w) * m_pixel_size;

        // Final safety check before memcpy
        if (src_offset + row_bytes > m_work_buf.size() || dst_offset + row_bytes > required_size) {
            ENGINE_ERR("Memcpy bounds violation prevented: row=%d, src_offset=%zu, dst_offset=%zu",
                       row, src_offset, dst_offset);
            continue;
        }

        memcpy(dst_ptr + dst_offset, m_work_buf.data() + src_offset, row_bytes);
    }
}

void PixelGrid::spawn_material(int x, int y, int radius,
                                uint8_t material, uint8_t category, uint8_t temp,
                                uint32_t color) {
    int min_x = x - radius;
    int max_x = x + radius;
    int min_y = y - radius;
    int max_y = y + radius;

    if (min_x < 0) min_x = 0;
    if (max_x >= m_width) max_x = m_width - 1;
    if (min_y < 0) min_y = 0;
    if (max_y >= m_height) max_y = m_height - 1;

    int patch_w = max_x - min_x + 1;
    int patch_h = max_y - min_y + 1;
    if (patch_w <= 0 || patch_h <= 0) return;

    size_t buf_bytes = static_cast<size_t>(patch_w) * patch_h * m_pixel_size;
    m_work_buf.resize(buf_bytes);

    readback_region(min_x, min_y, patch_w, patch_h,
                    m_work_buf.data(), static_cast<int>(buf_bytes));

    int r2 = radius * radius;
    bool changed = false;

    // Unpack color components (0xRRGGBBAA format)
    uint8_t color_r = (color >> 24) & 0xFF;
    uint8_t color_g = (color >> 16) & 0xFF;
    uint8_t color_b = (color >> 8) & 0xFF;
    uint8_t color_a = color & 0xFF;

    for (int row = 0; row < patch_h; row++) {
        for (int px = 0; px < patch_w; px++) {
            int dx = (min_x + px) - x;
            int dy = (min_y + row) - y;
            if (dx * dx + dy * dy <= r2) {
                size_t idx = (static_cast<size_t>(row) * patch_w + px) * m_pixel_size;
                m_work_buf[idx + 0] = material;   // byte 0 = material ID
                m_work_buf[idx + 1] = category;   // byte 1 = category (engine physics)
                m_work_buf[idx + 2] = temp;       // byte 2 = temperature
                m_work_buf[idx + 3] = 0;          // byte 3 = flags

                // Set color bytes for 8-byte pixel mode
                if (m_pixel_size >= 8) {
                    m_work_buf[idx + 4] = color_r;
                    m_work_buf[idx + 5] = color_g;
                    m_work_buf[idx + 6] = color_b;
                    m_work_buf[idx + 7] = color_a;
                }
                changed = true;
            }
        }
    }

    if (changed) {
        upload_both(min_x, min_y, patch_w, patch_h, m_work_buf.data());
    }
}

} // namespace engine::simulation
