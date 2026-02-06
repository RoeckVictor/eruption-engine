#include "engine/graphics/Shader.h"
#include "engine/core/Log.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>

namespace engine::graphics {

Shader::~Shader() {
    destroy();
}

Shader::Shader(Shader&& other) noexcept
    : m_program(other.m_program)
    , m_uniform_cache(std::move(other.m_uniform_cache))
    , m_vert_path(std::move(other.m_vert_path))
    , m_frag_path(std::move(other.m_frag_path))
    , m_comp_path(std::move(other.m_comp_path))
    , m_vert_time(other.m_vert_time)
    , m_frag_time(other.m_frag_time)
    , m_comp_time(other.m_comp_time)
{
    other.m_program = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
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
        other.m_program = 0;
    }
    return *this;
}

std::string Shader::read_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ENGINE_ERR("Failed to open shader: %s", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

uint32_t Shader::compile_shader(uint32_t type, const char* source, const char* path) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        ENGINE_ERR("Shader compile error in %s:\n%s", path, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::load_graphics(const char* vert_path, const char* frag_path) {
    destroy(); // Release any existing program before loading a new one

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
        char log[1024];
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
        snapshot_times();
    }
    return m_program != 0;
}

bool Shader::load_compute(const char* comp_path) {
    destroy(); // Release any existing program before loading a new one

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
        char log[1024];
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
        snapshot_times();
    }
    return m_program != 0;
}

void Shader::use() const {
    glUseProgram(m_program);
}

void Shader::destroy() {
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniform_cache.clear();
}

int Shader::get_location(const char* name) const {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) return it->second;
    int loc = glGetUniformLocation(m_program, name);
    m_uniform_cache[name] = loc;
    return loc;
}

void Shader::set_int(const char* name, int value) const {
    glUniform1i(get_location(name), value);
}

void Shader::set_uint(const char* name, uint32_t value) const {
    glUniform1ui(get_location(name), value);
}

void Shader::set_float(const char* name, float value) const {
    glUniform1f(get_location(name), value);
}

void Shader::set_vec2(const char* name, float x, float y) const {
    glUniform2f(get_location(name), x, y);
}

// ---- Hot-reload ----

Shader::FileTime Shader::safe_last_write(const std::string& path) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(path, ec);
    return ec ? FileTime{} : t;
}

void Shader::snapshot_times() {
    if (!m_vert_path.empty()) m_vert_time = safe_last_write(m_vert_path);
    if (!m_frag_path.empty()) m_frag_time = safe_last_write(m_frag_path);
    if (!m_comp_path.empty()) m_comp_time = safe_last_write(m_comp_path);
}

bool Shader::files_changed() const {
    if (!m_vert_path.empty() && safe_last_write(m_vert_path) != m_vert_time) return true;
    if (!m_frag_path.empty() && safe_last_write(m_frag_path) != m_frag_time) return true;
    if (!m_comp_path.empty() && safe_last_write(m_comp_path) != m_comp_time) return true;
    return false;
}

bool Shader::try_reload() {
    if (!m_program) return false;
    if (!files_changed()) return false;

    // Attempt to build a new program from the same source files.
    // On failure, the old program stays active.
    bool is_compute = !m_comp_path.empty();

    if (is_compute) {
        std::string src = read_file(m_comp_path.c_str());
        if (src.empty()) { snapshot_times(); return false; }

        GLuint comp = compile_shader(GL_COMPUTE_SHADER, src.c_str(), m_comp_path.c_str());
        if (!comp) { snapshot_times(); return false; }

        GLuint prog = glCreateProgram();
        glAttachShader(prog, comp);
        glLinkProgram(prog);
        glDeleteShader(comp);

        GLint success;
        glGetProgramiv(prog, GL_LINK_STATUS, &success);
        if (!success) {
            char log[1024];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            ENGINE_ERR("Hot-reload link error:\n%s", log);
            glDeleteProgram(prog);
            snapshot_times();
            return false;
        }

        glDeleteProgram(m_program);
        m_program = prog;
    } else {
        std::string vert_src = read_file(m_vert_path.c_str());
        std::string frag_src = read_file(m_frag_path.c_str());
        if (vert_src.empty() || frag_src.empty()) { snapshot_times(); return false; }

        GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src.c_str(), m_vert_path.c_str());
        GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src.c_str(), m_frag_path.c_str());
        if (!vert || !frag) {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            snapshot_times();
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
            char log[1024];
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            ENGINE_ERR("Hot-reload link error:\n%s", log);
            glDeleteProgram(prog);
            snapshot_times();
            return false;
        }

        glDeleteProgram(m_program);
        m_program = prog;
    }

    m_uniform_cache.clear();
    snapshot_times();
    ENGINE_LOG("Shader hot-reloaded: %s",
               is_compute ? m_comp_path.c_str() : m_vert_path.c_str());
    return true;
}

} // namespace engine::graphics
