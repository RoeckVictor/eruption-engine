#include "engine/asset/AssetDatabase.h"
#include "engine/core/Log.h"

namespace engine::asset {

bool AssetDatabase::init(const std::string& base_path) {
    auto result = m_vfs.mount_directory(base_path);
    if (result.is_err()) {
        ENGINE_ERR("AssetDatabase: Failed to mount base path '%s': %s",
                   base_path.c_str(), result.error().message.c_str());
        return false;
    }
    ENGINE_LOG("AssetDatabase initialized (base: '%s')", base_path.c_str());
    return true;
}

void AssetDatabase::shutdown() {
    m_file_watcher.clear();

    // Destroy all assets in all stores
    for (auto& [type_key, store] : m_stores) {
        for (auto& slot : store.slots) {
            slot.destroy();
        }
        store.slots.clear();
        store.path_to_index.clear();
    }
    m_stores.clear();

    ENGINE_LOG("AssetDatabase shut down");
}

void AssetDatabase::poll_hot_reload() {
    m_file_watcher.poll();
}

} // namespace engine::asset
