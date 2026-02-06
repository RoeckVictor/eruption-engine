#pragma once

#include "engine/graphics/Shader.h"
#include <cstdint>
#include <vector>

namespace engine::graphics { class Texture; }

namespace engine::render {

/// Batched sprite renderer with texture + UV support.
/// Vertex format: position (vec2) + UV (vec2) + color tint (vec4).
class TexturedSpriteRenderer {
public:
    TexturedSpriteRenderer() = default;
    ~TexturedSpriteRenderer();

    TexturedSpriteRenderer(const TexturedSpriteRenderer&) = delete;
    TexturedSpriteRenderer& operator=(const TexturedSpriteRenderer&) = delete;
    TexturedSpriteRenderer(TexturedSpriteRenderer&& other) noexcept;
    TexturedSpriteRenderer& operator=(TexturedSpriteRenderer&& other) noexcept;

    bool init(const char* vert_path = "shaders/textured_sprite.vert",
              const char* frag_path = "shaders/textured_sprite.frag");
    void shutdown();

    /// Begin a batch for a specific texture.
    void begin(const graphics::Texture& texture,
               float cam_x, float cam_y, float zoom,
               int screen_w, int screen_h);

    /// Draw a textured quad with UV coordinates and optional color tint.
    void draw_sprite(float min_x, float min_y, float max_x, float max_y,
                     float u0, float v0, float u1, float v1,
                     float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    /// Flush the batch and issue the draw call.
    void end();

private:
    graphics::Shader m_shader;
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_ebo = 0;

    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

    float m_cam_x = 0, m_cam_y = 0;
    float m_zoom = 1.0f;
    int m_screen_w = 0, m_screen_h = 0;
    const graphics::Texture* m_bound_texture = nullptr;
};

} // namespace engine::render
