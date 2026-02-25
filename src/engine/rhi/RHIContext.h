#pragma once

#include "RHITypes.h"

namespace engine::rhi {

// Forward declarations
class RHIBuffer;
class RHITexture;
class RHIShader;
class RHIPipeline;
class RHIFramebuffer;

/// Abstract rendering context for command submission
/// Manages state and issues draw/dispatch commands
class RHIContext {
public:
    virtual ~RHIContext() = default;

    // Non-copyable
    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;

    // =========================================================================
    // Frame Management
    // =========================================================================

    /// Begin a new frame (call before any rendering)
    virtual void begin_frame() = 0;

    /// End the current frame (call after all rendering)
    virtual void end_frame() = 0;

    // =========================================================================
    // State Management
    // =========================================================================

    /// Set the viewport
    virtual void set_viewport(int x, int y, int w, int h) = 0;

    /// Set the scissor rectangle
    virtual void set_scissor(int x, int y, int w, int h) = 0;

    /// Clear the current render target
    /// @param r Red component (0.0 - 1.0)
    /// @param g Green component (0.0 - 1.0)
    /// @param b Blue component (0.0 - 1.0)
    /// @param a Alpha component (0.0 - 1.0)
    virtual void clear(float r, float g, float b, float a = 1.0f) = 0;

    /// Clear depth buffer
    /// @param depth Depth clear value (0.0 - 1.0, default 1.0)
    virtual void clear_depth(float depth = 1.0f) = 0;

    /// Clear stencil buffer
    /// @param stencil Stencil clear value
    virtual void clear_stencil(int stencil = 0) = 0;

    // =========================================================================
    // Resource Binding
    // =========================================================================

    /// Bind a pipeline (shader + state)
    virtual void bind_pipeline(RHIPipeline* pipeline) = 0;

    /// Bind a framebuffer (render target)
    /// @param fb Framebuffer to bind, or nullptr for default (screen)
    virtual void bind_framebuffer(RHIFramebuffer* fb) = 0;

    /// Bind a vertex buffer
    /// @param buffer Vertex buffer to bind
    /// @param binding Binding slot index
    /// @param offset Byte offset into buffer
    virtual void bind_vertex_buffer(RHIBuffer* buffer, uint32_t binding = 0, size_t offset = 0) = 0;

    /// Bind an index buffer
    /// @param buffer Index buffer to bind
    /// @param index_type Size of indices (2 for uint16, 4 for uint32)
    virtual void bind_index_buffer(RHIBuffer* buffer, uint32_t index_type = 4) = 0;

    /// Bind a texture to a texture unit
    /// @param texture Texture to bind
    /// @param unit Texture unit index
    virtual void bind_texture(const RHITexture* texture, uint32_t unit) = 0;

    /// Bind a buffer as a storage buffer (SSBO)
    /// @param buffer Buffer to bind
    /// @param slot Binding slot index
    virtual void bind_storage_buffer(RHIBuffer* buffer, uint32_t slot) = 0;

    /// Bind a texture as an image (for compute shaders)
    /// @param texture Texture to bind
    /// @param unit Image unit index
    /// @param access Read/write access mode
    virtual void bind_image(RHITexture* texture, uint32_t unit, ImageAccess access) = 0;

    // =========================================================================
    // Draw Commands
    // =========================================================================

    /// Draw non-indexed primitives
    /// @param vertex_count Number of vertices to draw
    /// @param first_vertex Offset to first vertex
    /// @param instance_count Number of instances (1 for non-instanced)
    virtual void draw(uint32_t vertex_count, uint32_t first_vertex = 0, uint32_t instance_count = 1) = 0;

    /// Draw indexed primitives
    /// @param index_count Number of indices to draw
    /// @param first_index Offset to first index
    /// @param vertex_offset Offset added to each index
    /// @param instance_count Number of instances (1 for non-instanced)
    virtual void draw_indexed(uint32_t index_count, uint32_t first_index = 0,
                              int vertex_offset = 0, uint32_t instance_count = 1) = 0;

    // =========================================================================
    // Compute Commands
    // =========================================================================

    /// Dispatch a compute shader
    /// @param groups_x Number of work groups in X
    /// @param groups_y Number of work groups in Y
    /// @param groups_z Number of work groups in Z
    virtual void dispatch_compute(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) = 0;

    // =========================================================================
    // Data Transfer
    // =========================================================================

    /// Asynchronously copy texture data to a buffer (for GPU->CPU readback).
    /// The copy is queued and completes asynchronously. Use map_read() on the
    /// destination buffer in a subsequent frame to access the data.
    /// @param src_texture Source texture to read from
    /// @param x X offset in pixels
    /// @param y Y offset in pixels
    /// @param w Width in pixels
    /// @param h Height in pixels
    /// @param dst_buffer Destination buffer (should be PixelPack type)
    /// @param dst_offset Byte offset into destination buffer
    virtual void copy_texture_to_buffer(
        const RHITexture* src_texture,
        int x, int y, int w, int h,
        RHIBuffer* dst_buffer,
        size_t dst_offset = 0) = 0;

    // =========================================================================
    // Synchronization
    // =========================================================================

    /// Insert a memory barrier
    /// @param flags Which memory operations to synchronize
    virtual void memory_barrier(BarrierFlags flags) = 0;

    // =========================================================================
    // Debug
    // =========================================================================

    /// Check for and log any backend errors
    /// @param context Optional context string for error messages
    /// @return true if an error occurred
    virtual bool check_error(const char* context = nullptr) = 0;

protected:
    RHIContext() = default;
};

} // namespace engine::rhi
