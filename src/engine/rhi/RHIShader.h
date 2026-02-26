#pragma once

#include "RHITypes.h"
#include <string>

namespace engine::rhi {

// Abstract shader program (vertex+fragment or compute)
class RHIShader {
public:
    virtual ~RHIShader() = default;

    RHIShader(const RHIShader&) = delete;
    RHIShader& operator=(const RHIShader&) = delete;

    virtual void bind() = 0;

    virtual void set_int(const char* name, int value) = 0;
    virtual void set_uint(const char* name, uint32_t value) = 0;
    virtual void set_float(const char* name, float value) = 0;
    virtual void set_vec2(const char* name, float x, float y) = 0;
    virtual void set_vec3(const char* name, float x, float y, float z) = 0;
    virtual void set_vec4(const char* name, float x, float y, float z, float w) = 0;
    virtual void set_mat3(const char* name, const float* value, bool transpose = false) = 0;
    virtual void set_mat4(const char* name, const float* value, bool transpose = false) = 0;

    virtual bool try_reload() = 0;

    virtual void* native_handle() const = 0;

    bool is_compute() const { return m_is_compute; }

    bool valid() const { return m_valid; }

protected:
    RHIShader() = default;

    bool m_is_compute = false;
    bool m_valid = false;
};

}
