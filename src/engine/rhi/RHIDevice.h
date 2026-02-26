#pragma once

#include "RHITypes.h"
#include <memory>
#include <string>

namespace engine::profiler {
class GPUProfiler;
}

namespace engine::rhi {

// Forward declarations
class RHIBuffer;
class RHITexture;
class RHIShader;
class RHIPipeline;
class RHIFramebuffer;
class RHIContext;

/ Descriptor for shader loading (convenience for graphics shaders)
struct GraphicsShaderDesc {
    const char* vertex_path = nullptr;
    const char* fragment_path = nullptr;
};

// Descriptor for compute shader loading
struct ComputeShaderDesc {
    const char* compute_path = nullptr;
};

// Abstract RHI device - factory for creating all GPU resources
class RHIDevice {
public:
    virtual ~RHIDevice() = default;

    RHIDevice(const RHIDevice&) = delete;
    RHIDevice& operator=(const RHIDevice&) = delete;

    virtual std::unique_ptr<RHIBuffer> create_buffer(const BufferDesc& desc) = 0;
    virtual std::unique_ptr<RHITexture> create_texture(const TextureDesc& desc) = 0;
    virtual std::unique_ptr<RHIShader> create_shader(const ShaderDesc& desc) = 0;
    virtual std::unique_ptr<RHIShader> create_graphics_shader(const GraphicsShaderDesc& desc) = 0;
    virtual std::unique_ptr<RHIShader> create_compute_shader(const ComputeShaderDesc& desc) = 0;
    virtual std::unique_ptr<RHIPipeline> create_pipeline(const PipelineDesc& desc) = 0;
    virtual std::unique_ptr<RHIFramebuffer> create_framebuffer(const FramebufferDesc& desc) = 0;
    virtual std::unique_ptr<RHIFramebuffer> create_simple_framebuffer(
        int width, int height,
        TextureFormat color_format = TextureFormat::RGBA8,
        bool create_depth = true) = 0;

    virtual RHIContext* context() = 0;

    virtual std::unique_ptr<profiler::GPUProfiler> create_gpu_profiler() = 0;

    virtual Backend backend() const = 0;
    virtual const char* backend_name() const = 0;
    virtual const char* renderer_name() const = 0;
    virtual const char* vendor_name() const = 0;

    virtual bool supports_compute() const = 0;
    virtual int max_texture_size() const = 0;
    virtual int max_texture_units() const = 0;
    virtual int max_storage_buffer_bindings() const = 0;
    virtual void max_compute_workgroup_size(int& x, int& y, int& z) const = 0;

protected:
    RHIDevice() = default;
};

using ProcAddressFunc = void* (*)(const char*);

std::unique_ptr<RHIDevice> create_rhi_device(Backend backend, ProcAddressFunc proc_address = nullptr);

void set_current_device(RHIDevice* device);
RHIDevice* get_current_device();
RHIContext* get_current_context();

}
