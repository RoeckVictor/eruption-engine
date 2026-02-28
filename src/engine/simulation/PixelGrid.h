#pragma once

#include "engine/graphics/ShaderStorageBuffer.h"
#include "engine/graphics/Texture.h"
#include "engine/graphics/Shader.h"
#include <cstdint>
#include <vector>

namespace engine::simulation {

/// GPU-backed pixel grid for cellular automata simulation using SSBOs
class PixelGrid {
public:
    bool init(int width, int height, size_t pixel_size = 8);
    void shutdown();

    int width() const { return m_width; }
    int height() const { return m_height; }
    size_t pixel_size() const { return m_pixel_size; }

    void spawn_material(int x, int y, int radius,
                        uint8_t material, uint8_t category, uint8_t temp,
                        uint32_t color = 0xFFFFFFFF);

    void readback_region(int x, int y, int w, int h,
                         void* dst, int dst_size) const;

    void upload_both(int x, int y, int w, int h, const void* data);


    graphics::ShaderStorageBuffer& ssbo(int idx) { return m_pixel_ssbos[idx]; }
    const graphics::ShaderStorageBuffer& current_ssbo() const { return m_pixel_ssbos[m_read_idx]; }

    void bind_read_ssbo(int binding) const { m_pixel_ssbos[m_read_idx].bind_base(binding); }
    void bind_write_ssbo(int binding) const { m_pixel_ssbos[1 - m_read_idx].bind_base(binding); }

    const graphics::Texture& render_texture() const { return m_render_texture; }

    void update_render_texture();

    graphics::Texture& texture(int /*idx*/) { return m_render_texture; }
    const graphics::Texture& current_texture() const { return m_render_texture; }

    int read_idx() const { return m_read_idx; }
    void swap() { m_read_idx = 1 - m_read_idx; }

    uint32_t frame_counter() const { return m_frame_counter; }
    void increment_frame() { m_frame_counter++; }

private:
    int m_width = 0;
    int m_height = 0;
    size_t m_pixel_size = 8;

    graphics::ShaderStorageBuffer m_pixel_ssbos[2];
    graphics::Texture m_render_texture;
    graphics::Shader m_copy_shader;

    int m_read_idx = 0;
    uint32_t m_frame_counter = 0;

    mutable std::vector<uint8_t> m_work_buf;
};

}
