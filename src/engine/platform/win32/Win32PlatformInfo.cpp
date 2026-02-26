#include "Win32PlatformInfo.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <array>
#include <vector>
#include <memory>

namespace engine::platform {

std::string Win32PlatformInfo::executable_directory() const {
    wchar_t path[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(path).parent_path().string();
    }
    return std::filesystem::current_path().string();
}

std::string Win32PlatformInfo::user_config_directory() const {
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        return appdata;
    }
    return executable_directory();
}

std::string Win32PlatformInfo::user_documents_directory() const {
    namespace fs = std::filesystem;
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        return (fs::path(userprofile) / "Documents").string();
    }
    return executable_directory();
}

std::string Win32PlatformInfo::find_cmake() const {
    namespace fs = std::filesystem;

    // Check for bundled CMake relative to the editor executable
    fs::path exe_dir = fs::path(executable_directory());
    fs::path bundled_cmake = exe_dir / "tools" / "cmake" / "bin" / "cmake.exe";
    if (fs::exists(bundled_cmake)) {
        return bundled_cmake.string();
    }

    // Check one level up (in case we're in Debug/Release subfolder)
    bundled_cmake = exe_dir.parent_path() / "tools" / "cmake" / "bin" / "cmake.exe";
    if (fs::exists(bundled_cmake)) {
        return bundled_cmake.string();
    }

    // Common CMake installation paths on Windows
    std::vector<std::string> search_paths = {
        "C:\\Program Files\\CMake\\bin\\cmake.exe",
        "C:\\Program Files (x86)\\CMake\\bin\\cmake.exe",
        "C:\\cmake\\bin\\cmake.exe",
    };

    // Check Visual Studio installations
    char* pf_raw = nullptr;
    size_t pf_len = 0;
    if (_dupenv_s(&pf_raw, &pf_len, "ProgramFiles") == 0 && pf_raw) {
        std::unique_ptr<char, decltype(&free)> program_files(pf_raw, &free);
        search_paths.push_back(std::string(program_files.get()) +
            "\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe");
        search_paths.push_back(std::string(program_files.get()) +
            "\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe");
        search_paths.push_back(std::string(program_files.get()) +
            "\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe");
    }

    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            return path;
        }
    }

    // Try system PATH as fallback (using where command)
    std::array<char, 512> buffer;
    FILE* pipe = _popen("where cmake 2>nul", "r");
    if (pipe) {
        if (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            std::string result = buffer.data();
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
            _pclose(pipe);
            if (!result.empty() && fs::exists(result)) {
                return result;
            }
        } else {
            _pclose(pipe);
        }
    }

    return "";
}

} // namespace engine::platform

#endif // _WIN32
