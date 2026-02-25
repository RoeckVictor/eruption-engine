#pragma once
#include <cstdint>
#include <memory>
#include "engine/rhi/RHIShader.h"

namespace engine::graphics {

/// Shader wrapper that delegates to RHI
/// @note For new code, consider using engine::rhi::RHIShader directly
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool load_graphics(const char* vert_path, const char* frag_path);
    bool load_compute(const char* comp_path);
    void use() const;
    void destroy();

    /// Attempt to reload from disk if source files changed.
    /// On success: swaps in the new program, clears uniform cache, returns true.
    /// On failure: keeps the old program, logs the error, returns false.
    bool try_reload();

    void set_int(const char* name, int value) const;
    void set_uint(const char* name, uint32_t value) const;
    void set_float(const char* name, float value) const;
    void set_bool(const char* name, bool value) const;
    void set_vec2(const char* name, float x, float y) const;
    void set_vec3(const char* name, float x, float y, float z) const;
    void set_vec4(const char* name, float x, float y, float z, float w) const;
    void set_mat3(const char* name, const float* value, bool transpose = false) const;
    void set_mat4(const char* name, const float* value, bool transpose = false) const;

    /// Get native handle (for legacy code that needs direct GL access)
    uint32_t handle() const;
    bool valid() const;

    /// Get the underlying RHI shader
    rhi::RHIShader* rhi_shader() { return m_shader.get(); }
    const rhi::RHIShader* rhi_shader() const { return m_shader.get(); }

private:
    std::unique_ptr<rhi::RHIShader> m_shader;
};

} // namespace engine::graphics
