#include "PlatformUtils.h"

#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <shellapi.h>
#else
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace engine::platform {

// =============================================================================
// Helpers
// =============================================================================

#ifdef _WIN32

/// Convert a UTF-8 std::string to a wide std::wstring.
static std::wstring utf8_to_wide(const std::string& str) {
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide.data(), len);
    wide.resize(len - 1);  // Remove the null terminator from string length
    return wide;
}

/// Convert a wide std::wstring to a UTF-8 std::string.
static std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string str(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, str.data(), len, nullptr, nullptr);
    str.resize(len - 1);  // Remove the null terminator from string length
    return str;
}

/// Build the double-null-terminated wide filter string for OPENFILENAMEW.
static std::wstring build_win32_filter(const std::vector<FileFilter>& filters) {
    std::wstring result;
    for (const auto& f : filters) {
        result += utf8_to_wide(f.description);
        result += L'\0';
        result += utf8_to_wide(f.pattern);
        result += L'\0';
    }
    result += L'\0';
    return result;
}

#else

/// Escape a string for safe use inside single quotes in shell commands.
/// Replaces each ' with '\'' (end quote, escaped quote, start quote).
static std::string shell_escape(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

/// Run a shell command and capture its stdout. Returns empty on failure/cancel.
static std::string run_command_capture(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::string result;
    try {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        return {};
    }

    int status = pclose(pipe);
    if (status != 0) return {};

    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

/// Check if a command-line tool is available.
/// Only accepts simple command names (alphanumeric, hyphens, underscores).
static bool has_command(const std::string& cmd) {
    if (cmd.empty()) return false;
    for (char c : cmd) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            return false;
        }
    }
    std::string check = "which " + cmd + " > /dev/null 2>&1";
    return system(check.c_str()) == 0;
}

/// Build zenity --file-filter arguments from filter list.
static std::string build_zenity_filters(const std::vector<FileFilter>& filters) {
    std::string result;
    for (const auto& f : filters) {
        result += " --file-filter=" + shell_escape(f.description + " | " + f.pattern);
    }
    return result;
}

#endif

// =============================================================================
// open_file_dialog
// =============================================================================

std::string open_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters)
{
#ifdef _WIN32
    std::wstring filter_str = build_win32_filter(filters);
    std::wstring wide_title = utf8_to_wide(title);

    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wide_title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        return wide_to_utf8(filename);
    }
    return {};

#else
    if (has_command("zenity")) {
        std::string cmd = "zenity --file-selection --title=" + shell_escape(title);
        cmd += build_zenity_filters(filters);
        return run_command_capture(cmd);
    }
    if (has_command("kdialog")) {
        std::string filter;
        for (const auto& f : filters) {
            if (!filter.empty()) filter += " ";
            filter += f.pattern;
        }
        std::string cmd = "kdialog --getopenfilename . " + shell_escape(filter) + " --title " + shell_escape(title);
        return run_command_capture(cmd);
    }
    return {};
#endif
}

// =============================================================================
// save_file_dialog
// =============================================================================

std::string save_file_dialog(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& default_ext,
    const std::string& initial_dir)
{
#ifdef _WIN32
    std::wstring filter_str = build_win32_filter(filters);
    std::wstring wide_title = utf8_to_wide(title);

    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wide_title.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // Windows lpstrDefExt must not include a leading dot
    std::string ext_no_dot = default_ext;
    if (!ext_no_dot.empty() && ext_no_dot[0] == '.') {
        ext_no_dot = ext_no_dot.substr(1);
    }
    std::wstring wide_ext = utf8_to_wide(ext_no_dot);
    std::wstring wide_initial_dir = utf8_to_wide(initial_dir);
    if (!wide_ext.empty()) {
        ofn.lpstrDefExt = wide_ext.c_str();
    }
    if (!wide_initial_dir.empty()) {
        ofn.lpstrInitialDir = wide_initial_dir.c_str();
    }

    if (GetSaveFileNameW(&ofn)) {
        return wide_to_utf8(filename);
    }
    return {};

#else
    if (has_command("zenity")) {
        std::string cmd = "zenity --file-selection --save --confirm-overwrite --title=" + shell_escape(title);
        cmd += build_zenity_filters(filters);
        if (!initial_dir.empty()) {
            cmd += " --filename=" + shell_escape(initial_dir + "/");
        }
        std::string result = run_command_capture(cmd);
        // Append default extension if user didn't provide one
        if (!result.empty() && !default_ext.empty()) {
            std::filesystem::path p(result);
            if (!p.has_extension()) {
                result += "." + default_ext;
            }
        }
        return result;
    }
    if (has_command("kdialog")) {
        std::string filter;
        for (const auto& f : filters) {
            if (!filter.empty()) filter += " ";
            filter += f.pattern;
        }
        std::string start = initial_dir.empty() ? "." : initial_dir;
        std::string cmd = "kdialog --getsavefilename " + shell_escape(start) + " " + shell_escape(filter) + " --title " + shell_escape(title);
        std::string result = run_command_capture(cmd);
        if (!result.empty() && !default_ext.empty()) {
            std::filesystem::path p(result);
            if (!p.has_extension()) {
                result += "." + default_ext;
            }
        }
        return result;
    }
    return {};
#endif
}

// =============================================================================
// folder_dialog
// =============================================================================

std::string folder_dialog(const std::string& title) {
#ifdef _WIN32
    std::string result;

    HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool we_initialized_com = SUCCEEDED(com_hr);
    bool com_available = we_initialized_com || com_hr == RPC_E_CHANGED_MODE;

    if (com_available) {
        IFileDialog* pDialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                              IID_IFileDialog, reinterpret_cast<void**>(&pDialog));

        if (SUCCEEDED(hr)) {
            DWORD options;
            pDialog->GetOptions(&options);
            pDialog->SetOptions(options | FOS_PICKFOLDERS);

            // Set title
            std::wstring wide_title = utf8_to_wide(title);
            if (!wide_title.empty()) {
                pDialog->SetTitle(wide_title.c_str());
            }

            hr = pDialog->Show(nullptr);
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                hr = pDialog->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr)) {
                        result = wide_to_utf8(pszPath);
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pDialog->Release();
        }

        // Only uninitialize if we were the ones who initialized COM
        if (we_initialized_com) {
            CoUninitialize();
        }
    }

    return result;

#else
    if (has_command("zenity")) {
        std::string cmd = "zenity --file-selection --directory --title=" + shell_escape(title);
        return run_command_capture(cmd);
    }
    if (has_command("kdialog")) {
        std::string cmd = "kdialog --getexistingdirectory . --title " + shell_escape(title);
        return run_command_capture(cmd);
    }
    return {};
#endif
}

// =============================================================================
// launch_detached
// =============================================================================

bool launch_detached(const std::string& exe_path, const std::vector<std::string>& args) {
#ifdef _WIN32
    // Build a properly quoted command line for Windows.
    // Each argument is individually quoted to handle spaces safely.
    std::wstring cmd_line = L"\"" + utf8_to_wide(exe_path) + L"\"";
    for (const auto& arg : args) {
        cmd_line += L" \"" + utf8_to_wide(arg) + L"\"";
    }

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

#else
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        // Child process — use execvp with a proper argv array (no shell).
        setsid();
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
#endif
}

// =============================================================================
// open_folder_in_file_manager
// =============================================================================

void open_folder_in_file_manager(const std::string& folder_path) {
#ifdef _WIN32
    std::wstring wide_path = utf8_to_wide(folder_path);
    ShellExecuteW(nullptr, L"open", wide_path.c_str(), nullptr, nullptr, SW_SHOW);
#else
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", folder_path.c_str(), nullptr);
        _exit(1);
    }
#endif
}

// =============================================================================
// reveal_in_file_manager
// =============================================================================

void reveal_in_file_manager(const std::string& file_path) {
#ifdef _WIN32
    std::wstring wide_param = L"/select,\"" + utf8_to_wide(file_path) + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", wide_param.c_str(), nullptr, SW_SHOW);
#else
    // Try to open the parent directory with xdg-open
    std::filesystem::path parent = std::filesystem::path(file_path).parent_path();
    pid_t pid = fork();
    if (pid == 0) {
        execlp("xdg-open", "xdg-open", parent.string().c_str(), nullptr);
        _exit(1);
    }
#endif
}

// =============================================================================
// executable_directory
// =============================================================================

std::string executable_directory() {
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(path).parent_path().string();
    }
    // Fallback to current directory
    return std::filesystem::current_path().string();
#else
    char path[4096] = {};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        return std::filesystem::path(path).parent_path().string();
    }
    return std::filesystem::current_path().string();
#endif
}

} // namespace engine::platform
