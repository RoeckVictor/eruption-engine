#pragma once

#include "engine/asset/AssetGUID.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <filesystem>
#include <vector>
#include <mutex>

namespace engine::asset {

using AssetMovedCallback = std::function<void(const std::string&, const std::string&)>;

using AssetDeletedCallback = std::function<void(const std::string&)>;

// Central registry for tracking assets by GUID
// Maintains bidirectional mapping between GUIDs and file paths
// Persists GUIDs in .meta sidecar files alongside each asset
class AssetRegistry {
public:
    AssetRegistry() = default;

    bool init(const std::string& root_path);
    void shutdown();

    AssetGUID get_guid(const std::string& path) const;
    std::string get_path(const AssetGUID& guid) const;
    bool has_path(const std::string& path) const;
    bool has_guid(const AssetGUID& guid) const;

    AssetGUID register_asset(const std::string& path);
    void unregister_asset(const std::string& path);

    void notify_moved(const std::string& old_path, const std::string& new_path);
    void notify_deleted(const std::string& path);

    void rescan();

    void save_cache();

    void load_cache_and_detect_moves();

    void set_moved_callback(AssetMovedCallback callback) { m_moved_callback = std::move(callback); }
    void set_deleted_callback(AssetDeletedCallback callback) { m_deleted_callback = std::move(callback); }

    std::vector<std::string> all_paths() const;
    std::vector<AssetGUID> all_guids() const;

    static bool should_track(const std::filesystem::path& path);

    static std::string meta_path_for(const std::string& asset_path);

private:
    AssetGUID load_or_create_meta(const std::string& asset_path);
    AssetGUID load_meta(const std::string& meta_path);
    bool save_meta(const std::string& meta_path, const AssetGUID& guid);

    void scan_directory(const std::filesystem::path& dir);

    std::string m_root_path;

    std::unordered_map<std::string, AssetGUID> m_path_to_guid;
    std::unordered_map<AssetGUID, std::string> m_guid_to_path;

    std::unordered_set<std::string> m_known_meta_files;

    AssetMovedCallback m_moved_callback;
    AssetDeletedCallback m_deleted_callback;

    mutable std::mutex m_mutex;
};

}
