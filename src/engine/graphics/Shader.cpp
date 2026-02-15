#include "engine/graphics/Shader.h"
#include "engine/core/Log.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine::graphics {

static constexpr int GL_LOG_BUFFER_SIZE = 1024;

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

/// Recursively resolve #include "filename" directives in GLSL source.
/// Resolves paths relative to the including file's directory.
/// Guards against circular includes via the `included` set.
static std::string resolve_includes(const std::string& source,
                                     const std::filesystem::path& base_dir,
                                     std::unordered_set<std::string>& included) {
    std::istringstream stream(source);
    std::ostringstream result;
    std::string line;

    while (std::getline(stream, line)) {
        // Check for #include "filename"
        auto pos = line.find("#include");
        if (pos != std::string::npos) {
            auto quote_start = line.find('"', pos);
            auto quote_end = line.find('"', quote_start + 1);
            if (quote_start != std::string::npos && quote_end != std::string::npos) {
                std::string include_name = line.substr(quote_start + 1, quote_end - quote_start - 1);
                auto include_path = (base_dir / include_name).lexically_normal();
                std::string key = include_path.string();

                if (included.count(key)) {
                    // Already included — skip (include guard)
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
                // Recursively resolve includes in the included file
                result << resolve_includes(inc_ss.str(), include_path.parent_path(), included);
                continue;
            }
        }
        result << line << '\n';
    }
    return result.str();
}

std::string Shader::read_file(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ENGINE_ERR("Failed to open shader: %s", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();

    // Resolve #include directives relative to the shader's directory
    std::unordered_set<std::string> included;
    auto base_dir = std::filesystem::path(path).parent_path();
    if (base_dir.empty()) base_dir = ".";
    included.insert(std::filesystem::path(path).lexically_normal().string());
    return resolve_includes(ss.str(), base_dir, included);
}

uint32_t Shader::compile_shader(uint32_t type, const char* source, const char* path) {
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
        char log[GL_LOG_BUFFER_SIZE];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        ENGINE_ERR("Compute shader link error:\n%s", log);
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(comp);

    if (m_program) {
        // Validate workgroup size against hardware limits
        GLint wg_size[3] = {0, 0, 0};
        glGetProgramiv(m_program, GL_COMPUTE_WORK_GROUP_SIZE, wg_size);
        GLint max_invocations;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_invocations);
        int total = wg_size[0] * wg_size[1] * wg_size[2];
        if (total > max_invocations) {
            ENGINE_ERR("Compute shader '%s' workgroup size %dx%dx%d (%d) exceeds "
                       "GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS (%d)",
                       comp_path, wg_size[0], wg_size[1], wg_size[2],
                       total, max_invocations);
        }

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
    if (!m_program) return -1;
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) return it->second;
    int loc = glGetUniformLocation(m_program, name);
    if (loc < 0) {
        ENGINE_LOG_WARN("Shader: uniform '%s' not found (program %u)", name, m_program);
        return loc;  // Don't cache invalid locations
    }
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

void Shader::set_bool(const char* name, bool value) const {
    glUniform1i(get_location(name), value ? 1 : 0);
}

void Shader::set_vec2(const char* name, float x, float y) const {
    glUniform2f(get_location(name), x, y);
}

void Shader::set_vec3(const char* name, float x, float y, float z) const {
    glUniform3f(get_location(name), x, y, z);
}

void Shader::set_vec4(const char* name, float x, float y, float z, float w) const {
    glUniform4f(get_location(name), x, y, z, w);
}

void Shader::set_mat3(const char* name, const float* value, bool transpose) const {
    glUniformMatrix3fv(get_location(name), 1, transpose ? GL_TRUE : GL_FALSE, value);
}

void Shader::set_mat4(const char* name, const float* value, bool transpose) const {
    glUniformMatrix4fv(get_location(name), 1, transpose ? GL_TRUE : GL_FALSE, value);
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

    // On failure, do NOT snapshot_times() — this allows recovery when the
    // file is fixed, since files_changed() will still detect a difference.
    if (is_compute) {
        std::string src = read_file(m_comp_path.c_str());
        if (src.empty()) { return false; }

        GLuint comp = compile_shader(GL_COMPUTE_SHADER, src.c_str(), m_comp_path.c_str());
        if (!comp) { return false; }

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
        if (vert_src.empty() || frag_src.empty()) { return false; }

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
    snapshot_times();  // Only snapshot on successful reload
    ENGINE_LOG("Shader hot-reloaded: %s",
               is_compute ? m_comp_path.c_str() : m_vert_path.c_str());
    return true;
}

} // namespace engine::graphics
