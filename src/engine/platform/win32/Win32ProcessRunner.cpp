#include "Win32ProcessRunner.h"
#include "Win32StringUtils.h"

#ifdef _WIN32

#include <shellapi.h>

namespace engine::platform {

using win32::utf8_to_wide;
using win32::wide_to_utf8;

std::wstring Win32ProcessRunner::build_command_line(
    const std::string& exe_path,
    const std::vector<std::string>& args)
{
    std::wstring cmd_line = L"\"" + utf8_to_wide(exe_path) + L"\"";
    for (const auto& arg : args) {
        cmd_line += L" \"" + utf8_to_wide(arg) + L"\"";
    }
    return cmd_line;
}

ProcessResult Win32ProcessRunner::run_sync(
    const std::string& command,
    uint32_t timeout_ms)
{
    ProcessResult result;

    // Create pipes for stdout/stderr capture
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr, stdout_write = nullptr;
    HANDLE stderr_read = nullptr, stderr_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &sa, 0)) {
        result.launch_failed = true;
        return result;
    }

    // Ensure read handles are not inherited
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    std::wstring wide_cmd = utf8_to_wide(command);

    BOOL success = CreateProcessW(
        nullptr,
        wide_cmd.data(),
        nullptr, nullptr,
        TRUE,  // Inherit handles
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi
    );

    // Close write ends after process creation
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!success) {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        result.launch_failed = true;
        return result;
    }

    // Wait for process with timeout
    DWORD wait_time = (timeout_ms == 0) ? INFINITE : timeout_ms;
    DWORD wait_result = WaitForSingleObject(pi.hProcess, wait_time);

    if (wait_result == WAIT_TIMEOUT) {
        result.timed_out = true;
        TerminateProcess(pi.hProcess, 1);
    }

    // Get exit code
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result.exit_code = static_cast<int>(exit_code);

    // Read stdout
    char buffer[4096];
    DWORD bytes_read;
    while (ReadFile(stdout_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        result.stdout_output += buffer;
    }

    // Read stderr
    while (ReadFile(stderr_read, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        result.stderr_output += buffer;
    }

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

ProcessResult Win32ProcessRunner::run_sync(
    const std::string& exe_path,
    const std::vector<std::string>& args,
    uint32_t timeout_ms)
{
    std::wstring cmd_line = build_command_line(exe_path, args);
    return run_sync(wide_to_utf8(cmd_line), timeout_ms);
}

bool Win32ProcessRunner::launch_detached(
    const std::string& exe_path,
    const std::vector<std::string>& args)
{
    std::wstring cmd_line = build_command_line(exe_path, args);

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessW(
        nullptr,
        cmd_line.data(),
        nullptr, nullptr,
        FALSE,
        0,
        nullptr, nullptr,
        &si, &pi
    );

    if (success) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

void Win32ProcessRunner::open_folder_in_file_manager(const std::string& folder_path) {
    std::wstring wide_path = utf8_to_wide(folder_path);
    ShellExecuteW(nullptr, L"open", wide_path.c_str(), nullptr, nullptr, SW_SHOW);
}

void Win32ProcessRunner::reveal_in_file_manager(const std::string& file_path) {
    std::wstring wide_param = L"/select,\"" + utf8_to_wide(file_path) + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", wide_param.c_str(), nullptr, SW_SHOW);
}

}

#endif // _WIN32
