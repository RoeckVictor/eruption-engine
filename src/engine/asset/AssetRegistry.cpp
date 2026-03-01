#include "engine/asset/AssetRegistry.h"
#include "engine/core/Log.h"
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace engine::asset {

bool AssetRegistry::init(const std::string& root_path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_root_path = root_path;
    m_path_to_guid.clear();
    m_guid_to_path.clear();
    m_known_meta_files.clear();

    if (!fs::exists(root_path)) {
        ENGINE_ERR("AssetRegistry: Root path does not exist: %s", root_path.c_str());
        return false;
    }

    ENGINE_LOG("AssetRegistry: Scanning assets in '%s'", root_path.c_str());

    scan_directory(root_path);

    ENGINE_LOG("AssetRegistry: Registered %zu assets", m_path_to_guid.size());
    return true;
}

void AssetRegistry::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_path_to_guid.clear();
    m_guid_to_path.clear();
    m_known_meta_files.clear();
    m_root_path.clear();
}

AssetGUID AssetRegistry::get_guid(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_path_to_guid.find(path);
    if (it != m_path_to_guid.end()) {
        return it->second;
    }
    return AssetGUID{};
}

std::string AssetRegistry::get_path(const AssetGUID& guid) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_guid_to_path.find(guid);
    if (it != m_guid_to_path.end()) {
        return it->second;
    }
    return "";
}

bool AssetRegistry::has_path(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_path_to_guid.find(path) != m_path_to_guid.end();
}

bool AssetRegistry::has_guid(const AssetGUID& guid) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_guid_to_path.find(guid) != m_guid_to_path.end();
}

AssetGUID AssetRegistry::register_asset(const std::string& path) {
    // Check if already registered
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_path_to_guid.find(path);
        if (it != m_path_to_guid.end()) {
            return it->second;
        }
    }

    // Load or create .meta file
    AssetGUID guid = load_or_create_meta(path);

    if (guid.valid()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_path_to_guid[path] = guid;
        m_guid_to_path[guid] = path;
        m_known_meta_files.insert(meta_path_for(path));
    }

    return guid;
}

void AssetRegistry::unregister_asset(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_path_to_guid.find(path);
    if (it != m_path_to_guid.end()) {
        m_guid_to_path.erase(it->second);
        m_path_to_guid.erase(it);
        m_known_meta_files.erase(meta_path_for(path));
    }
}

void AssetRegistry::notify_moved(const std::string& old_path, const std::string& new_path) {
    AssetGUID guid;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_path_to_guid.find(old_path);
        if (it != m_path_to_guid.end()) {
            guid = it->second;
            m_path_to_guid.erase(it);
            m_path_to_guid[new_path] = guid;
            m_guid_to_path[guid] = new_path;

            // Update meta file tracking
            m_known_meta_files.erase(meta_path_for(old_path));
            m_known_meta_files.insert(meta_path_for(new_path));

            ENGINE_LOG("AssetRegistry: Asset moved '%s' -> '%s'", old_path.c_str(), new_path.c_str());
        }
    }

    // Invoke callback outside lock
    if (guid.valid() && m_moved_callback) {
        m_moved_callback(old_path, new_path);
    }
}

void AssetRegistry::notify_deleted(const std::string& path) {
    AssetGUID guid;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_path_to_guid.find(path);
        if (it != m_path_to_guid.end()) {
            guid = it->second;
            m_guid_to_path.erase(it->second);
            m_path_to_guid.erase(it);
            m_known_meta_files.erase(meta_path_for(path));

            ENGINE_LOG("AssetRegistry: Asset deleted '%s'", path.c_str());
        }
    }

    // Invoke callback outside lock
    if (guid.valid() && m_deleted_callback) {
        m_deleted_callback(path);
    }
}

void AssetRegistry::rescan() {
    ENGINE_LOG("AssetRegistry: Rescanning for changes...");

    // Build set of currently known paths
    std::unordered_map<AssetGUID, std::string> old_guid_to_path;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        old_guid_to_path = m_guid_to_path;
        m_path_to_guid.clear();
        m_guid_to_path.clear();
        m_known_meta_files.clear();
    }

    // Rescan directory
    if (!m_root_path.empty() && fs::exists(m_root_path)) {
        scan_directory(m_root_path);
    }

    // Collect moves and deletions while holding the lock
    std::vector<std::pair<std::string, std::string>> moves;  // old_path, new_path
    std::vector<std::string> deletions;
    size_t asset_count = 0;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Detect moves by comparing GUIDs
        for (const auto& [guid, new_path] : m_guid_to_path) {
            auto it = old_guid_to_path.find(guid);
            if (it != old_guid_to_path.end() && it->second != new_path) {
                // This GUID moved to a new path
                ENGINE_LOG("AssetRegistry: Detected move '%s' -> '%s'",
                           it->second.c_str(), new_path.c_str());
                moves.emplace_back(it->second, new_path);
            }
        }

        // Detect deletions
        for (const auto& [guid, old_path] : old_guid_to_path) {
            if (m_guid_to_path.find(guid) == m_guid_to_path.end()) {
                // This GUID no longer exists
                ENGINE_LOG("AssetRegistry: Detected deletion '%s'", old_path.c_str());
                deletions.push_back(old_path);
            }
        }

        asset_count = m_path_to_guid.size();
    }

    // Invoke callbacks outside the lock to prevent deadlocks
    if (m_moved_callback) {
        for (const auto& [old_path, new_path] : moves) {
            m_moved_callback(old_path, new_path);
        }
    }
    if (m_deleted_callback) {
        for (const auto& path : deletions) {
            m_deleted_callback(path);
        }
    }

    ENGINE_LOG("AssetRegistry: Rescan complete, %zu assets registered", asset_count);
}

std::vector<std::string> AssetRegistry::all_paths() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> paths;
    paths.reserve(m_path_to_guid.size());
    for (const auto& [path, guid] : m_path_to_guid) {
        paths.push_back(path);
    }
    return paths;
}

std::vector<AssetGUID> AssetRegistry::all_guids() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<AssetGUID> guids;
    guids.reserve(m_guid_to_path.size());
    for (const auto& [guid, path] : m_guid_to_path) {
        guids.push_back(guid);
    }
    return guids;
}

bool AssetRegistry::should_track(const fs::path& path) {
    // Skip .meta files
    if (path.extension() == ".meta") {
        return false;
    }

    // Skip hidden files (starting with .)
    std::string filename = path.filename().string();
    if (!filename.empty() && filename[0] == '.') {
        return false;
    }

    // Skip common non-asset files
    std::string ext = path.extension().string();
    if (ext == ".tmp" || ext == ".bak" || ext == ".swp") {
        return false;
    }

    return true;
}

std::string AssetRegistry::meta_path_for(const std::string& asset_path) {
    return asset_path + ".meta";
}

AssetGUID AssetRegistry::load_or_create_meta(const std::string& asset_path) {
    std::string meta = meta_path_for(asset_path);

    // Try to load existing .meta
    if (fs::exists(meta)) {
        AssetGUID guid = load_meta(meta);
        if (guid.valid()) {
            return guid;
        }
        // Invalid .meta file, regenerate
        ENGINE_LOG_WARN("AssetRegistry: Regenerating invalid .meta for '%s'", asset_path.c_str());
    }

    // Generate new GUID and save
    AssetGUID guid = AssetGUID::generate();
    if (save_meta(meta, guid)) {
        ENGINE_LOG("AssetRegistry: Created .meta for '%s'", asset_path.c_str());
        return guid;
    }

    ENGINE_ERR("AssetRegistry: Failed to create .meta for '%s'", asset_path.c_str());
    return AssetGUID{};
}

AssetGUID AssetRegistry::load_meta(const std::string& meta_path) {
    std::ifstream file(meta_path);
    if (!file.is_open()) {
        return AssetGUID{};
    }

    // Simple format: just the GUID string on a single line
    // Future: Could expand to JSON with more metadata
    std::string line;
    if (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            line = line.substr(start, end - start + 1);
        }
        return AssetGUID::from_string(line);
    }

    return AssetGUID{};
}

bool AssetRegistry::save_meta(const std::string& meta_path, const AssetGUID& guid) {
    std::ofstream file(meta_path);
    if (!file.is_open()) {
        return false;
    }

    file << guid.to_string() << "\n";
    return file.good();
}

void AssetRegistry::scan_directory(const fs::path& dir) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec);
         it != fs::recursive_directory_iterator(); ++it) {

        if (ec) {
            ENGINE_LOG_WARN("AssetRegistry: Error scanning directory: %s", ec.message().c_str());
            ec.clear();
            continue;
        }

        if (!it->is_regular_file()) {
            continue;
        }

        fs::path path = it->path();

        if (!should_track(path)) {
            continue;
        }

        std::string path_str = path.string();

        // Normalize path separators
        for (char& c : path_str) {
            if (c == '\\') c = '/';
        }

        // Load or create .meta and register
        AssetGUID guid = load_or_create_meta(path_str);
        if (guid.valid()) {
            // Note: This is called during init, so we don't acquire lock again
            m_path_to_guid[path_str] = guid;
            m_guid_to_path[guid] = path_str;
            m_known_meta_files.insert(meta_path_for(path_str));
        }
    }
}

void AssetRegistry::save_cache() {
    if (m_root_path.empty()) {
        return;
    }

    // Cache file goes in a Library folder next to Assets
    fs::path cache_dir = fs::path(m_root_path).parent_path() / "Library";
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) {
        ENGINE_LOG_WARN("AssetRegistry: Failed to create Library directory: %s", ec.message().c_str());
        return;
    }

    fs::path cache_path = cache_dir / "AssetRegistry.cache";

    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(cache_path);
    if (!file.is_open()) {
        ENGINE_LOG_WARN("AssetRegistry: Failed to save cache to '%s'", cache_path.string().c_str());
        return;
    }

    // Simple format: one line per entry: GUID|path
    for (const auto& [guid, path] : m_guid_to_path) {
        file << guid.to_string() << "|" << path << "\n";
    }

    ENGINE_LOG("AssetRegistry: Saved cache with %zu entries", m_guid_to_path.size());
}

void AssetRegistry::load_cache_and_detect_moves() {
    if (m_root_path.empty()) {
        return;
    }

    fs::path cache_path = fs::path(m_root_path).parent_path() / "Library" / "AssetRegistry.cache";

    if (!fs::exists(cache_path)) {
        ENGINE_LOG("AssetRegistry: No cache file found, skipping move detection");
        return;
    }

    // Load cached GUID -> path mapping
    std::unordered_map<AssetGUID, std::string> cached_guid_to_path;

    std::ifstream file(cache_path);
    if (!file.is_open()) {
        ENGINE_LOG_WARN("AssetRegistry: Failed to open cache file");
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Find the separator
        size_t sep = line.find('|');
        if (sep == std::string::npos) {
            continue;
        }

        std::string guid_str = line.substr(0, sep);
        std::string path = line.substr(sep + 1);

        AssetGUID guid = AssetGUID::from_string(guid_str);
        if (guid.valid()) {
            cached_guid_to_path[guid] = path;
        }
    }

    file.close();

    ENGINE_LOG("AssetRegistry: Loaded cache with %zu entries, comparing with current state...", cached_guid_to_path.size());

    // Compare cached state with current state (already populated by init())
    std::lock_guard<std::mutex> lock(m_mutex);

    int moves_detected = 0;
    int deletions_detected = 0;

    // Check for moves: GUID exists in both but with different paths
    for (const auto& [guid, current_path] : m_guid_to_path) {
        auto it = cached_guid_to_path.find(guid);
        if (it != cached_guid_to_path.end() && it->second != current_path) {
            // This GUID moved from cached path to current path
            ENGINE_LOG("AssetRegistry: Detected offline move '%s' -> '%s'",
                       it->second.c_str(), current_path.c_str());
            moves_detected++;

            if (m_moved_callback) {
                // Call outside lock would be safer, but we need to collect all moves first
                // For now, callback must not try to acquire our lock
                m_moved_callback(it->second, current_path);
            }
        }
    }

    // Check for deletions: GUID exists in cache but not in current
    for (const auto& [guid, cached_path] : cached_guid_to_path) {
        if (m_guid_to_path.find(guid) == m_guid_to_path.end()) {
            // Check if the .meta file also no longer exists (true deletion vs just missing asset)
            std::string meta = meta_path_for(cached_path);
            if (!fs::exists(meta)) {
                ENGINE_LOG("AssetRegistry: Detected offline deletion '%s'", cached_path.c_str());
                deletions_detected++;

                if (m_deleted_callback) {
                    m_deleted_callback(cached_path);
                }
            }
        }
    }

    if (moves_detected > 0 || deletions_detected > 0) {
        ENGINE_LOG("AssetRegistry: Offline changes detected - %d moves, %d deletions",
                   moves_detected, deletions_detected);
    } else {
        ENGINE_LOG("AssetRegistry: No offline changes detected");
    }
}

}