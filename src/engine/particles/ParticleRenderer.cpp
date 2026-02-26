#include "engine/particles/ParticleRenderer.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/graphics/Texture.h"
#include "engine/render/Camera2D.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"
#include "engine/profiler/Profiler.h"
#include "engine/core/Log.h"

namespace engine::particles {

bool ParticleRenderer::init() {
    if (!m_shader.load_graphics("shaders/particle.vert", "shaders/particle.frag")) {
        return false;
    }

    // Create pipeline with points topology, blending, and program point size
    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("ParticleRenderer::init - No RHI device available");
        return false;
    }

    rhi::PipelineDesc desc{};
    desc.shader = m_shader.rhi_shader();
    desc.topology = rhi::PrimitiveTopology::Points;

    // Enable alpha blending
    desc.blend.enabled = true;
    desc.blend.src_color = rhi::BlendFactor::SrcAlpha;
    desc.blend.dst_color = rhi::BlendFactor::OneMinusSrcAlpha;
    desc.blend.src_alpha = rhi::BlendFactor::One;
    desc.blend.dst_alpha = rhi::BlendFactor::OneMinusSrcAlpha;

    // Enable program point size and disable culling (points don't need it)
    desc.rasterizer.program_point_size = true;
    desc.rasterizer.cull_mode = rhi::CullMode::None;

    // No vertex attributes - particle data comes from SSBO
    desc.attribute_count = 0;
    desc.binding_count = 0;

    m_pipeline = device->create_pipeline(desc);
    if (!m_pipeline || !m_pipeline->valid()) {
        ENGINE_ERR("ParticleRenderer::init - Failed to create pipeline");
        m_shader.destroy();
        return false;
    }

    // Set constant uniforms
    m_shader.use();
    m_shader.set_int("u_palette", 0); // texture unit 0

    ENGINE_LOG("ParticleRenderer initialized");
    return true;
}

void ParticleRenderer::shutdown() {
    m_pipeline.reset();
    m_shader.destroy();
}

void ParticleRenderer::draw(ParticleBuffer& buffer,
                             const graphics::Texture& palette,
                             const render::Camera2D& camera,
                             float screen_w, float screen_h,
                             float grid_origin_x, float grid_origin_y, int grid_height) {
    int alive = buffer.alive_count();
    if (alive == 0) return;
    PROFILE_SCOPE("ParticleRenderer::draw");

    auto* ctx = rhi::get_current_context();
    if (!ctx) return;

    // Bind pipeline (sets shader, blend state, point size, VAO)
    ctx->bind_pipeline(m_pipeline.get());

    // Bind texture and SSBO
    ctx->bind_texture(palette.rhi_texture(), 0);
    buffer.bind_particles();

    // Set per-draw uniforms
    m_shader.set_vec2("u_camera_pos", camera.x, camera.y);
    m_shader.set_vec2("u_screen_size", screen_w, screen_h);
    m_shader.set_float("u_zoom", camera.zoom);
    m_shader.set_vec2("u_grid_origin", grid_origin_x, grid_origin_y);
    m_shader.set_int("u_grid_height", grid_height);

    // Draw particles
    ctx->draw(static_cast<uint32_t>(alive), 0, 1);
}

} // namespace engine::particles
