#pragma once

#include "RHITypes.h"
#include <string>

namespace engine::rhi {

/// Abstract shader program (vertex+fragment or compute)
class RHIShader {
public:
    virtual ~RHIShader() = default;

    // Non-copyable
    RHIShader(const RHIShader&) = delete;
    RHIShader& operator=(const RHIShader&) = delete;

    /// Bind this shader for use in rendering
    virtual void bind() = 0;

    /// Set a uniform integer value
    virtual void set_int(const char* name, int value) = 0;

    /// Set a uniform unsigned integer value
    virtual void set_uint(const char* name, uint32_t value) = 0;

    /// Set a uniform float value
    virtual void set_float(const char* name, float value) = 0;

    /// Set a uniform vec2 value
    virtual void set_vec2(const char* name, float x, float y) = 0;

    /// Set a uniform vec3 value
    virtual void set_vec3(const char* name, float x, float y, float z) = 0;

    /// Set a uniform vec4 value
    virtual void set_vec4(const char* name, float x, float y, float z, float w) = 0;

    /// Set a uniform mat3 value
    virtual void set_mat3(const char* name, const float* value, bool transpose = false) = 0;

    /// Set a uniform mat4 value
    virtual void set_mat4(const char* name, const float* value, bool transpose = false) = 0;

    /// Attempt to reload the shader from disk
    /// @return true if reload succeeded, false otherwise (keeps old shader)
    virtual bool try_reload() = 0;

    /// Get the shader's native handle (backend-specific)
    /// For OpenGL: GLuint program ID
    /// For Vulkan: VkPipeline or VkShaderModule
    virtual void* native_handle() const = 0;

    /// Check if this is a compute shader
    bool is_compute() const { return m_is_compute; }

    bool valid() const { return m_valid; }

protected:
    RHIShader() = default;

    bool m_is_compute = false;
    bool m_valid = false;
};

} // namespace engine::rhi
