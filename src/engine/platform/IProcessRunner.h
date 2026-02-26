#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace engine::platform {

struct ProcessResult {
    int exit_code = -1;
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out = false;
    bool launch_failed = false;

    bool success() const {
        return !launch_failed && !timed_out && exit_code == 0;
    }
};

// Abstract interface for running external processes
// Platform-specific implementations handle the actual process management
class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;

    virtual ProcessResult run_sync(
        const std::string& command,
        uint32_t timeout_ms = 0) = 0;

    virtual ProcessResult run_sync(
        const std::string& exe_path,
        const std::vector<std::string>& args,
        uint32_t timeout_ms = 0) = 0;

    virtual bool launch_detached(
        const std::string& exe_path,
        const std::vector<std::string>& args = {}) = 0;

    virtual void open_folder_in_file_manager(const std::string& folder_path) = 0;
    virtual void reveal_in_file_manager(const std::string& file_path) = 0;

protected:
    IProcessRunner() = default;
};

std::unique_ptr<IProcessRunner> create_process_runner();

}
