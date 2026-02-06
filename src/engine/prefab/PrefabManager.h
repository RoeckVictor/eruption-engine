#pragma once

#include "engine/prefab/ComponentRegistry.h"
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::asset { class VFS; }

namespace engine::prefab {

/// A parsed prefab definition: a list of component entries.
struct PrefabDef {
    std::string name;
    struct ComponentEntry {
        std::string type_name;
        nlohmann::json data;
    };
    std::vector<ComponentEntry> components;
};

/// Loads prefab definitions from JSON files and instantiates entities.
class PrefabManager {
public:
    void set_registry(ComponentRegistry& registry) { m_component_registry = &registry; }

    /// Load a prefab definition from a VFS path.
    /// JSON format:
    /// {
    ///   "name": "player",
    ///   "components": [
    ///     { "type": "Transform", "data": { "x": 0, "y": 0 } },
    ///     { "type": "Velocity" },
    ///     { "type": "PlayerTag" }
    ///   ]
    /// }
    bool load_prefab(const asset::VFS& vfs, const std::string& virtual_path);

    /// Load a prefab from a JSON string.
    bool load_prefab_from_string(const std::string& json_str);

    /// Get a loaded prefab definition by name.
    const PrefabDef* find(const std::string& name) const;

    /// Instantiate a prefab into a registry.
    /// Returns entt::null if the prefab is not found.
    entt::entity instantiate(const std::string& prefab_name,
                             entt::registry& target_registry) const;

    /// Instantiate with JSON overrides (merged over prefab defaults).
    entt::entity instantiate(const std::string& prefab_name,
                             entt::registry& target_registry,
                             const nlohmann::json& overrides) const;

    size_t prefab_count() const { return m_prefabs.size(); }

private:
    ComponentRegistry* m_component_registry = nullptr;
    std::unordered_map<std::string, PrefabDef> m_prefabs;

    bool parse_and_store(const nlohmann::json& root, const char* source);
};

} // namespace engine::prefab
