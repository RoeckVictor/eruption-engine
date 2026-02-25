#pragma once

#include "engine/rhi/RHIDevice.h"
#include "GLContext.h"
#include <memory>
#include <string>

namespace engine::rhi {

/// Function pointer type for loading OpenGL function addresses.
/// Compatible with glfwGetProcAddress, SDL_GL_GetProcAddress, etc.
using GLLoadFunc = void* (*)(const char*);

/// OpenGL implementation of RHIDevice
class GLDevice : public RHIDevice {
public:
    GLDevice() = default;
    ~GLDevice() override = default;

    /// Initialize the OpenGL device.
    /// @param load_func Function to load OpenGL function pointers (e.g., glfwGetProcAddress)
    /// @return true on success
    bool init(GLLoadFunc load_func);

    // =========================================================================
    // Resource Creation
    // =========================================================================
    std::unique_ptr<RHIBuffer> create_buffer(const BufferDesc& desc) override;
    std::unique_ptr<RHITexture> create_texture(const TextureDesc& desc) override;
    std::unique_ptr<RHIShader> create_shader(const ShaderDesc& desc) override;
    std::unique_ptr<RHIShader> create_graphics_shader(const GraphicsShaderDesc& desc) override;
    std::unique_ptr<RHIShader> create_compute_shader(const ComputeShaderDesc& desc) override;
    std::unique_ptr<RHIPipeline> create_pipeline(const PipelineDesc& desc) override;
    std::unique_ptr<RHIFramebuffer> create_framebuffer(const FramebufferDesc& desc) override;
    std::unique_ptr<RHIFramebuffer> create_simple_framebuffer(
        int width, int height,
        TextureFormat color_format = TextureFormat::RGBA8,
        bool create_depth = true) override;

    // =========================================================================
    // Context Access
    // =========================================================================
    RHIContext* context() override { return &m_context; }

    // =========================================================================
    // Backend Info
    // =========================================================================
    Backend backend() const override { return Backend::OpenGL; }
    const char* backend_name() const override { return m_backend_name.c_str(); }
    const char* renderer_name() const override { return m_renderer_name.c_str(); }
    const char* vendor_name() const override { return m_vendor_name.c_str(); }

    // =========================================================================
    // Capabilities
    // =========================================================================
    bool supports_compute() const override { return m_supports_compute; }
    int max_texture_size() const override { return m_max_texture_size; }
    int max_texture_units() const override { return m_max_texture_units; }
    int max_storage_buffer_bindings() const override { return m_max_storage_buffer_bindings; }
    void max_compute_workgroup_size(int& x, int& y, int& z) const override;

private:
    GLContext m_context;

    // Backend info strings
    std::string m_backend_name;
    std::string m_renderer_name;
    std::string m_vendor_name;

    // Capabilities
    bool m_supports_compute = false;
    int m_max_texture_size = 0;
    int m_max_texture_units = 0;
    int m_max_storage_buffer_bindings = 0;
    int m_max_compute_workgroup_size[3] = {0, 0, 0};
};

/// Create an OpenGL RHI device.
/// @param load_func Function to load OpenGL function pointers (e.g., glfwGetProcAddress)
/// @return Created device, or nullptr on failure
std::unique_ptr<RHIDevice> create_opengl_device(GLLoadFunc load_func);

} // namespace engine::rhi
