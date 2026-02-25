#pragma once

#include "engine/graphics/Shader.h"
#include "engine/rhi/RHIPipeline.h"
#include "engine/rhi/RHIBuffer.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::render {

/// Immediate-mode line renderer for debug visualization.
/// Uses the same camera transform as SpriteRenderer (sprite.vert/frag shaders).
class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer();

    DebugRenderer(const DebugRenderer&) = delete;
    DebugRenderer& operator=(const DebugRenderer&) = delete;

    bool init();
    void shutdown();

    void begin(float cam_x, float cam_y, float zoom,
               int screen_w, int screen_h);

    /// Draw a line segment between two world-space points.
    void draw_line(float x0, float y0, float x1, float y1,
                   float r, float g, float b, float a = 1.0f);

    void end();

private:
    graphics::Shader m_shader;
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHIBuffer> m_vbo;
    size_t m_vbo_capacity = 0;  // Current VBO capacity in bytes

    struct Vertex {
        float x, y;
        float r, g, b, a;
    };

    std::vector<Vertex> m_vertices;

    float m_cam_x = 0, m_cam_y = 0;
    float m_zoom = 1.0f;
    int m_screen_w = 0, m_screen_h = 0;
};

} // namespace engine::render
