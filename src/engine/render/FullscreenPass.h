#pragma once
#include "engine/rhi/RHIPipeline.h"
#include <cstdint>
#include <memory>

namespace engine::render {

/// Utility class for drawing a fullscreen triangle.
/// The caller is responsible for binding the shader and setting uniforms.
class FullscreenPass {
public:
    FullscreenPass() = default;
    ~FullscreenPass();

    FullscreenPass(const FullscreenPass&) = delete;
    FullscreenPass& operator=(const FullscreenPass&) = delete;
    FullscreenPass(FullscreenPass&& other) noexcept;
    FullscreenPass& operator=(FullscreenPass&& other) noexcept;

    bool init();
    void shutdown();
    void draw() const;

private:
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
};

} // namespace engine::render
