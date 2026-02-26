#pragma once

#include "engine/rhi/RHICommandBuffer.h"
#include <vector>
#include <variant>
#include <functional>

namespace engine::rhi {

class GLDescriptorSet;

///Recorded commands for deferred execution
namespace cmd {

struct BindPipeline { RHIPipeline* pipeline; };
struct BindFramebuffer { RHIFramebuffer* framebuffer; };
struct SetViewport { int x, y, width, height; };
struct SetScissor { int x, y, width, height; };
struct BindVertexBuffer { RHIBuffer* buffer; uint32_t binding; };
struct BindIndexBuffer { RHIBuffer* buffer; IndexType type; };
struct BindTexture { const RHITexture* texture; uint32_t unit; };
struct BindDescriptorSet { RHIDescriptorSet* set; uint32_t index; };
struct BindUniformBuffer { RHIBuffer* buffer; uint32_t binding; };
struct BindStorageBuffer { RHIBuffer* buffer; uint32_t binding; BufferAccess access; };
struct BindStorageImage { RHITexture* texture; uint32_t binding; BufferAccess access; };
struct ClearColor { float r, g, b, a; };
struct ClearDepth { float depth; };
struct ClearStencil { int value; };
struct Draw { uint32_t vertex_count, first_vertex, instance_count; };
struct DrawIndexed { uint32_t index_count, first_index, instance_count; };
struct DispatchCompute { uint32_t groups_x, groups_y, groups_z; };
struct MemoryBarrier { BarrierFlags flags; };

using Command = std::variant<
    BindPipeline,
    BindFramebuffer,
    SetViewport,
    SetScissor,
    BindVertexBuffer,
    BindIndexBuffer,
    BindTexture,
    BindDescriptorSet,
    BindUniformBuffer,
    BindStorageBuffer,
    BindStorageImage,
    ClearColor,
    ClearDepth,
    ClearStencil,
    Draw,
    DrawIndexed,
    DispatchCompute,
    MemoryBarrier
>;

}

/// OpenGL implementation of RHICommandBuffer.
/// Records commands into a vector and executes them immediately on submit().
class GLCommandBuffer : public RHICommandBuffer {
public:
    GLCommandBuffer() = default;
    ~GLCommandBuffer() override = default;

    bool init();

    void begin() override;
    void end() override;
    void reset() override;
    bool is_recording() const override { return m_recording; }

    void bind_pipeline(RHIPipeline* pipeline) override;
    void bind_framebuffer(RHIFramebuffer* framebuffer) override;
    void set_viewport(int x, int y, int width, int height) override;
    void set_scissor(int x, int y, int width, int height) override;

    void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0) override;
    void bind_index_buffer(RHIBuffer* buffer, IndexType type) override;
    void bind_texture(const RHITexture* texture, uint32_t unit) override;
    void bind_descriptor_set(RHIDescriptorSet* set, uint32_t index = 0) override;
    void bind_uniform_buffer(RHIBuffer* buffer, uint32_t binding) override;
    void bind_storage_buffer(RHIBuffer* buffer, uint32_t binding, BufferAccess access = BufferAccess::ReadWrite) override;
    void bind_storage_image(RHITexture* texture, uint32_t binding, BufferAccess access = BufferAccess::ReadWrite) override;

    void clear_color(float r, float g, float b, float a) override;
    void clear_depth(float depth = 1.0f) override;
    void clear_stencil(int value = 0) override;

    void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) override;
    void draw_indexed(uint32_t index_count, uint32_t first_index = 0, uint32_t instance_count = 1) override;

    void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) override;

    void memory_barrier(BarrierFlags flags) override;

    void execute();

    const std::vector<cmd::Command>& commands() const { return m_commands; }

private:
    std::vector<cmd::Command> m_commands;
    bool m_recording = false;

    void execute_command(const cmd::Command& command);
};

}
