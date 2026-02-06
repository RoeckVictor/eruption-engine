#pragma once

#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/graphics/Texture.h"
#include "engine/graphics/Shader.h"
#include <cstdint>
#include <vector>

namespace engine::simulation {

/// GPU-backed pixel grid for cellular automata simulation using SSBOs.
///
/// Stores pixel data in two Shader Storage Buffer Objects in a ping-pong
/// arrangement for compute shader read/write. A separate render texture
/// holds material IDs for efficient palette-based rendering.
///
/// The pixel struct layout is defined by the game but must start with
/// engine-mandated fields (see PixelData.h):
///   byte 0 = material ID   - game-defined index for color lookup
///   byte 1 = category      - engine physics category (see MaterialDefs.h)
///   byte 2 = temperature   - thermal value (0-255)
///   byte 3 = flags         - engine flags (reserved)
///   bytes 4+ = game-specific fields
///
/// The category in byte 1 is critical: engine physics code reads it directly
/// to determine collision behavior (static/powder/liquid/gas). Material ID
/// in byte 0 is for game logic and rendering - the engine doesn't interpret it.
class PixelGrid {
public:
    /// Initialize the pixel grid.
    /// @param width Grid width in pixels
    /// @param height Grid height in pixels
    /// @param pixel_size Size of each pixel struct in bytes (minimum 4)
    bool init(int width, int height, size_t pixel_size = 4);
    void shutdown();

    int width() const { return m_width; }
    int height() const { return m_height; }
    size_t pixel_size() const { return m_pixel_size; }

    // --- Pixel manipulation ---

    /// Stamp a circle of material at a position.
    /// @param category Engine physics category from MaterialDefs.h (CAT_EMPTY, CAT_STATIC, etc.)
    void spawn_material(int x, int y, int radius,
                        uint8_t material, uint8_t category, uint8_t temp);

    /// Read a region from the current read SSBO.
    /// Data is returned in row-major order with pixel_size bytes per pixel.
    void readback_region(int x, int y, int w, int h,
                         void* dst, int dst_size) const;

    /// Upload data to both ping-pong SSBOs.
    /// Data must be in row-major order with pixel_size bytes per pixel.
    void upload_both(int x, int y, int w, int h, const void* data);

    // --- SSBO access (for compute shaders) ---

    graphics::ShaderStorageBuffer& ssbo(int idx) { return m_pixel_ssbos[idx]; }
    const graphics::ShaderStorageBuffer& current_ssbo() const { return m_pixel_ssbos[m_read_idx]; }

    /// Bind the read SSBO to a binding point.
    void bind_read_ssbo(int binding) const { m_pixel_ssbos[m_read_idx].bind_base(binding); }
    /// Bind the write SSBO to a binding point.
    void bind_write_ssbo(int binding) const { m_pixel_ssbos[1 - m_read_idx].bind_base(binding); }

    // --- Render texture (for display) ---

    /// Get the render texture (R8UI with material IDs).
    const graphics::Texture& render_texture() const { return m_render_texture; }

    /// Update the render texture from the current read SSBO.
    /// Call this after simulation before rendering.
    void update_render_texture();

    // --- Legacy texture access (for migration) ---

    /// Legacy: Get texture by index. Returns render texture (read-only).
    /// @deprecated Use render_texture() or ssbo() instead.
    graphics::Texture& texture(int /*idx*/) { return m_render_texture; }
    const graphics::Texture& current_texture() const { return m_render_texture; }

    // --- Ping-pong state ---

    int read_idx() const { return m_read_idx; }
    void swap() { m_read_idx = 1 - m_read_idx; }

    // --- Frame counter ---

    uint32_t frame_counter() const { return m_frame_counter; }
    void increment_frame() { m_frame_counter++; }

private:
    int m_width = 0;
    int m_height = 0;
    size_t m_pixel_size = 4;

    graphics::ShaderStorageBuffer m_pixel_ssbos[2];
    graphics::Texture m_render_texture;
    graphics::Shader m_copy_shader;

    int m_read_idx = 0;
    uint32_t m_frame_counter = 0;

    // Reusable work buffer for CPU operations
    mutable std::vector<uint8_t> m_work_buf;
};

} // namespace engine::simulation
