#include "UnixProcessRunner.h"

#ifndef _WIN32

#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <poll.h>

namespace engine::platform {

ProcessResult UnixProcessRunner::run_sync(
    const std::string& command,
    uint32_t timeout_ms)
{
    ProcessResult result;

    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        result.launch_failed = true;
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        result.launch_failed = true;
        return result;
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Set non-blocking
    fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

    auto start_time = std::chrono::steady_clock::now();
    bool done = false;
    int status = 0;

    while (!done) {
        // Check if process has exited
        pid_t wait_result = waitpid(pid, &status, WNOHANG);
        if (wait_result == pid) {
            done = true;
        } else if (wait_result < 0) {
            done = true;
            result.launch_failed = true;
        }

        // Read available output
        char buffer[4096];
        ssize_t bytes;

        while ((bytes = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            result.stdout_output += buffer;
        }

        while ((bytes = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            result.stderr_output += buffer;
        }

        // Check timeout
        if (!done && timeout_ms > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            if (elapsed_ms >= timeout_ms) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                result.timed_out = true;
                done = true;
            }
        }

        if (!done) {
            // Small sleep to avoid busy-waiting
            usleep(10000);  // 10ms
        }
    }

    // Read any remaining output
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        result.stdout_output += buffer;
    }
    while ((bytes = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        result.stderr_output += buffer;
    }

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    } else {
        result.exit_code = -1;
    }

    return result;
}

ProcessResult UnixProcessRunner::run_sync(
    const std::string& exe_path,
    const std::vector<std::string>& args,
    uint32_t timeout_ms)
{
    // Build command string with proper escaping
    std::string command = "'" + exe_path + "'";
    for (const auto& arg : args) {
        command += " '";
        for (char c : arg) {
            if (c == '\'') {
                command += "'\\''";
            } else {
                command += c;
            }
        }
        command += "'";
    }
    return run_sync(command, timeout_ms);
}

bool UnixProcessRunner::launch_detached(
    const std::string& exe_path,
    const std::vector<std::string>& args)
{
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        // Child process
        setsid();  // Create new session

        // Build argv array
        std::vector<const char*> argv;
        argv.push_back(exe_path.c_str());
        for (const auto& arg : args) {
            argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);

        execvp(exe_path.c_str(), const_cast<char* const*>(argv.data()));
        _exit(1);
    }

    // Parent - don't wait
    return true;
}

void UnixProcessRunner::open_folder_in_file_manager(const std::string& folder_path) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", folder_path.c_str(), nullptr);
        _exit(1);
    }
}

void UnixProcessRunner::reveal_in_file_manager(const std::string& file_path) {
    // Try to open the parent directory with xdg-open
    std::filesystem::path parent = std::filesystem::path(file_path).parent_path();
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", parent.string().c_str(), nullptr);
        _exit(1);
    }
}

}

#endif // !_WIN32
