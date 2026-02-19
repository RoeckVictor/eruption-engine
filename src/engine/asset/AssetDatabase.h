#pragma once

#include "engine/asset/AssetHandle.h"
#include "engine/asset/AssetLoader.h"
#include "engine/asset/VFS.h"
#include "engine/asset/FileWatcher.h"
#include "engine/core/Log.h"
#include <any>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace engine::asset {

/// Central asset manager. Owns all loaded assets, provides handle-based access,
/// deduplication by virtual path, and hot-reload polling.
class AssetDatabase {
public:
    bool init(const std::string& base_path = ".");
    void shutdown();

    VFS& vfs() { return m_vfs; }
    const VFS& vfs() const { return m_vfs; }

    /// Load an asset of type T from a VFS path. Returns a cached handle if
    /// already loaded. The Handle is stable across hot-reloads.
    template<typename T>
    Handle<T> load(const std::string& virtual_path);

    /// Get a raw pointer to the asset. Returns nullptr if handle is invalid
    /// or has been invalidated by generational mismatch.
    template<typename T>
    T* get(Handle<T> handle) const;

    /// Force-reload a specific asset.
    template<typename T>
    bool reload(Handle<T> handle);

    /// Poll all tracked assets for file changes and reload as needed.
    /// Called once per frame by the Engine main loop.
    void poll_hot_reload();

    /// Unload a specific asset by handle. The handle becomes invalid after this.
    /// Returns true if the asset was unloaded, false if handle was invalid.
    template<typename T>
    bool unload(Handle<T> handle);

    /// Unload an asset by virtual path.
    /// Returns true if an asset was found and unloaded.
    template<typename T>
    bool unload(const std::string& virtual_path);

    /// Check if an asset is currently loaded at the given path.
    template<typename T>
    bool is_loaded(const std::string& virtual_path) const;

    /// Get the virtual path for a loaded asset handle.
    /// Returns empty string if handle is invalid.
    template<typename T>
    std::string get_path(Handle<T> handle) const;

private:
    VFS m_vfs;
    FileWatcher m_file_watcher;

    // Type-safe per-slot storage using std::any
    struct AssetSlot {
        std::any data;  // Type-safe type erasure (handles destruction automatically)
        AssetMeta meta;
        bool occupied = false;

        ~AssetSlot() { destroy(); }
        AssetSlot() = default;
        AssetSlot(const AssetSlot&) = delete;
        AssetSlot& operator=(const AssetSlot&) = delete;
        AssetSlot(AssetSlot&& o) noexcept
            : data(std::move(o.data)), meta(std::move(o.meta)), occupied(o.occupied) {
            o.occupied = false;
        }
        AssetSlot& operator=(AssetSlot&& o) noexcept {
            if (this != &o) {
                destroy();
                data = std::move(o.data);
                meta = std::move(o.meta);
                occupied = o.occupied;
                o.occupied = false;
            }
            return *this;
        }
        void destroy() {
            data.reset();
            occupied = false;
        }
    };

    struct TypeStore {
        std::vector<AssetSlot> slots;
        std::unordered_map<std::string, uint32_t> path_to_index;
    };

    std::unordered_map<std::type_index, TypeStore> m_stores;

    template<typename T>
    TypeStore& get_or_create_store() {
        auto key = std::type_index(typeid(T));
        return m_stores[key];
    }

    template<typename T>
    const TypeStore* find_store() const {
        auto key = std::type_index(typeid(T));
        auto it = m_stores.find(key);
        return (it != m_stores.end()) ? &it->second : nullptr;
    }
};

// ---- Template implementations ----

template<typename T>
Handle<T> AssetDatabase::load(const std::string& virtual_path) {
    auto& store = get_or_create_store<T>();

    // Check dedup cache
    auto it = store.path_to_index.find(virtual_path);
    if (it != store.path_to_index.end()) {
        auto& slot = store.slots[it->second];
        if (slot.occupied) {
            return Handle<T>{it->second, slot.meta.generation};
        }
    }

    // Load new asset
    auto asset = AssetLoader<T>::load(m_vfs, virtual_path);
    if (!asset) {
        ENGINE_ERR("AssetDatabase: Failed to load '%s'", virtual_path.c_str());
        return Handle<T>{};
    }

    // Find or create a slot
    uint32_t index = static_cast<uint32_t>(store.slots.size());
    store.slots.emplace_back();

    auto& slot = store.slots[index];
    // Store as std::shared_ptr<T> in std::any for type-safe storage and automatic cleanup
    // (std::any requires copy-constructible types, so we convert unique_ptr to shared_ptr)
    slot.data = std::shared_ptr<T>(std::move(asset));
    slot.meta.virtual_path = virtual_path;
    slot.meta.generation = 1;
    slot.occupied = true;

    // Resolve physical path for hot-reload
    auto physical = m_vfs.resolve(virtual_path);
    if (physical.is_ok()) {
        slot.meta.physical_path = physical.value();
        std::error_code ec;
        slot.meta.last_modified = std::filesystem::last_write_time(physical.value(), ec);

        // Set up file watching for hot-reload
        // NOTE: Capture virtual_path instead of index to avoid stale index if slots vector reallocates
        m_file_watcher.watch(physical.value(), [this, vpath = virtual_path, key = std::type_index(typeid(T))](const std::string&) {
            auto store_it = m_stores.find(key);
            if (store_it == m_stores.end()) return;

            // Look up current index from path (safe even after reallocation)
            auto path_it = store_it->second.path_to_index.find(vpath);
            if (path_it == store_it->second.path_to_index.end()) return;

            auto& s = store_it->second.slots[path_it->second];
            if (!s.occupied || !s.data.has_value()) return;

            // Type-safe hot-reload using std::any_cast
            try {
                auto* ptr = std::any_cast<std::shared_ptr<T>>(&s.data);
                if (!ptr || !*ptr) return;

                ENGINE_LOG("AssetDatabase: Hot-reloading '%s'", vpath.c_str());
                bool ok = AssetLoader<T>::reload(**ptr, m_vfs, vpath);
                if (ok) {
                    s.meta.generation++;
                    ENGINE_LOG("AssetDatabase: Reloaded '%s' (gen %u)", vpath.c_str(), s.meta.generation);
                } else {
                    ENGINE_ERR("AssetDatabase: Failed to reload '%s'", vpath.c_str());
                }
            } catch (const std::bad_any_cast& e) {
                ENGINE_ERR("AssetDatabase: Type mismatch during hot-reload for '%s': %s",
                           vpath.c_str(), e.what());
            }
        });
    }

    store.path_to_index[virtual_path] = index;

    ENGINE_LOG("AssetDatabase: Loaded '%s' [slot %u]", virtual_path.c_str(), index);
    return Handle<T>{index, slot.meta.generation};
}

template<typename T>
T* AssetDatabase::get(Handle<T> handle) const {
    if (!handle.valid()) return nullptr;

    const auto* store = find_store<T>();
    if (!store) return nullptr;

    if (handle.index >= store->slots.size()) return nullptr;

    const auto& slot = store->slots[handle.index];
    if (!slot.occupied) return nullptr;

    // Generation check: handles remain valid even if generation advances (hot-reload).
    // A handle from gen 1 can still access the asset after it becomes gen 2.
    // The generation in the handle just reflects when it was acquired.

    // Type-safe extraction using std::any_cast
    try {
        const auto* ptr = std::any_cast<std::shared_ptr<T>>(&slot.data);
        if (ptr && *ptr) {
            return ptr->get();
        }
    } catch (const std::bad_any_cast& e) {
        ENGINE_ERR("AssetDatabase: Type mismatch for handle index %u: %s",
                   handle.index, e.what());
    }

    return nullptr;
}

template<typename T>
bool AssetDatabase::reload(Handle<T> handle) {
    if (!handle.valid()) return false;

    auto& store = get_or_create_store<T>();
    if (handle.index >= store.slots.size()) return false;

    auto& slot = store.slots[handle.index];
    if (!slot.occupied || !slot.data.has_value()) return false;

    // Type-safe extraction for reload
    try {
        auto* ptr = std::any_cast<std::shared_ptr<T>>(&slot.data);
        if (!ptr || !*ptr) return false;

        bool ok = AssetLoader<T>::reload(**ptr, m_vfs, slot.meta.virtual_path);
        if (ok) {
            slot.meta.generation++;
        }
        return ok;
    } catch (const std::bad_any_cast& e) {
        ENGINE_ERR("AssetDatabase: Type mismatch during reload for '%s': %s",
                   slot.meta.virtual_path.c_str(), e.what());
        return false;
    }
}

template<typename T>
bool AssetDatabase::unload(Handle<T> handle) {
    if (!handle.valid()) return false;

    auto key = std::type_index(typeid(T));
    auto store_it = m_stores.find(key);
    if (store_it == m_stores.end()) return false;

    auto& store = store_it->second;
    if (handle.index >= store.slots.size()) return false;

    auto& slot = store.slots[handle.index];
    if (!slot.occupied) return false;

    // Stop watching for hot-reload
    if (!slot.meta.physical_path.empty()) {
        m_file_watcher.unwatch(slot.meta.physical_path);
    }

    // Remove from path lookup
    auto path_it = store.path_to_index.find(slot.meta.virtual_path);
    if (path_it != store.path_to_index.end() && path_it->second == handle.index) {
        store.path_to_index.erase(path_it);
    }

    std::string path = slot.meta.virtual_path;
    slot.destroy();
    // Increment generation so any remaining handles become stale
    slot.meta.generation++;

    ENGINE_LOG("AssetDatabase: Unloaded '%s' [slot %u]", path.c_str(), handle.index);
    return true;
}

template<typename T>
bool AssetDatabase::unload(const std::string& virtual_path) {
    auto key = std::type_index(typeid(T));
    auto store_it = m_stores.find(key);
    if (store_it == m_stores.end()) return false;

    auto& store = store_it->second;
    auto it = store.path_to_index.find(virtual_path);
    if (it == store.path_to_index.end()) return false;

    return unload<T>(Handle<T>{it->second, store.slots[it->second].meta.generation});
}

template<typename T>
bool AssetDatabase::is_loaded(const std::string& virtual_path) const {
    const auto* store = find_store<T>();
    if (!store) return false;

    auto it = store->path_to_index.find(virtual_path);
    if (it == store->path_to_index.end()) return false;

    return store->slots[it->second].occupied;
}

template<typename T>
std::string AssetDatabase::get_path(Handle<T> handle) const {
    if (!handle.valid()) return "";

    const auto* store = find_store<T>();
    if (!store) return "";

    if (handle.index >= store->slots.size()) return "";

    const auto& slot = store->slots[handle.index];
    if (!slot.occupied) return "";

    return slot.meta.virtual_path;
}

} // namespace engine::asset
