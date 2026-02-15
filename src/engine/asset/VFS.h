#pragma once

#include "engine/core/Result.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine::asset {

/// Result of a VFS file read.
struct FileData {
    std::vector<uint8_t> bytes;
};

/// Virtual filesystem with mount-point stacking.
/// Later mounts override earlier ones (highest priority = last mounted).
class VFS {
public:
    /// Mount a disk directory at a virtual prefix.
    /// e.g., mount_directory("assets", "data/") means "data/textures/foo.png"
    ///        resolves to "assets/textures/foo.png" on disk.
    /// Returns an error if the directory doesn't exist or can't be accessed.
    Result<void, ErrorInfo> mount_directory(const std::string& disk_path,
                                            const std::string& virtual_prefix = "");

    /// Resolve a virtual path to a physical filesystem path.
    /// Returns error if the file doesn't exist in any mount.
    Result<std::string, ErrorInfo> resolve(const std::string& virtual_path) const;

    /// Read a file's contents as raw bytes.
    /// Returns error if file not found or read fails.
    Result<FileData, ErrorInfo> read_file(const std::string& virtual_path) const;

    /// Read a file as a UTF-8 string.
    /// Returns error if file not found or read fails.
    Result<std::string, ErrorInfo> read_text(const std::string& virtual_path) const;

    /// Check if a file exists in any mount.
    bool exists(const std::string& virtual_path) const;

    /// List files in a virtual directory matching an optional extension filter.
    /// e.g., list_files("prefabs", ".json") returns all .json files in prefabs/.
    std::vector<std::string> list_files(const std::string& virtual_dir,
                                         const std::string& extension = "") const;

private:
    struct MountPoint {
        std::string disk_path;
        std::string virtual_prefix;
    };

    /// Try to strip a mount's virtual_prefix from a normalized virtual path.
    /// Returns the relative path on success, or std::nullopt if the path doesn't match.
    static std::optional<std::string> strip_prefix(const std::string& norm_path,
                                                    const std::string& norm_prefix);

    std::vector<MountPoint> m_mounts;  // searched in reverse order
};

} // namespace engine::asset
