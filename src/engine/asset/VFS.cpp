#include "engine/asset/VFS.h"
#include "engine/core/Log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace engine::asset {

namespace fs = std::filesystem;

Result<void, ErrorInfo> VFS::mount_directory(const std::string& disk_path,
                                             const std::string& virtual_prefix) {
    // Normalize the disk path to an absolute canonical form
    std::error_code ec;
    auto canonical = fs::canonical(disk_path, ec);
    if (ec) {
        std::string error_msg = "Cannot mount directory: " + ec.message();
        ENGINE_ERR("VFS: %s (path: '%s')", error_msg.c_str(), disk_path.c_str());
        return Err(EngineError::InvalidPath, error_msg, disk_path);
    }

    MountPoint mp;
    mp.disk_path = canonical.string();
    mp.virtual_prefix = virtual_prefix;

    // Remove trailing slashes from prefix for consistent matching
    while (!mp.virtual_prefix.empty() && (mp.virtual_prefix.back() == '/' || mp.virtual_prefix.back() == '\\')) {
        mp.virtual_prefix.pop_back();
    }

    m_mounts.push_back(std::move(mp));
    ENGINE_LOG("VFS: Mounted '%s' at prefix '%s'",
               m_mounts.back().disk_path.c_str(),
               m_mounts.back().virtual_prefix.c_str());

    return Ok();
}

Result<std::string, ErrorInfo> VFS::resolve(const std::string& virtual_path) const {
    // Search mounts in reverse order (latest mount = highest priority)
    for (auto it = m_mounts.rbegin(); it != m_mounts.rend(); ++it) {
        const auto& mp = *it;

        std::string relative_path;
        if (mp.virtual_prefix.empty()) {
            relative_path = virtual_path;
        } else {
            // Check if the virtual path starts with the mount prefix
            if (virtual_path.size() <= mp.virtual_prefix.size()) continue;
            if (virtual_path.compare(0, mp.virtual_prefix.size(), mp.virtual_prefix) != 0) continue;

            char separator = virtual_path[mp.virtual_prefix.size()];
            if (separator != '/' && separator != '\\') continue;

            relative_path = virtual_path.substr(mp.virtual_prefix.size() + 1);
        }

        fs::path candidate = fs::path(mp.disk_path) / relative_path;

        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            return Ok(candidate.string());
        }
    }

    return Err<std::string>(EngineError::FileNotFound,
                            "Virtual path not found in any mount",
                            virtual_path);
}

Result<FileData, ErrorInfo> VFS::read_file(const std::string& virtual_path) const {
    auto physical = resolve(virtual_path);
    if (physical.is_err()) {
        return Err<FileData>(physical.error().code,
                             physical.error().message,
                             physical.error().context);
    }

    std::ifstream file(physical.value(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return Err<FileData>(EngineError::FileReadError,
                             "Failed to open file",
                             virtual_path);
    }

    auto size = file.tellg();
    if (size < 0) {
        return Err<FileData>(EngineError::FileReadError,
                             "Failed to get file size",
                             virtual_path);
    }

    FileData data;
    data.bytes.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.bytes.data()), size);

    if (!file) {
        return Err<FileData>(EngineError::FileReadError,
                             "Failed to read file contents",
                             virtual_path);
    }

    return Ok(std::move(data));
}

Result<std::string, ErrorInfo> VFS::read_text(const std::string& virtual_path) const {
    auto data = read_file(virtual_path);
    if (data.is_err()) {
        return Err<std::string>(data.error().code,
                                data.error().message,
                                data.error().context);
    }
    return Ok(std::string(data.value().bytes.begin(), data.value().bytes.end()));
}

bool VFS::exists(const std::string& virtual_path) const {
    return resolve(virtual_path).is_ok();
}

std::vector<std::string> VFS::list_files(const std::string& virtual_dir,
                                          const std::string& extension) const {
    std::vector<std::string> results;

    for (auto it = m_mounts.rbegin(); it != m_mounts.rend(); ++it) {
        const auto& mp = *it;

        fs::path dir_path;
        if (mp.virtual_prefix.empty()) {
            dir_path = fs::path(mp.disk_path) / virtual_dir;
        } else {
            if (virtual_dir.size() < mp.virtual_prefix.size()) continue;
            if (virtual_dir.compare(0, mp.virtual_prefix.size(), mp.virtual_prefix) != 0) continue;

            std::string relative;
            if (virtual_dir.size() == mp.virtual_prefix.size()) {
                relative = "";
            } else {
                char sep = virtual_dir[mp.virtual_prefix.size()];
                if (sep != '/' && sep != '\\') continue;
                relative = virtual_dir.substr(mp.virtual_prefix.size() + 1);
            }
            dir_path = fs::path(mp.disk_path) / relative;
        }

        std::error_code ec;
        if (!fs::is_directory(dir_path, ec)) continue;

        for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
            if (!entry.is_regular_file()) continue;
            if (!extension.empty()) {
                auto ext = entry.path().extension().string();
                if (ext != extension) continue;
            }

            // Reconstruct the virtual path
            auto rel = fs::relative(entry.path(), mp.disk_path, ec);
            if (ec) continue;

            std::string vpath;
            if (mp.virtual_prefix.empty()) {
                vpath = rel.generic_string();
            } else {
                vpath = mp.virtual_prefix + "/" + rel.generic_string();
            }

            // Avoid duplicates
            if (std::find(results.begin(), results.end(), vpath) == results.end()) {
                results.push_back(std::move(vpath));
            }
        }
    }

    return results;
}

} // namespace engine::asset
