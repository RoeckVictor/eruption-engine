#pragma once

#include <string>
#include <memory>

namespace engine::platform {

/// Interface for querying platform-specific information.
/// Implementations are provided for each supported platform (Windows, Linux, macOS).
class IPlatformInfo {
public:
    virtual ~IPlatformInfo() = default;

    /// Get the directory containing the running executable.
    virtual std::string executable_directory() const = 0;

    /// Get the extension for shared libraries (.dll, .so, .dylib).
    virtual const char* shared_library_extension() const = 0;

    /// Get the prefix for shared libraries ("" on Windows, "lib" on Unix).
    virtual const char* shared_library_prefix() const = 0;

    /// Get the extension for executables (.exe on Windows, "" on Unix).
    virtual const char* executable_extension() const = 0;

    /// Get the path separator character ('\' on Windows, '/' on Unix).
    virtual char path_separator() const = 0;

    /// Check if the filesystem uses case-insensitive comparisons.
    virtual bool filesystem_case_insensitive() const = 0;

    /// Get the user's config directory (APPDATA on Windows, ~/.config on Linux).
    virtual std::string user_config_directory() const = 0;

    /// Get the user's documents directory.
    virtual std::string user_documents_directory() const = 0;

    /// Find the CMake executable on the system.
    virtual std::string find_cmake() const = 0;

    /// Construct a full shared library name from a base name.
    std::string shared_library_name(const std::string& base_name) const {
        return std::string(shared_library_prefix()) + base_name + shared_library_extension();
    }
};

/// Create the platform-specific implementation.
std::unique_ptr<IPlatformInfo> create_platform_info();

} // namespace engine::platform
