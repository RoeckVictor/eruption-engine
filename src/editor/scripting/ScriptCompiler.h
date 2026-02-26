#pragma once

#include "engine/platform/IProcessRunner.h"
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <atomic>
#include <memory>

namespace editor {

// Build status
enum class BuildStatus {
    Idle,
    Configuring,
    Building,
    Success,
    Failed
};

// Compiles user scripts into a DLL
// Generates CMakeLists.txt and invokes CMake
class ScriptCompiler {
public:
    ScriptCompiler();
    ~ScriptCompiler();

    void set_project_path(const std::string& path);
    void set_engine_include_path(const std::string& path);
    void set_engine_build_path(const std::string& path);
    std::string dll_path() const;

    bool generate_cmake();

    void start_build();
    bool is_building() const { return m_status == BuildStatus::Configuring || m_status == BuildStatus::Building; }

    BuildStatus status() const { return m_status; }

    const std::string& build_output() const { return m_build_output; }

    const std::string& last_error() const { return m_last_error; }

    using BuildCompleteCallback = std::function<void(bool success)>;
    void set_build_complete_callback(BuildCompleteCallback callback) { m_build_complete_callback = callback; }

    void update();

    bool build_sync();

private:
    bool run_cmake_configure();
    bool run_cmake_build();
    void build_thread();
    std::string find_cmake() const;

    std::string m_project_path;
    mutable std::string m_cmake_path;
    std::string m_engine_include_path;
    std::string m_engine_build_path;
    std::string m_scripts_path;
    std::string m_build_path;

    std::atomic<BuildStatus> m_status{BuildStatus::Idle};
    std::string m_build_output;
    std::string m_last_error;

    std::future<bool> m_build_future;
    BuildCompleteCallback m_build_complete_callback;

    std::unique_ptr<engine::platform::IProcessRunner> m_process_runner;
};

}