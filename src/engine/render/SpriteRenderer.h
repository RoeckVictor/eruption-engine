#pragma once

#include "engine/graphics/Shader.h"
#include <cstdint>
#include <vector>

namespace engine::render {

class SpriteRenderer {
public:
    SpriteRenderer() = default;
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;
    SpriteRenderer(SpriteRenderer&& other) noexcept;
    SpriteRenderer& operator=(SpriteRenderer&& other) noexcept;

    bool init(const char* vert_path = "shaders/sprite.vert",
              const char* frag_path = "shaders/sprite.frag");
    void shutdown();

    void begin(float cam_x, float cam_y, float zoom,
               int screen_w, int screen_h);
    void draw_rect(float min_x, float min_y, float max_x, float max_y,
                   float r, float g, float b, float a = 1.0f);
    void end();

private:
    graphics::Shader m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_ebo = 0;

    struct Vertex {
        float x, y;
        float r, g, b, a;
    };

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    // Camera state for current batch
    float m_cam_x = 0, m_cam_y = 0;
    float m_zoom = 1.0f;
    int m_screen_w = 0, m_screen_h = 0;
};

} // namespace engine::render
