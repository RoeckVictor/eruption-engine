#pragma once

#include <string>
#include <vector>
#include <functional>
#include <future>
#include <atomic>

namespace editor {

/// Build status.
enum class BuildStatus {
    Idle,           // No build in progress
    Configuring,    // Running CMake configure
    Building,       // Running CMake build
    Success,        // Build completed successfully
    Failed          // Build failed
};

/// Compiles user scripts into a DLL.
/// Generates CMakeLists.txt and invokes CMake.
class ScriptCompiler {
public:
    ScriptCompiler();
    ~ScriptCompiler();

    /// Set the project path (where Scripts/ folder is located).
    void set_project_path(const std::string& path);

    /// Set the path to the engine's include directory (src folder).
    void set_engine_include_path(const std::string& path);

    /// Set the path to the engine's build directory (for FetchContent deps like EnTT).
    void set_engine_build_path(const std::string& path);

    /// Get the output DLL path.
    std::string dll_path() const;

    /// Generate the CMakeLists.txt for user scripts.
    bool generate_cmake();

    /// Start an asynchronous build.
    void start_build();

    /// Check if a build is currently in progress.
    bool is_building() const { return m_status == BuildStatus::Configuring || m_status == BuildStatus::Building; }

    /// Get the current build status.
    BuildStatus status() const { return m_status; }

    /// Get build output/errors.
    const std::string& build_output() const { return m_build_output; }

    /// Get the last error message.
    const std::string& last_error() const { return m_last_error; }

    /// Callback when build completes.
    using BuildCompleteCallback = std::function<void(bool success)>;
    void set_build_complete_callback(BuildCompleteCallback callback) { m_build_complete_callback = callback; }

    /// Poll for build completion (call from main thread).
    void update();

    /// Synchronous build (blocks until complete).
    bool build_sync();

private:
    bool run_cmake_configure();
    bool run_cmake_build();
    void build_thread();
    std::string find_cmake() const;

    std::string m_project_path;
    mutable std::string m_cmake_path;  // Cached cmake executable path
    std::string m_engine_include_path;
    std::string m_engine_build_path;   // Engine build dir for FetchContent deps
    std::string m_scripts_path;
    std::string m_build_path;

    std::atomic<BuildStatus> m_status{BuildStatus::Idle};
    std::string m_build_output;
    std::string m_last_error;

    std::future<bool> m_build_future;
    BuildCompleteCallback m_build_complete_callback;
};

} // namespace editor
