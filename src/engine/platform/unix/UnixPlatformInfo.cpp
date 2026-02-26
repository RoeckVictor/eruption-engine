#include "UnixPlatformInfo.h"

#ifndef _WIN32

#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace engine::platform {

std::string UnixPlatformInfo::executable_directory() const {
#ifdef __APPLE__
    char path[4096] = {};
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return std::filesystem::path(path).parent_path().string();
    }
    return std::filesystem::current_path().string();
#else
    // Linux
    char path[4096] = {};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        return std::filesystem::path(path).parent_path().string();
    }
    return std::filesystem::current_path().string();
#endif
}

const char* UnixPlatformInfo::shared_library_extension() const {
#ifdef __APPLE__
    return ".dylib";
#else
    return ".so";
#endif
}

std::string UnixPlatformInfo::user_config_directory() const {
    namespace fs = std::filesystem;
    const char* home = std::getenv("HOME");
    if (home) {
#ifdef __APPLE__
        return (fs::path(home) / "Library" / "Application Support").string();
#else
        return (fs::path(home) / ".config").string();
#endif
    }
    return executable_directory();
}

std::string UnixPlatformInfo::user_documents_directory() const {
    namespace fs = std::filesystem;
    const char* home = std::getenv("HOME");
    if (home) {
#ifdef __APPLE__
        return (fs::path(home) / "Documents").string();
#else
        return home;
#endif
    }
    return executable_directory();
}

std::string UnixPlatformInfo::find_cmake() const {
    namespace fs = std::filesystem;

    std::vector<std::string> search_paths = {
        "/usr/bin/cmake",
        "/usr/local/bin/cmake",
        "/opt/homebrew/bin/cmake",           // macOS Homebrew ARM
        "/usr/local/Cellar/cmake/bin/cmake"  // macOS Homebrew Intel
    };

    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            return path;
        }
    }

    // Return "cmake" and hope it's in PATH
    return "cmake";
}

} // namespace engine::platform

#endif // !_WIN32
