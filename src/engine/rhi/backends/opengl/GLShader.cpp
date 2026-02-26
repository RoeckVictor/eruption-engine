#include "GLShader.h"
#include "engine/core/Log.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine::rhi {

static constexpr int GL_LOG_BUFFER_SIZE = 1024;

namespace {

// Recursively resolve #include "filename" directives in GLSL source.
std::string resolve_includes(const std::string& source,
                             const std::filesystem::path& base_dir,
                             std::unordered_set<std::string>& included) {
    std::istringstream stream(source);
    std::ostringstream result;
    std::string line;

    while (std::getline(stream, line)) {
        auto pos = line.find("#include");
        if (pos != std::string::npos) {
            auto quote_start = line.find('"', pos);
            auto quote_end = line.find('"', quote_start + 1);
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                std::string include_name = line.substr(quote_start + 1, quote_end - quote_start - 1);
                auto include_path = (base_dir / include_name).lexically_normal();
                std::string key = include_path.string();

                if (included.count(key)) {
                    continue;
                }
                included.insert(key);

                std::ifstream inc_file(include_path);
                if (!inc_file.is_open()) {
                    ENGINE_ERR("Shader #include not found: %s", key.c_str());
                    continue;
                }
                std::stringstream inc_ss;
                inc_ss << inc_file.rdbuf();
                result << resolve_includes(inc_ss.str(), include_path.parent_path(), included);
                continue;
            }
        }
        result << line << '\n';
    }
    return result.str();
}

}

GLShader::~GLShader() {
    destroy();
}

GLShader::GLShader(GLShader&& other) noexcept
    : RHIShader()
{
    m_program = other.m_program;
    m_uniform_cache = std::move(other.m_uniform_cache);
    m_vert_path = std::move(other.m_vert_path);
    m_frag_path = std::move(other.m_frag_path);
    m_comp_path = std::move(other.m_comp_path);
    m_vert_time = other.m_vert_time;
    m_frag_time = other.m_frag_time;
    m_comp_time = other.m_comp_time;
    m_is_compute = other.m_is_compute;
    m_valid = other.m_valid;

    other.m_program = 0;
    other.m_valid = false;
}

GLShader& GLShader::operator=(GLShader&& other) noexcept {
    if (this != &other) {
        destroy();

        m_program = other.m_program;
        m_uniform_cache = std::move(other.m_uniform_cache);
        m_vert_path = std::move(other.m_vert_path);
        m_frag_path = std::move(other.m_frag_path);
        m_comp_path = std::move(other.m_comp_path);
        m_vert_time = other.m_vert_time;
        m_frag_time = other.m_frag_time;
        m_comp_time = other.m_comp_time;
        m_is_compute = other.m_is_compute;
        m_valid = other.m_valid;

        other.m_program = 0;
        other.m_valid = false;
    }
    return *this;
}

std::string GLShader::read_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ENGINE_ERR("Failed to open shader: %s", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();

    std::unordered_set<std::string> included;
    auto base_dir = std::filesystem::path(path).parent_path();
    if (base_dir.empty()) base_dir = ".";
    included.insert(std::filesystem::path(path).lexically_normal().string());
    return resolve_includes(ss.str(), base_dir, included);
}

uint32_t GLShader::compile_shader(uint32_t type, const char* source, const char* path) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[GL_LOG_BUFFER_SIZE];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        ENGINE_ERR("Shader compile error in %s:\n%s", path, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool GLShader::init(const ShaderDesc& desc) {
    const char* vert_path = nullptr;
    const char* frag_path = nullptr;
    const char* comp_path = nullptr;

    for (uint32_t i = 0; i < desc.stage_count; ++i) {
        const auto& stage = desc.stages[i];
        switch (stage.stage) {
            case ShaderStage::Vertex:
                vert_path = stage.source_path;
                break;
            case ShaderStage::Fragment:
                frag_path = stage.source_path;
                break;
            case ShaderStage::Compute:
                comp_path = stage.source_path;
                break;
            default:
                // Geometry, tessellation stages not yet supported
                break;
        }
    }

    if (comp_path) {
        return init_compute(comp_path);
    } else if (vert_path && frag_path) {
        return init_graphics(vert_path, frag_path);
    }

    return false;
}

bool GLShader::init_graphics(const char* vert_path, const char* frag_path) {
    destroy();

    std::string vert_src = read_file(vert_path);
    std::string frag_src = read_file(frag_path);
    if (vert_src.empty() || frag_src.empty()) return false;

    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src.c_str(), vert_path);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src.c_str(), frag_path);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vert);
    glAttachShader(m_program, frag);
    glLinkProgram(m_program);

    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[GL_LOG_BUFFER_SIZE];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        ENGINE_ERR("Shader link error:\n%s", log);
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    if (m_program) {
        m_vert_path = vert_path;
        m_frag_path = frag_path;
        m_comp_path.clear();
        m_is_compute = false;
        m_valid = true;
        snapshot_times();
    }
    return m_program != 0;
}

bool GLShader::init_compute(const char* comp_path) {
    destroy();

    std::string src = read_file(comp_path);
    if (src.empty()) return false;

    GLuint comp = compile_shader(GL_COMPUTE_SHADER, src.c_str(), comp_path);
    if (!comp) return false;

    m_program = glCreateProgram();
    glAttachShader(m_program, comp);
    glLinkProgram(m_program);

    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[GL_LOG_BUFFER_SIZE];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        ENGINE_ERR("Compute shader link error:\n%s", log);
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(comp);

    if (m_program) {
        m_vert_path.clear();
        m_frag_path.clear();
        m_comp_path = comp_path;
        m_is_compute = true;
        m_valid = true;
        snapshot_times();
    }
    return m_program != 0;
}

void GLShader::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniform_cache.clear();
    m_valid = false;
}

void GLShader::bind() {
    glUseProgram(m_program);
}

int GLShader::get_location(const char* name) const {
    if (!m_program) return -1;
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) return it->second;
    int loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        m_uniform_cache[name] = loc;
    }
    return loc;
}

void GLShader::set_int(const char* name, int value) {
    glUniform1i(get_location(name), value);
}

void GLShader::set_uint(const char* name, uint32_t value) {
    glUniform1ui(get_location(name), value);
}

void GLShader::set_float(const char* name, float value) {
    glUniform1f(get_location(name), value);
}

void GLShader::set_vec2(const char* name, float x, float y) {
    glUniform2f(get_location(name), x, y);
}

void GLShader::set_vec3(const char* name, float x, float y, float z) {
    glUniform3f(get_location(name), x, y, z);
}

void GLShader::set_vec4(const char* name, float x, float y, float z, float w) {
    glUniform4f(get_location(name), x, y, z, w);
}

void GLShader::set_mat3(const char* name, const float* value, bool transpose) {
    glUniformMatrix3fv(get_location(name), 1, transpose ? GL_TRUE : GL_FALSE, value);
}

void GLShader::set_mat4(const char* name, const float* value, bool transpose) {
    glUniformMatrix4fv(get_location(name), 1, transpose ? GL_TRUE : GL_FALSE, value);
}

GLShader::FileTime GLShader::safe_last_write(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    return ec ? FileTime{} : t;
}

void GLShader::snapshot_times() {
    if (!m_vert_path.empty()) m_vert_time = safe_last_write(m_vert_path);
    if (!m_frag_path.empty()) m_frag_time = safe_last_write(m_frag_path);
    if (!m_comp_path.empty()) m_comp_time = safe_last_write(m_comp_path);
}

bool GLShader::files_changed() const {
    if (!m_vert_path.empty() && safe_last_write(m_vert_path) != m_vert_time) return true;
    if (!m_frag_path.empty() && safe_last_write(m_frag_path) != m_frag_time) return true;
    if (!m_comp_path.empty() && safe_last_write(m_comp_path) != m_comp_time) return true;
    return false;
}

bool GLShader::try_reload() {
    if (!m_program) return false;
    if (!files_changed()) return false;

    if (m_is_compute) {
        std::string src = read_file(m_comp_path.c_str());
        if (src.empty()) return false;

        GLuint comp = compile_shader(GL_COMPUTE_SHADER, src.c_str(), m_comp_path.c_str());
        if (!comp) return false;

        GLuint prog = glCreateProgram();
        glAttachShader(prog, comp);
        glLinkProgram(prog);
        glDeleteShader(comp);

        GLint success;
        glGetProgramiv(prog, GL_LINK_STATUS, &success);
        if (!success) {
            char log[GL_LOG_BUFFER_SIZE];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            ENGINE_ERR("Hot-reload link error:\n%s", log);
            glDeleteProgram(prog);
            return false;
        }

        glDeleteProgram(m_program);
        m_program = prog;
    } else {
        std::string vert_src = read_file(m_vert_path.c_str());
        std::string frag_src = read_file(m_frag_path.c_str());
        if (vert_src.empty() || frag_src.empty()) return false;

        GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src.c_str(), m_vert_path.c_str());
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src.c_str(), m_frag_path.c_str());
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return false;
        }

        GLuint prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);
        glLinkProgram(prog);
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint success;
        glGetProgramiv(prog, GL_LINK_STATUS, &success);
        if (!success) {
            char log[GL_LOG_BUFFER_SIZE];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            ENGINE_ERR("Hot-reload link error:\n%s", log);
            glDeleteProgram(prog);
            return false;
        }

        glDeleteProgram(m_program);
        m_program = prog;
    }

    m_uniform_cache.clear();
    snapshot_times();
    ENGINE_LOG("Shader hot-reloaded: %s",
               m_is_compute ? m_comp_path.c_str() : m_vert_path.c_str());
    return true;
}

}
