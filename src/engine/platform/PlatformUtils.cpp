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

/// Build the double-null-terminated filter string for OPENFILENAME.
static std::string build_win32_filter(const std::vector<FileFilter>& filters) {
    std::string result;
    for (const auto& f : filters) {
        result += f.description;
        result += '\0';
        result += f.pattern;
        result += '\0';
    }
    result += '\0';
    return result;
}

#else

/// Run a shell command and capture its stdout. Returns empty on failure/cancel.
static std::string run_command_capture(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::string result;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
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
static bool has_command(const std::string& cmd) {
    std::string check = "which " + cmd + " > /dev/null 2>&1";
    return system(check.c_str()) == 0;
}

/// Build zenity --file-filter arguments from filter list.
static std::string build_zenity_filters(const std::vector<FileFilter>& filters) {
    std::string result;
    for (const auto& f : filters) {
        result += " --file-filter=\"" + f.description + " | " + f.pattern + "\"";
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
    std::string filter_str = build_win32_filter(filters);

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return {};

#else
    if (has_command("zenity")) {
        std::string cmd = "zenity --file-selection --title=\"" + title + "\"";
        cmd += build_zenity_filters(filters);
        return run_command_capture(cmd);
    }
    if (has_command("kdialog")) {
        std::string filter;
        for (const auto& f : filters) {
            if (!filter.empty()) filter += " ";
            filter += f.pattern;
        }
        std::string cmd = "kdialog --getopenfilename . \"" + filter + "\" --title \"" + title + "\"";
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
    std::string filter_str = build_win32_filter(filters);

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter_str.c_str();
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (!default_ext.empty()) {
        ofn.lpstrDefExt = default_ext.c_str();
    }
    if (!initial_dir.empty()) {
        ofn.lpstrInitialDir = initial_dir.c_str();
    }

    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
    return {};

#else
    if (has_command("zenity")) {
        std::string cmd = "zenity --file-selection --save --confirm-overwrite --title=\"" + title + "\"";
        cmd += build_zenity_filters(filters);
        if (!initial_dir.empty()) {
            cmd += " --filename=\"" + initial_dir + "/\"";
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
        std::string cmd = "kdialog --getsavefilename \"" + start + "\" \"" + filter + "\" --title \"" + title + "\"";
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

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool com_initialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    if (com_initialized) {
        IFileDialog* pDialog = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                              IID_IFileDialog, reinterpret_cast<void**>(&pDialog));

        if (SUCCEEDED(hr)) {
            DWORD options;
            pDialog->GetOptions(&options);
            pDialog->SetOptions(options | FOS_PICKFOLDERS);

            // Set title
            int wide_len = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
            if (wide_len > 0) {
                std::wstring wide_title(wide_len, 0);
                MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide_title.data(), wide_len);
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
                        int utf8_len = WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, nullptr, 0, nullptr, nullptr);
                        if (utf8_len > 0) {
                            result.resize(utf8_len - 1);
                            WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, result.data(), utf8_len, nullptr, nullptr);
                        }
                        CoTaskMemFree(pszPath);
                    }
                    pItem->Release();
                }
            }
            pDialog->Release();
        }

        // Only uninitialize if we initialized it
        if (SUCCEEDED(hr) || hr != RPC_E_CHANGED_MODE) {
            CoUninitialize();
        }
    }

    return result;

#else
    if (has_command("zenity")) {
        std::string cmd = "zenity --file-selection --directory --title=\"" + title + "\"";
        return run_command_capture(cmd);
    }
    if (has_command("kdialog")) {
        std::string cmd = "kdialog --getexistingdirectory . --title \"" + title + "\"";
        return run_command_capture(cmd);
    }
    return {};
#endif
}

// =============================================================================
// launch_detached
// =============================================================================

bool launch_detached(const std::string& exe_path, const std::string& args) {
#ifdef _WIN32
    std::string cmd_line = "\"" + exe_path + "\"";
    if (!args.empty()) {
        cmd_line += " " + args;
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessA(
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
        // Child process
        setsid();
        if (args.empty()) {
            execl(exe_path.c_str(), exe_path.c_str(), nullptr);
        } else {
            execl("/bin/sh", "sh", "-c",
                  (exe_path + " " + args).c_str(), nullptr);
        }
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
    ShellExecuteA(nullptr, "open", folder_path.c_str(), nullptr, nullptr, SW_SHOW);
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
    std::string param = "/select,\"" + file_path + "\"";
    ShellExecuteA(nullptr, "open", "explorer.exe", param.c_str(), nullptr, SW_SHOW);
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

} // namespace engine::platform
