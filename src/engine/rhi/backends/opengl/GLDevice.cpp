#include "GLDevice.h"
#include "GLBuffer.h"
#include "GLTexture.h"
#include "GLShader.h"
#include "GLPipeline.h"
#include "GLFramebuffer.h"
#include "GLGPUProfiler.h"
#include "GLCommandBuffer.h"
#include "GLDescriptorSet.h"
#include "GLPipelineCache.h"
#include "GLSynchronization.h"
#include "engine/rhi/RHIDescriptorSet.h"
#include "engine/rhi/RHIPipelineCache.h"
#include "engine/core/Log.h"
#include <glad/gl.h>

namespace engine::rhi {

bool GLDevice::init(GLLoadFunc load_func) {
    // Load OpenGL function pointers via GLAD
    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(load_func)) == 0) {
        ENGINE_ERR("Failed to load OpenGL functions via GLAD");
        return false;
    }

    // Query backend info
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

    m_backend_name = version ? version : "OpenGL";
    m_renderer_name = renderer ? renderer : "Unknown";
    m_vendor_name = vendor ? vendor : "Unknown";

    // Query capabilities
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_max_texture_size);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_max_texture_units);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &m_max_storage_buffer_bindings);

    // Query compute shader capabilities
    GLint major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    m_supports_compute = (major > 4 || (major == 4 && minor >= 3));

    if (m_supports_compute) {
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &m_max_compute_workgroup_size[0]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &m_max_compute_workgroup_size[1]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &m_max_compute_workgroup_size[2]);
    }

    return true;
}

std::unique_ptr<RHIBuffer> GLDevice::create_buffer(const BufferDesc& desc) {
    auto buffer = std::make_unique<GLBuffer>();
    if (!buffer->init(desc)) {
        return nullptr;
    }
    return buffer;
}

std::unique_ptr<RHITexture> GLDevice::create_texture(const TextureDesc& desc) {
    auto texture = std::make_unique<GLTexture>();
    if (!texture->init(desc)) {
        return nullptr;
    }
    return texture;
}

std::unique_ptr<RHIShader> GLDevice::create_shader(const ShaderDesc& desc) {
    auto shader = std::make_unique<GLShader>();
    if (!shader->init(desc)) {
        return nullptr;
    }
    return shader;
}

std::unique_ptr<RHIShader> GLDevice::create_graphics_shader(const GraphicsShaderDesc& desc) {
    auto shader = std::make_unique<GLShader>();
    if (!shader->init_graphics(desc.vertex_path, desc.fragment_path)) {
        return nullptr;
    }
    return shader;
}

std::unique_ptr<RHIShader> GLDevice::create_compute_shader(const ComputeShaderDesc& desc) {
    auto shader = std::make_unique<GLShader>();
    if (!shader->init_compute(desc.compute_path)) {
        return nullptr;
    }
    return shader;
}

std::unique_ptr<RHIPipeline> GLDevice::create_pipeline(const PipelineDesc& desc) {
    auto pipeline = std::make_unique<GLPipeline>();
    if (!pipeline->init(desc)) {
        return nullptr;
    }
    return pipeline;
}

std::unique_ptr<RHIFramebuffer> GLDevice::create_framebuffer(const FramebufferDesc& desc) {
    auto framebuffer = std::make_unique<GLFramebuffer>();
    if (!framebuffer->init(desc)) {
        return nullptr;
    }
    return framebuffer;
}

std::unique_ptr<RHIFramebuffer> GLDevice::create_simple_framebuffer(
    int width, int height,
    TextureFormat color_format,
    bool create_depth)
{
    auto framebuffer = std::make_unique<GLFramebuffer>();
    if (!framebuffer->init_with_new_textures(width, height, 1, color_format, create_depth)) {
        return nullptr;
    }
    return framebuffer;
}

void GLDevice::max_compute_workgroup_size(int& x, int& y, int& z) const {
    x = m_max_compute_workgroup_size[0];
    y = m_max_compute_workgroup_size[1];
    z = m_max_compute_workgroup_size[2];
}

std::unique_ptr<profiler::GPUProfiler> GLDevice::create_gpu_profiler() {
    auto profiler = std::make_unique<GLGPUProfiler>();
    if (!profiler->init()) {
        return nullptr;
    }
    return profiler;
}

std::unique_ptr<RHICommandBuffer> GLDevice::create_command_buffer() {
    auto cmd_buffer = std::make_unique<GLCommandBuffer>();
    if (!cmd_buffer->init()) {
        return nullptr;
    }
    return cmd_buffer;
}

std::unique_ptr<RHIDescriptorSetLayout> GLDevice::create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc) {
    auto layout = std::make_unique<GLDescriptorSetLayout>();
    if (!layout->init(desc)) {
        return nullptr;
    }
    return layout;
}

std::unique_ptr<RHIDescriptorSet> GLDevice::create_descriptor_set(const RHIDescriptorSetLayout* layout) {
    auto set = std::make_unique<GLDescriptorSet>();
    if (!set->init(static_cast<const GLDescriptorSetLayout*>(layout))) {
        return nullptr;
    }
    return set;
}

std::unique_ptr<RHIPipelineCache> GLDevice::create_pipeline_cache(const PipelineCacheDesc& desc) {
    auto cache = std::make_unique<GLPipelineCache>();
    if (!cache->init(desc)) {
        return nullptr;
    }
    return cache;
}

std::unique_ptr<RHIFence> GLDevice::create_fence() {
    auto fence = std::make_unique<GLFence>();
    if (!fence->init()) {
        return nullptr;
    }
    return fence;
}

std::unique_ptr<RHIEvent> GLDevice::create_event() {
    auto event = std::make_unique<GLEvent>();
    if (!event->init()) {
        return nullptr;
    }
    return event;
}

std::unique_ptr<RHISemaphore> GLDevice::create_semaphore() {
    auto semaphore = std::make_unique<GLSemaphore>();
    if (!semaphore->init()) {
        return nullptr;
    }
    return semaphore;
}

std::unique_ptr<RHITimelineSemaphore> GLDevice::create_timeline_semaphore() {
    auto semaphore = std::make_unique<GLTimelineSemaphore>();
    if (!semaphore->init()) {
        return nullptr;
    }
    return semaphore;
}

std::unique_ptr<RHIDevice> create_opengl_device(GLLoadFunc load_func) {
    auto device = std::make_unique<GLDevice>();
    if (!device->init(load_func)) {
        return nullptr;
    }
    return device;
}

}
