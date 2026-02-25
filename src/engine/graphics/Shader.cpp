#include "engine/graphics/Shader.h"
#include "engine/rhi/RHI.h"
#include "engine/core/Log.h"

namespace engine::graphics {

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : m_shader(std::move(other.m_shader))
{
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        m_shader = std::move(other.m_shader);
    }
    return *this;
}

bool Shader::load_graphics(const char* vert_path, const char* frag_path) {
    destroy();

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("No RHI device available for shader creation");
        return false;
    }

    rhi::GraphicsShaderDesc desc;
    desc.vertex_path = vert_path;
    desc.fragment_path = frag_path;

    m_shader = device->create_graphics_shader(desc);
    return m_shader != nullptr && m_shader->valid();
}

bool Shader::load_compute(const char* comp_path) {
    destroy();

    auto* device = rhi::get_current_device();
    if (!device) {
        ENGINE_ERR("No RHI device available for shader creation");
        return false;
    }

    rhi::ComputeShaderDesc desc;
    desc.compute_path = comp_path;

    m_shader = device->create_compute_shader(desc);
    return m_shader != nullptr && m_shader->valid();
}

void Shader::use() const {
    if (m_shader) {
        m_shader->bind();
    }
}

void Shader::destroy() {
    m_shader.reset();
}

bool Shader::try_reload() {
    if (!m_shader) return false;
    return m_shader->try_reload();
}

void Shader::set_int(const char* name, int value) const {
    if (m_shader) m_shader->set_int(name, value);
}

void Shader::set_uint(const char* name, uint32_t value) const {
    if (m_shader) m_shader->set_uint(name, value);
}

void Shader::set_float(const char* name, float value) const {
    if (m_shader) m_shader->set_float(name, value);
}

void Shader::set_bool(const char* name, bool value) const {
    if (m_shader) m_shader->set_int(name, value ? 1 : 0);
}

void Shader::set_vec2(const char* name, float x, float y) const {
    if (m_shader) m_shader->set_vec2(name, x, y);
}

void Shader::set_vec3(const char* name, float x, float y, float z) const {
    if (m_shader) m_shader->set_vec3(name, x, y, z);
}

void Shader::set_vec4(const char* name, float x, float y, float z, float w) const {
    if (m_shader) m_shader->set_vec4(name, x, y, z, w);
}

void Shader::set_mat3(const char* name, const float* value, bool transpose) const {
    if (m_shader) m_shader->set_mat3(name, value, transpose);
}

void Shader::set_mat4(const char* name, const float* value, bool transpose) const {
    if (m_shader) m_shader->set_mat4(name, value, transpose);
}

uint32_t Shader::handle() const {
    if (!m_shader) return 0;
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(m_shader->native_handle()));
}

bool Shader::valid() const {
    return m_shader != nullptr && m_shader->valid();
}

} // namespace engine::graphics
