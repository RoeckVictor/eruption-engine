#include "SceneSerializer.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/ComponentTypeRegistry.h"
#include "engine/core/Logger.h"
#include "engine/core/Transform.h"
#include "engine/core/TransformSystem.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/ScreenRectSystem.h"
#include "engine/reflection/ReflectionSerializer.h"
#include "runtime/ScriptComponent.h"
#include <fstream>
#include <functional>

namespace editor {

// Recursively find an entity by name among descendants.
// If multiple entities share the same name, the first match is returned and a warning is logged.
static entt::entity find_entity_by_name_recursive(entt::registry& reg, entt::entity root, const std::string& name) {
    if (!reg.valid(root)) return entt::null;

    entt::entity result = entt::null;
    int match_count = 0;

    std::function<void(entt::entity)> search = [&](entt::entity parent) {
        if (!reg.all_of<engine::Hierarchy>(parent)) return;
        const auto& hierarchy = reg.get<engine::Hierarchy>(parent);
        for (auto child : hierarchy.children) {
            if (!reg.valid(child)) continue;
            if (reg.all_of<EntityInfo>(child)) {
                if (reg.get<EntityInfo>(child).name == name) {
                    if (result == entt::null) {
                        result = child;
                    }
                    match_count++;
                }
            }
            search(child);
        }
    };

    search(root);

    if (match_count > 1) {
        engine::Logger::instance().warning("SceneSerializer",
            "Ambiguous entity reference: found %d entities named '%s' under the same hierarchy. "
            "Using the first match. Consider using unique names for entity references.",
            match_count, name.c_str());
    }

    return result;
}

SceneSerializer::SceneSerializer(entt::registry& registry)
    : m_registry(registry)
{
}

std::string SceneSerializer::resolve_entity_name(entt::entity e) const {
    if (e == entt::null || !m_registry.valid(e)) return "";
    if (m_registry.all_of<EntityInfo>(e)) {
        return m_registry.get<EntityInfo>(e).name;
    }
    return "";
}

bool SceneSerializer::save(const std::filesystem::path& path) {
    try {
        nlohmann::json json = serialize();

        std::ofstream file(path);
        if (!file.is_open()) {
            m_last_error = "Failed to open file for writing: " + path.string();
            return false;
        }

        file << json.dump(2);
        file.close();

        engine::Logger::instance().info("SceneSerializer", "Saved scene to: %s", path.string().c_str());
        return true;
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while saving: ") + e.what();
        engine::Logger::instance().error("SceneSerializer", "Failed to save scene: %s", e.what());
        return false;
    }
}

bool SceneSerializer::load(const std::filesystem::path& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            m_last_error = "Failed to open file for reading: " + path.string();
            return false;
        }

        nlohmann::json json;
        file >> json;
        file.close();

        bool result = deserialize(json);
        if (result) {
            engine::Logger::instance().info("SceneSerializer", "Loaded scene from: %s", path.string().c_str());
        }
        return result;
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while loading: ") + e.what();
        engine::Logger::instance().error("SceneSerializer", "Failed to load scene: %s", e.what());
        return false;
    }
}

std::string SceneSerializer::save_to_string() {
    try {
        nlohmann::json json = serialize();
        return json.dump();
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while serializing to string: ") + e.what();
        return "";
    }
}

bool SceneSerializer::load_from_string(const std::string& data) {
    try {
        nlohmann::json json = nlohmann::json::parse(data);
        return deserialize(json);
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while parsing string: ") + e.what();
        return false;
    }
}

nlohmann::json SceneSerializer::serialize() const {
    nlohmann::json json;

    // Scene metadata
    json["version"] = SCENE_FORMAT_VERSION;

    // Scene settings
    json["settings"] = {
        {"gravity", {m_settings.gravity_x, m_settings.gravity_y}},
        {"pixelsPerMeter", m_settings.pixels_per_meter},
        {"physicsSubsteps", m_settings.physics_substeps},
        {"backgroundColor", {m_settings.bg_color[0], m_settings.bg_color[1],
                             m_settings.bg_color[2], m_settings.bg_color[3]}},
        {"referenceResolution", {m_settings.reference_width, m_settings.reference_height}}
    };

    // Get root entities
    auto roots = get_root_entities(const_cast<entt::registry&>(m_registry));

    // Serialize entities
    json["entities"] = nlohmann::json::array();
    for (auto entity : roots) {
        json["entities"].push_back(serialize_entity(entity));
    }

    return json;
}

bool SceneSerializer::deserialize(const nlohmann::json& json) {
    try {
        // Check scene format version
        if (json.contains("version")) {
            auto& ver = json["version"];
            int file_version = 0;
            if (ver.is_number_integer()) {
                file_version = ver.get<int>();
            } else if (ver.is_string()) {
                // Handle legacy string versions like "1.0"
                file_version = static_cast<int>(std::stof(ver.get<std::string>()));
            }
            if (file_version > SCENE_FORMAT_VERSION) {
                engine::Logger::instance().warning("SceneSerializer",
                    "Scene file version %d is newer than supported version %d; "
                    "some data may not load correctly",
                    file_version, SCENE_FORMAT_VERSION);
            }
        } else {
            engine::Logger::instance().warning("SceneSerializer",
                "Scene file has no version field; assuming version %d", SCENE_FORMAT_VERSION);
        }

        // Validate that the reflection system is initialized
        {
            auto& type_registry = engine::reflection::TypeRegistry::instance();
            if (type_registry.all_types().empty()) {
                m_last_error = "TypeRegistry is empty — init_engine_reflections() may not have been called";
                engine::Logger::instance().error("SceneSerializer", "%s", m_last_error.c_str());
                return false;
            }
        }

        // Validation pass
        // Parse settings and walk entity JSON to catch structural errors
        // BEFORE clearing the registry. This prevents data loss on corrupt files.
        SceneSettings new_settings = m_settings;
        if (json.contains("settings")) {
            const auto& settings = json["settings"];

            if (settings.contains("gravity") && settings["gravity"].is_array()) {
                auto& gravity = settings["gravity"];
                if (gravity.size() >= 2) {
                    new_settings.gravity_x = gravity[0].get<float>();
                    new_settings.gravity_y = gravity[1].get<float>();
                }
            }
            if (settings.contains("pixelsPerMeter")) {
                new_settings.pixels_per_meter = settings["pixelsPerMeter"].get<float>();
            }
            if (settings.contains("physicsSubsteps")) {
                new_settings.physics_substeps = settings["physicsSubsteps"].get<int>();
            }
            if (settings.contains("backgroundColor") && settings["backgroundColor"].is_array()) {
                auto& bg = settings["backgroundColor"];
                for (size_t i = 0; i < std::min(bg.size(), size_t(4)); ++i) {
                    new_settings.bg_color[i] = bg[i].get<float>();
                }
            }
            if (settings.contains("referenceResolution") && settings["referenceResolution"].is_array()) {
                auto& res = settings["referenceResolution"];
                if (res.size() >= 2) {
                    new_settings.reference_width = res[0].get<float>();
                    new_settings.reference_height = res[1].get<float>();
                }
            }
        }

        // Validate entity array is structurally sound before committing
        if (json.contains("entities")) {
            if (!json["entities"].is_array()) {
                m_last_error = "Scene 'entities' field is not an array";
                engine::Logger::instance().error("SceneSerializer", "%s", m_last_error.c_str());
                return false;
            }
            // Walk each entity to ensure basic JSON access won't throw
            for (const auto& entity_json : json["entities"]) {
                if (!entity_json.is_object()) {
                    m_last_error = "Scene contains a non-object entity entry";
                    engine::Logger::instance().error("SceneSerializer", "%s", m_last_error.c_str());
                    return false;
                }
            }
        }

        // Commit pass
        // Validation passed; now safe to clear and rebuild
        m_registry.clear();
        m_settings = new_settings;

        // Load entities
        if (json.contains("entities") && json["entities"].is_array()) {
            for (const auto& entity_json : json["entities"]) {
                deserialize_entity(entity_json, entt::null);
            }
        }

        // Update world transforms
        update_world_transforms(m_registry);

        // Update screen-space entity positions
        engine::ScreenRectSystem::update(m_registry, m_settings.reference_width, m_settings.reference_height);

        return true;
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while deserializing: ") + e.what();
        return false;
    }
}

void SceneSerializer::serialize_components(entt::entity entity, nlohmann::json& json) const {
    json["components"] = nlohmann::json::array();

    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();
    const auto& all_types = type_registry.all_types();

    std::vector<const engine::reflection::TypeInfo*> present_types;
    for (const auto* type_info_ptr : all_types) {
        if (!type_info_ptr) continue;
        void* ptr = component_registry.get_component(m_registry, entity, type_info_ptr->type_index());
        if (ptr) {
            present_types.push_back(type_info_ptr);
        }
    }

    apply_component_order(m_registry, entity, present_types);

    for (const auto* ti : present_types) {
        void* ptr = component_registry.get_component(m_registry, entity, ti->type_index());
        if (ptr) {
            json["components"].push_back(serialize_component(entity, *ti, ptr));
        }
    }
}

void SceneSerializer::serialize_scripts(entt::entity entity, nlohmann::json& json) const {
    if (!m_registry.all_of<runtime::ScriptComponent>(entity)) return;

    const auto& sc = m_registry.get<runtime::ScriptComponent>(entity);
    if (sc.script_types.empty()) return;

    json["scripts"] = nlohmann::json::array();
    for (size_t i = 0; i < sc.script_types.size(); ++i) {
        nlohmann::json script_json;
        script_json["type"] = sc.script_types[i];

        if (i < sc.scripts.size() && sc.scripts[i]) {
            nlohmann::json props;
            sc.scripts[i]->serialize_properties(props);
            script_json["properties"] = props;
        } else if (i < sc.script_properties.size()) {
            script_json["properties"] = sc.script_properties[i];
        } else {
            script_json["properties"] = nlohmann::json::object();
        }

        json["scripts"].push_back(script_json);
    }
}

void SceneSerializer::serialize_children(entt::entity entity, nlohmann::json& json, bool is_prefab) const {
    if (!m_registry.all_of<Hierarchy>(entity)) return;

    const auto& hierarchy = m_registry.get<Hierarchy>(entity);
    if (hierarchy.children.empty()) return;

    json["children"] = nlohmann::json::array();
    for (auto child : hierarchy.children) {
        if (m_registry.valid(child)) {
            json["children"].push_back(
                is_prefab ? serialize_prefab_entity(child) : serialize_entity(child));
        }
    }
}

void SceneSerializer::deserialize_scripts(entt::entity entity, const nlohmann::json& json) {
    if (!json.contains("scripts") || !json["scripts"].is_array()) return;

    auto& sc = m_registry.emplace<runtime::ScriptComponent>(entity);
    for (const auto& script_json : json["scripts"]) {
        if (!script_json.is_object()) continue;

        std::string type_name = script_json.value("type", "");
        nlohmann::json properties = script_json.value("properties", nlohmann::json::object());

        if (!type_name.empty()) {
            sc.script_types.push_back(type_name);
            sc.script_properties.push_back(properties);
        }
    }
}

void SceneSerializer::deserialize_children(entt::entity entity, const nlohmann::json& json, bool is_prefab) {
    if (!json.contains("children") || !json["children"].is_array()) return;

    for (const auto& child_json : json["children"]) {
        if (is_prefab) {
            deserialize_prefab_entity(child_json, entity);
        } else {
            deserialize_entity(child_json, entity);
        }
    }
}

void SceneSerializer::resolve_deferred_refs(entt::entity entity, entt::entity parent,
                                             std::vector<engine::reflection::DeferredEntityRef>& deferred_refs) {
    if (deferred_refs.empty()) return;

    auto& component_registry = ComponentTypeRegistry::instance();
    for (auto& ref : deferred_refs) {
        entt::entity resolved = find_entity_by_name_recursive(m_registry, entity, ref.entity_name);

        if (resolved == entt::null && parent != entt::null) {
            resolved = find_entity_by_name_recursive(m_registry, parent, ref.entity_name);
        }

        void* comp_ptr = component_registry.get_component(m_registry, entity, ref.component_type);
        if (comp_ptr) {
            auto* target = reinterpret_cast<entt::entity*>(static_cast<char*>(comp_ptr) + ref.field_offset);
            *target = resolved;
        }
    }
}

nlohmann::json SceneSerializer::serialize_entity(entt::entity entity) const {
    nlohmann::json json;

    // Entity info (scene-specific metadata: guid, enabled, prefab link, component order)
    if (m_registry.all_of<EntityInfo>(entity)) {
        const auto& info = m_registry.get<EntityInfo>(entity);
        json["name"] = info.name;
        json["guid"] = info.guid;
        json["enabled"] = info.enabled;
        if (!info.prefab_path.empty()) {
            json["prefab"] = info.prefab_path;
            json["isPrefabInstance"] = info.is_prefab_instance;
        }
        if (!info.component_order.empty()) {
            json["componentOrder"] = info.component_order;
        }
    } else {
        json["name"] = "Entity";
        json["enabled"] = true;
    }

    serialize_components(entity, json);
    serialize_scripts(entity, json);
    serialize_children(entity, json, false);

    return json;
}

nlohmann::json SceneSerializer::serialize_entities(const std::vector<entt::entity>& entities) const {
    nlohmann::json json;
    json["type"] = "clipboard";
    json["version"] = SCENE_FORMAT_VERSION;
    json["entities"] = nlohmann::json::array();

    for (auto entity : entities) {
        if (m_registry.valid(entity)) {
            json["entities"].push_back(serialize_entity(entity));
        }
    }

    return json;
}

std::vector<entt::entity> SceneSerializer::deserialize_entities(const nlohmann::json& json) {
    std::vector<entt::entity> created_entities;

    try {
        if (!json.contains("entities") || !json["entities"].is_array()) {
            return created_entities;
        }

        for (const auto& entity_json : json["entities"]) {
            auto entity = deserialize_entity(entity_json, entt::null);
            if (entity != entt::null) {
                created_entities.push_back(entity);
            }
        }

        update_world_transforms(m_registry);

        engine::ScreenRectSystem::update(m_registry, m_settings.reference_width, m_settings.reference_height);

    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while deserializing entities: ") + e.what();
    }

    return created_entities;
}

entt::entity SceneSerializer::deserialize_entity(const nlohmann::json& json, entt::entity parent) {
    auto entity = m_registry.create();

    EntityInfo info;
    if (json.contains("name")) {
        info.name = json["name"].get<std::string>();
    }
    if (json.contains("guid")) {
        info.guid = json["guid"].get<std::string>();
    }
    if (json.contains("enabled")) {
        info.enabled = json["enabled"].get<bool>();
    }
    if (json.contains("prefab")) {
        info.prefab_path = json["prefab"].get<std::string>();
    }
    if (json.contains("isPrefabInstance")) {
        info.is_prefab_instance = json["isPrefabInstance"].get<bool>();
    }
    // Auto-unpack if prefab source file is missing
    if (info.is_prefab_instance && !info.prefab_path.empty()) {
        if (!std::filesystem::exists(info.prefab_path)) {
            engine::Logger::instance().warning("SceneSerializer",
                "Prefab source '%s' not found for entity '%s', unpacking instance",
                info.prefab_path.c_str(), info.name.c_str());
            info.is_prefab_instance = false;
            info.prefab_path.clear();
        }
    }
    if (json.contains("componentOrder") && json["componentOrder"].is_array()) {
        for (const auto& name : json["componentOrder"]) {
            info.component_order.push_back(name.get<std::string>());
        }
    }
    m_registry.emplace<EntityInfo>(entity, info);

    m_registry.emplace<Hierarchy>(entity);
    // NOTE: set_parent is called AFTER components are deserialized (below)
    // because set_parent validates screen-space vs world-space matching,
    // which requires ScreenRect to be present on the entity first.

    // Components - deserialize using reflection
    // Collect deferred entity refs for later resolution
    std::vector<engine::reflection::DeferredEntityRef> deferred_refs;

    if (json.contains("components") && json["components"].is_array()) {
        auto& type_registry = engine::reflection::TypeRegistry::instance();
        auto& component_registry = ComponentTypeRegistry::instance();

        for (const auto& comp : json["components"]) {
            std::string type = comp.value("type", "");
            const auto& data = comp.value("data", nlohmann::json::object());

            const auto* type_info = type_registry.get_by_name(type);
            if (!type_info) {
                if (!type.empty()) {
                    engine::Logger::instance().warning("SceneSerializer",
                        "Unknown component type '%s' on entity '%s', skipping",
                        type.c_str(), info.name.c_str());
                }
                continue;
            }

            // Create component dynamically using ComponentTypeRegistry
            void* component_ptr = component_registry.create_component(m_registry, entity, type_info->type_index());

            if (!component_ptr) {
                engine::Logger::instance().error("SceneSerializer",
                    "Failed to create component '%s' on entity '%s' — type may not be registered in ComponentTypeRegistry",
                    type.c_str(), info.name.c_str());
                continue;
            }

            engine::reflection::deserialize_properties_with_deferred(*type_info, component_ptr, data, &deferred_refs);
        }
    }

    // Ensure world-space and screen-space entities are mutually exclusive:
    // - World-space entities have Transform (no ScreenRect)
    // - Screen-space entities have ScreenRect (no Transform)
    if (m_registry.all_of<engine::ScreenRect>(entity)) {
        // Screen-space entity: remove Transform if it was loaded from an old scene
        if (m_registry.all_of<engine::Transform>(entity)) {
            m_registry.remove<engine::Transform>(entity);
        }
    } else if (!m_registry.all_of<engine::Transform>(entity)) {
        // World-space entity without Transform: add it
        m_registry.emplace<engine::Transform>(entity);
    }

    // Set parent relationship NOW that components are loaded.
    // This must happen after ScreenRect is added so screen-space validation works.
    // Use the engine's set_parent directly (not the editor's world-preserving version)
    // because we want to keep the local transforms as specified in the serialized data,
    // not recompute them to preserve world transforms (which are at defaults after deserialization).
    if (parent != entt::null) {
        engine::TransformSystem::set_parent(m_registry, entity, parent);
    }

    deserialize_scripts(entity, json);
    deserialize_children(entity, json, false);
    resolve_deferred_refs(entity, parent, deferred_refs);

    return entity;
}

nlohmann::json SceneSerializer::serialize_component(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr) const {
    (void)entity;  // May be used for context in future
    if (!component_ptr) return nlohmann::json();

    nlohmann::json json;
    json["type"] = type_info.name();
    json["data"] = nlohmann::json::object();

    auto name_resolver = [this](entt::entity e) -> std::string { return resolve_entity_name(e); };
    engine::reflection::serialize_properties_with_context(type_info, component_ptr, json["data"], name_resolver);
    return json;
}

bool SceneSerializer::deserialize_component(entt::entity entity, const nlohmann::json& json) {
    if (!json.contains("type") || !json.contains("data")) {
        return false;
    }

    std::string type_name = json["type"].get<std::string>();
    const auto& data = json["data"];

    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();

    const auto* type_info = type_registry.get_by_name(type_name);
    if (!type_info) return false;

    void* component_ptr = component_registry.create_component(m_registry, entity, type_info->type_index());

    if (!component_ptr) {
        return false;
    }

    engine::reflection::deserialize_properties(*type_info, component_ptr, data);
    return true;
}

nlohmann::json SceneSerializer::serialize_prefab_entity(entt::entity entity) const {
    nlohmann::json json;

    // Prefab only stores name (no guid, enabled, prefab link, component order)
    if (m_registry.all_of<EntityInfo>(entity)) {
        json["name"] = m_registry.get<EntityInfo>(entity).name;
    } else {
        json["name"] = "prefab";
    }

    serialize_components(entity, json);
    serialize_scripts(entity, json);
    serialize_children(entity, json, true);

    return json;
}

entt::entity SceneSerializer::deserialize_prefab_entity(const nlohmann::json& json, entt::entity parent) {
    if (!json.contains("components") || !json["components"].is_array()) {
        return entt::null;
    }

    // Detect if this is a screen-space entity (has ScreenRect component)
    bool is_screen_entity = false;
    for (const auto& comp : json["components"]) {
        if (comp.value("type", "") == "engine::ScreenRect") {
            is_screen_entity = true;
            break;
        }
    }

    // Create entity with name (screen or world space based on components)
    std::string name = json.value("name", "prefab");
    auto entity = is_screen_entity
        ? create_screen_entity(m_registry, name)
        : create_entity(m_registry, name);

    // Deserialize components with deferred entity ref support
    std::vector<engine::reflection::DeferredEntityRef> deferred_refs;
    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();

    for (const auto& comp : json["components"]) {
        std::string type = comp.value("type", "");
        const auto& data = comp.value("data", nlohmann::json::object());

        const auto* type_info = type_registry.get_by_name(type);
        if (!type_info) continue;

        void* component_ptr = component_registry.create_component(m_registry, entity, type_info->type_index());
        if (!component_ptr) continue;

        engine::reflection::deserialize_properties_with_deferred(*type_info, component_ptr, data, &deferred_refs);
    }

    // Set parent after components are loaded (ScreenRect must exist for space validation)
    if (parent != entt::null) {
        engine::TransformSystem::set_parent(m_registry, entity, parent);
    }

    deserialize_scripts(entity, json);
    deserialize_children(entity, json, true);
    resolve_deferred_refs(entity, parent, deferred_refs);

    return entity;
}

entt::entity SceneSerializer::load_prefab(const std::filesystem::path& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            m_last_error = "Failed to open prefab file: " + path.string();
            return entt::null;
        }

        nlohmann::json json;
        file >> json;
        file.close();

        // Require the new component-based format
        if (!json.contains("components") || !json["components"].is_array()) {
            m_last_error = "Prefab file missing 'components' array (legacy format not supported): " + path.string();
            engine::Logger::instance().warning("SceneSerializer", "%s", m_last_error.c_str());
            return entt::null;
        }

        // Deserialize entity hierarchy recursively
        auto entity = deserialize_prefab_entity(json, entt::null);

        // Mark the root entity as a prefab instance and store the source path
        if (entity != entt::null && m_registry.all_of<EntityInfo>(entity)) {
            auto& info = m_registry.get<EntityInfo>(entity);
            info.prefab_path = path.string();
            info.is_prefab_instance = true;
        }

        update_world_transforms(m_registry);
        engine::ScreenRectSystem::update(m_registry, m_settings.reference_width, m_settings.reference_height);

        engine::Logger::instance().info("SceneSerializer", "Loaded prefab: %s", path.string().c_str());
        return entity;
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception loading prefab: ") + e.what();
        engine::Logger::instance().error("SceneSerializer", "%s", m_last_error.c_str());
        return entt::null;
    }
}

bool SceneSerializer::save_prefab(const std::filesystem::path& path, entt::entity entity) {
    try {
        if (!m_registry.valid(entity)) {
            m_last_error = "Invalid entity for prefab save";
            return false;
        }

        // Serialize entity with all children recursively
        nlohmann::json json = serialize_prefab_entity(entity);

        std::ofstream file(path);
        if (!file.is_open()) {
            m_last_error = "Failed to open file for writing: " + path.string();
            return false;
        }

        file << json.dump(2);
        file.close();

        engine::Logger::instance().info("SceneSerializer", "Saved prefab: %s", path.string().c_str());
        return true;
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception saving prefab: ") + e.what();
        engine::Logger::instance().error("SceneSerializer", "%s", m_last_error.c_str());
        return false;
    }
}

void SceneSerializer::sync_entity_from_prefab(entt::entity target, entt::registry& source_registry, entt::entity source) {
    if (!m_registry.valid(target) || !source_registry.valid(source)) return;

    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();
    const auto& all_types = type_registry.all_types();

    // Get all components from source entity
    for (const auto* type_info : all_types) {
        if (!type_info) continue;

        // Skip EntityInfo - we want to preserve the instance's identity
        if (type_info->name() == "editor::EntityInfo") continue;

        // Handle Transform specially - sync rotation and scale, but preserve position
        if (type_info->name() == "engine::Transform") {
            if (source_registry.all_of<engine::Transform>(source) && m_registry.all_of<engine::Transform>(target)) {
                const auto& src_t = source_registry.get<engine::Transform>(source);
                auto& dst_t = m_registry.get<engine::Transform>(target);

                // Preserve instance position, sync rotation and scale from prefab
                dst_t.rotation = src_t.rotation;
                dst_t.scale_x = src_t.scale_x;
                dst_t.scale_y = src_t.scale_y;
            }
            continue;
        }

        // Check if source has this component
        void* source_ptr = component_registry.get_component(source_registry, source, type_info->type_index());
        if (!source_ptr) {
            // Source doesn't have this component - remove from target if it exists
            component_registry.remove_component(m_registry, target, type_info->type_index());
            continue;
        }

        // Serialize source component to JSON
        nlohmann::json comp_json;
        comp_json["type"] = type_info->name();
        comp_json["data"] = nlohmann::json::object();
        engine::reflection::serialize_properties(*type_info, source_ptr, comp_json["data"]);

        deserialize_component(target, comp_json);
    }

    update_world_transforms(m_registry);
    engine::ScreenRectSystem::update(m_registry, m_settings.reference_width, m_settings.reference_height);
}

bool SceneSerializer::is_screen_prefab(const std::filesystem::path& prefab_path) {
    std::ifstream file(prefab_path);
    if (!file.is_open()) return false;

    nlohmann::json json;
    try {
        file >> json;
    } catch (...) {
        return false;
    }

    // Check root entity components for ScreenRect
    if (json.contains("components")) {
        for (const auto& comp : json["components"]) {
            if (comp.contains("type") && comp["type"] == "engine::ScreenRect") {
                return true;
            }
        }
    }
    return false;
}

}
