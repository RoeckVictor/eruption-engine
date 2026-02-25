#include "engine/render/FullscreenPass.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/rhi/RHIContext.h"

namespace engine::render {

FullscreenPass::~FullscreenPass() {
    shutdown();
}

FullscreenPass::FullscreenPass(FullscreenPass&& other) noexcept
    : m_pipeline(std::move(other.m_pipeline))
{
}

FullscreenPass& FullscreenPass::operator=(FullscreenPass&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_pipeline = std::move(other.m_pipeline);
    }
    return *this;
}

bool FullscreenPass::init() {
    auto* device = rhi::get_current_device();
    if (!device) return false;

    // Create a minimal pipeline with just an empty VAO for fullscreen triangles
    // The shader is managed separately by the caller
    rhi::PipelineDesc desc{};
    desc.shader = nullptr;  // No shader - caller binds their own
    desc.topology = rhi::PrimitiveTopology::Triangles;
    desc.attribute_count = 0;
    desc.binding_count = 0;
    desc.rasterizer.cull_mode = rhi::CullMode::None;

    m_pipeline = device->create_pipeline(desc);
    return m_pipeline != nullptr && m_pipeline->valid();
}

void FullscreenPass::shutdown() {
    m_pipeline.reset();
}

void FullscreenPass::draw() const {
    if (!m_pipeline) return;

    auto* ctx = rhi::get_current_context();
    if (!ctx) return;

    // Bind pipeline (VAO and state, shader already bound by caller)
    ctx->bind_pipeline(m_pipeline.get());
    ctx->draw(3, 0, 1);  // Draw fullscreen triangle
}

} // namespace engine::render
