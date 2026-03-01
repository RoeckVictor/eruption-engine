#pragma once

#include "engine/platform/IProcessRunner.h"

namespace engine::platform {

// Windows implementation of IProcessRunner
// Uses CreateProcess for process management
class Win32ProcessRunner : public IProcessRunner {
public:
    Win32ProcessRunner() = default;
    ~Win32ProcessRunner() override = default;

    ProcessResult run_sync(
        const std::string& command,
        uint32_t timeout_ms) override;

    ProcessResult run_sync(
        const std::string& exe_path,
        const std::vector<std::string>& args,
        uint32_t timeout_ms) override;

    bool launch_detached(
        const std::string& exe_path,
        const std::vector<std::string>& args) override;

    void open_folder_in_file_manager(const std::string& folder_path) override;
    void reveal_in_file_manager(const std::string& file_path) override;

private:
    static std::wstring build_command_line(const std::string& exe_path, const std::vector<std::string>& args);
};

}
