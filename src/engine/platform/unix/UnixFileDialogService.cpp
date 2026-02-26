#include "UnixFileDialogService.h"

#ifndef _WIN32

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace engine::platform {

UnixFileDialogService::UnixFileDialogService() {
    // Detect available dialog tool on construction
    if (has_command("zenity")) {
        m_tool = DialogTool::Zenity;
    } else if (has_command("kdialog")) {
        m_tool = DialogTool::Kdialog;
    }
}

bool UnixFileDialogService::has_command(const std::string& cmd) {
    if (cmd.empty()) return false;
    for (char c : cmd) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            return false;
        }
    }
    std::string check = "which " + cmd + " > /dev/null 2>&1";
    return system(check.c_str()) == 0;
}

std::string UnixFileDialogService::run_command_capture(const std::string& cmd) {
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

std::string UnixFileDialogService::shell_escape(const std::string& s) {
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

std::string UnixFileDialogService::build_zenity_filters(const std::vector<FileFilter>& filters) {
    std::string result;
    for (const auto& f : filters) {
        result += " --file-filter=" + shell_escape(f.description + " | " + f.pattern);
    }
    return result;
}

std::string UnixFileDialogService::open_file(
    const std::string& title,
    const std::vector<FileFilter>& filters)
{
    switch (m_tool) {
        case DialogTool::Zenity: {
            std::string cmd = "zenity --file-selection --title=" + shell_escape(title);
            cmd += build_zenity_filters(filters);
            return run_command_capture(cmd);
        }
        case DialogTool::Kdialog: {
            std::string filter;
            for (const auto& f : filters) {
                if (!filter.empty()) filter += " ";
                filter += f.pattern;
            }
            std::string cmd = "kdialog --getopenfilename . " + shell_escape(filter) + " --title " + shell_escape(title);
            return run_command_capture(cmd);
        }
        default:
            return {};
    }
}

std::string UnixFileDialogService::save_file(
    const std::string& title,
    const std::vector<FileFilter>& filters,
    const std::string& default_ext,
    const std::string& initial_dir)
{
    std::string result;

    switch (m_tool) {
        case DialogTool::Zenity: {
            std::string cmd = "zenity --file-selection --save --confirm-overwrite --title=" + shell_escape(title);
            cmd += build_zenity_filters(filters);
            if (!initial_dir.empty()) {
                cmd += " --filename=" + shell_escape(initial_dir + "/");
            }
            result = run_command_capture(cmd);
            break;
        }
        case DialogTool::Kdialog: {
            std::string filter;
            for (const auto& f : filters) {
                if (!filter.empty()) filter += " ";
                filter += f.pattern;
            }
            std::string start = initial_dir.empty() ? "." : initial_dir;
            std::string cmd = "kdialog --getsavefilename " + shell_escape(start) + " " + shell_escape(filter) + " --title " + shell_escape(title);
            result = run_command_capture(cmd);
            break;
        }
        default:
            return {};
    }

    // Append default extension if user didn't provide one
    if (!result.empty() && !default_ext.empty()) {
        std::filesystem::path p(result);
        if (!p.has_extension()) {
            std::string ext = default_ext;
            if (!ext.empty() && ext[0] != '.') {
                ext = "." + ext;
            }
            result += ext;
        }
    }

    return result;
}

std::string UnixFileDialogService::select_folder(const std::string& title) {
    switch (m_tool) {
        case DialogTool::Zenity: {
            std::string cmd = "zenity --file-selection --directory --title=" + shell_escape(title);
            return run_command_capture(cmd);
        }
        case DialogTool::Kdialog: {
            std::string cmd = "kdialog --getexistingdirectory . --title " + shell_escape(title);
            return run_command_capture(cmd);
        }
        default:
            return {};
    }
}

}

#endif // !_WIN32
