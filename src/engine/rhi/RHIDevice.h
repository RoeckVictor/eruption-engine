#pragma once

#include "RHITypes.h"
#include <memory>
#include <string>

namespace engine::rhi {

// Forward declarations
class RHIBuffer;
class RHITexture;
class RHIShader;
class RHIPipeline;
class RHIFramebuffer;
class RHIContext;

/// Descriptor for shader loading (convenience for graphics shaders)
struct GraphicsShaderDesc {
    const char* vertex_path = nullptr;
    const char* fragment_path = nullptr;
};

/// Descriptor for compute shader loading
struct ComputeShaderDesc {
    const char* compute_path = nullptr;
};

/// Abstract RHI device - factory for creating all GPU resources
class RHIDevice {
public:
    virtual ~RHIDevice() = default;

    // Non-copyable
    RHIDevice(const RHIDevice&) = delete;
    RHIDevice& operator=(const RHIDevice&) = delete;

    // =========================================================================
    // Resource Creation
    // =========================================================================

    /// Create a buffer (vertex, index, uniform, or storage)
    /// @param desc Buffer description
    /// @return Created buffer, or nullptr on failure
    virtual std::unique_ptr<RHIBuffer> create_buffer(const BufferDesc& desc) = 0;

    /// Create a texture
    /// @param desc Texture description
    /// @return Created texture, or nullptr on failure
    virtual std::unique_ptr<RHITexture> create_texture(const TextureDesc& desc) = 0;

    /// Create a shader from individual stage descriptions
    /// @param desc Shader description with stages
    /// @return Created shader, or nullptr on failure
    virtual std::unique_ptr<RHIShader> create_shader(const ShaderDesc& desc) = 0;

    /// Create a graphics shader from vertex and fragment paths (convenience)
    /// @param desc Graphics shader description
    /// @return Created shader, or nullptr on failure
    virtual std::unique_ptr<RHIShader> create_graphics_shader(const GraphicsShaderDesc& desc) = 0;

    /// Create a compute shader from a single path (convenience)
    /// @param desc Compute shader description
    /// @return Created shader, or nullptr on failure
    virtual std::unique_ptr<RHIShader> create_compute_shader(const ComputeShaderDesc& desc) = 0;

    /// Create a pipeline (shader + state)
    /// @param desc Pipeline description
    /// @return Created pipeline, or nullptr on failure
    virtual std::unique_ptr<RHIPipeline> create_pipeline(const PipelineDesc& desc) = 0;

    /// Create a framebuffer (render target)
    /// @param desc Framebuffer description
    /// @return Created framebuffer, or nullptr on failure
    virtual std::unique_ptr<RHIFramebuffer> create_framebuffer(const FramebufferDesc& desc) = 0;

    /// Create a simple framebuffer with internally-managed textures
    /// @param width Framebuffer width
    /// @param height Framebuffer height
    /// @param color_format Format for the color attachment
    /// @param create_depth Whether to create a depth/stencil attachment
    /// @return Created framebuffer, or nullptr on failure
    virtual std::unique_ptr<RHIFramebuffer> create_simple_framebuffer(
        int width, int height,
        TextureFormat color_format = TextureFormat::RGBA8,
        bool create_depth = true) = 0;

    // =========================================================================
    // Context Access
    // =========================================================================

    /// Get the rendering context for this device
    virtual RHIContext* context() = 0;

    // =========================================================================
    // Backend Info
    // =========================================================================

    /// Get the backend type
    virtual Backend backend() const = 0;

    /// Get the backend name string (e.g., "OpenGL 4.5", "Vulkan 1.2")
    virtual const char* backend_name() const = 0;

    /// Get the renderer/GPU name
    virtual const char* renderer_name() const = 0;

    /// Get the vendor name
    virtual const char* vendor_name() const = 0;

    // =========================================================================
    // Capabilities
    // =========================================================================

    /// Check if compute shaders are supported
    virtual bool supports_compute() const = 0;

    /// Get maximum texture size
    virtual int max_texture_size() const = 0;

    /// Get maximum number of texture units
    virtual int max_texture_units() const = 0;

    /// Get maximum number of storage buffer bindings
    virtual int max_storage_buffer_bindings() const = 0;

    /// Get compute shader workgroup size limits
    virtual void max_compute_workgroup_size(int& x, int& y, int& z) const = 0;

protected:
    RHIDevice() = default;
};

/// Function pointer type for loading graphics API function addresses.
/// For OpenGL: glfwGetProcAddress, SDL_GL_GetProcAddress, etc.
using ProcAddressFunc = void* (*)(const char*);

/// Create an RHI device for the specified backend.
/// @param backend Which graphics API to use
/// @param proc_address Function to load API function pointers (required for OpenGL)
/// @return Created device, or nullptr on failure
std::unique_ptr<RHIDevice> create_rhi_device(Backend backend, ProcAddressFunc proc_address = nullptr);

// =========================================================================
// Global Device Accessor
// =========================================================================

/// Set the current RHI device (called by Engine during initialization)
/// @note This does NOT transfer ownership - the caller must keep the device alive
void set_current_device(RHIDevice* device);

/// Get the current RHI device
/// @return The current device, or nullptr if not set
RHIDevice* get_current_device();

/// Get the current RHI context (convenience for get_current_device()->context())
/// @return The current context, or nullptr if no device is set
RHIContext* get_current_context();

} // namespace engine::rhi
