#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace engine::graphics {

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
    void set_vec2(const char* name, float x, float y) const;

    uint32_t handle() const { return m_program; }
    bool valid() const { return m_program != 0; }

private:
    int get_location(const char* name) const;
    void snapshot_times();
    bool files_changed() const;

    uint32_t m_program = 0;
    mutable std::unordered_map<std::string, int> m_uniform_cache;

    // Source paths for hot-reload
    std::string m_vert_path;
    std::string m_frag_path;
    std::string m_comp_path;

    using FileTime = std::filesystem::file_time_type;
    FileTime m_vert_time{};
    FileTime m_frag_time{};
    FileTime m_comp_time{};

    static std::string read_file(const char* path);
    static uint32_t compile_shader(uint32_t type, const char* source, const char* path);
    static FileTime safe_last_write(const std::string& path);
};

} // namespace engine::graphics
