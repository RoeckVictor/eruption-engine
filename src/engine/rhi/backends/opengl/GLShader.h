#pragma once

#include "engine/rhi/RHIShader.h"
#include <unordered_map>
#include <string>
#include <filesystem>

namespace engine::rhi {

// OpenGL implementation of RHIShader
class GLShader : public RHIShader {
public:
    GLShader() = default;
    ~GLShader() override;

    GLShader(GLShader&& other) noexcept;
    GLShader& operator=(GLShader&& other) noexcept;

    bool init(const ShaderDesc& desc);
    bool init_graphics(const char* vert_path, const char* frag_path);
    bool init_compute(const char* comp_path);

    void destroy();

    void bind() override;
    void set_int(const char* name, int value) override;
    void set_uint(const char* name, uint32_t value) override;
    void set_float(const char* name, float value) override;
    void set_vec2(const char* name, float x, float y) override;
    void set_vec3(const char* name, float x, float y, float z) override;
    void set_vec4(const char* name, float x, float y, float z, float w) override;
    void set_mat3(const char* name, const float* value, bool transpose) override;
    void set_mat4(const char* name, const float* value, bool transpose) override;
    bool try_reload() override;
    void* native_handle() const override { return reinterpret_cast<void*>(static_cast<uintptr_t>(m_program)); }

    uint32_t handle() const { return m_program; }

private:
    int get_location(const char* name) const;
    void snapshot_times();
    bool files_changed() const;

    static std::string read_file(const char* path);
    static uint32_t compile_shader(uint32_t type, const char* source, const char* path);

    using FileTime = std::filesystem::file_time_type;
    static FileTime safe_last_write(const std::string& path);

    uint32_t m_program = 0;
    mutable std::unordered_map<std::string, int> m_uniform_cache;

    std::string m_vert_path;
    std::string m_frag_path;
    std::string m_comp_path;

    FileTime m_vert_time{};
    FileTime m_frag_time{};
    FileTime m_comp_time{};
};

}
