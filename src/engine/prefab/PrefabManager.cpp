#include "engine/prefab/PrefabManager.h"
#include "engine/asset/VFS.h"
#include "engine/core/Log.h"

namespace engine::prefab {

bool PrefabManager::load_prefab(const asset::VFS& vfs, const std::string& virtual_path) {
    auto text = vfs.read_text(virtual_path);
    if (text.is_err()) {
        ENGINE_ERR("PrefabManager: Cannot read '%s': %s",
                   virtual_path.c_str(), text.error().message.c_str());
        return false;
    }

    try {
        auto root = nlohmann::json::parse(text.value());
        return parse_and_store(root, virtual_path.c_str());
    } catch (const nlohmann::json::exception& e) {
        ENGINE_ERR("PrefabManager: JSON parse error in '%s': %s", virtual_path.c_str(), e.what());
        return false;
    }
}

bool PrefabManager::load_prefab_from_string(const std::string& json_str) {
    try {
        auto root = nlohmann::json::parse(json_str);
        return parse_and_store(root, "<string>");
    } catch (const nlohmann::json::exception& e) {
        ENGINE_ERR("PrefabManager: JSON parse error: %s", e.what());
        return false;
    }
}

bool PrefabManager::parse_and_store(const nlohmann::json& root, const char* source) {
    if (!root.contains("name") || !root["name"].is_string()) {
        ENGINE_ERR("PrefabManager: Missing 'name' in '%s'", source);
        return false;
    }
    if (!root.contains("components") || !root["components"].is_array()) {
        ENGINE_ERR("PrefabManager: Missing 'components' array in '%s'", source);
        return false;
    }

    PrefabDef def;
    def.name = root["name"].get<std::string>();

    for (const auto& comp : root["components"]) {
        if (!comp.contains("type") || !comp["type"].is_string()) {
            ENGINE_ERR("PrefabManager: Component missing 'type' in '%s'", source);
            continue;
        }

        PrefabDef::ComponentEntry entry;
        entry.type_name = comp["type"].get<std::string>();
        if (comp.contains("data")) {
            entry.data = comp["data"];
        } else {
            entry.data = nlohmann::json::object();
        }
        def.components.push_back(std::move(entry));
    }

    ENGINE_LOG("PrefabManager: Loaded prefab '%s' (%zu components) from '%s'",
               def.name.c_str(), def.components.size(), source);

    m_prefabs[def.name] = std::move(def);
    return true;
}

const PrefabDef* PrefabManager::find(const std::string& name) const {
    auto it = m_prefabs.find(name);
    return (it != m_prefabs.end()) ? &it->second : nullptr;
}

entt::entity PrefabManager::instantiate(const std::string& prefab_name,
                                         entt::registry& target_registry) const {
    return instantiate(prefab_name, target_registry, nlohmann::json::object());
}

entt::entity PrefabManager::instantiate(const std::string& prefab_name,
                                         entt::registry& target_registry,
                                         const nlohmann::json& overrides) const {
    if (!m_component_registry) {
        ENGINE_ERR("PrefabManager: No ComponentRegistry set");
        return entt::null;
    }

    const auto* def = find(prefab_name);
    if (!def) {
        ENGINE_ERR("PrefabManager: Prefab '%s' not found", prefab_name.c_str());
        return entt::null;
    }

    auto entity = target_registry.create();

    for (const auto& comp : def->components) {
        const auto* factory = m_component_registry->find(comp.type_name);
        if (!factory) {
            ENGINE_ERR("PrefabManager: Unknown component type '%s' in prefab '%s'",
                       comp.type_name.c_str(), prefab_name.c_str());
            continue;
        }

        // Merge overrides: if overrides has a key matching the component type,
        // merge its data over the prefab defaults
        nlohmann::json merged_data = comp.data;
        if (overrides.contains(comp.type_name) && overrides[comp.type_name].is_object()) {
            merged_data.merge_patch(overrides[comp.type_name]);
        }

        (*factory)(target_registry, entity, merged_data);
    }

    return entity;
}

} // namespace engine::prefab
