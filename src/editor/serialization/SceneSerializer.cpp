#include "SceneSerializer.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/ComponentTypeRegistry.h"
#include "engine/core/Logger.h"
#include "engine/core/Transform.h"
#include "engine/reflection/ReflectionSerializer.h"
#include "runtime/ScriptComponent.h"
#include <fstream>

namespace editor {

SceneSerializer::SceneSerializer(entt::registry& registry)
    : m_registry(registry)
{
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
                             m_settings.bg_color[2], m_settings.bg_color[3]}}
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

        // --- Validation pass ---
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

        // --- Commit pass ---
        // Validation passed; now safe to clear and rebuild.
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

        return true;
    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while deserializing: ") + e.what();
        return false;
    }
}

nlohmann::json SceneSerializer::serialize_entity(entt::entity entity) const {
    nlohmann::json json;

    // Entity info
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

    // Components - serialize all components using reflection
    json["components"] = nlohmann::json::array();

    // Use TypeRegistry to dynamically serialize all components
    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();
    const auto& all_types = type_registry.all_types();

    // Build ordered list of present components (respecting component_order)
    std::vector<const engine::reflection::TypeInfo*> present_types;
    for (const auto* type_info_ptr : all_types) {
        if (!type_info_ptr) continue;
        void* ptr = component_registry.get_component(m_registry, entity, type_info_ptr->type_index());
        if (ptr) {
            present_types.push_back(type_info_ptr);
        }
    }

    // Apply custom component order if available
    apply_component_order(m_registry, entity, present_types);

    for (const auto* type_info_ptr : present_types) {
        const auto& type_info = *type_info_ptr;

        void* component_ptr = component_registry.get_component(m_registry, entity, type_info.type_index());
        if (!component_ptr) continue;

        nlohmann::json comp_json;
        comp_json["type"] = type_info.name();
        comp_json["data"] = nlohmann::json::object();
        engine::reflection::serialize_properties(type_info, component_ptr, comp_json["data"]);
        json["components"].push_back(comp_json);
    }

    // Scripts
    if (m_registry.all_of<runtime::ScriptComponent>(entity)) {
        const auto& sc = m_registry.get<runtime::ScriptComponent>(entity);
        if (!sc.script_types.empty()) {
            json["scripts"] = nlohmann::json::array();
            for (const auto& type_name : sc.script_types) {
                json["scripts"].push_back(type_name);
            }
        }
    }

    // Children
    if (m_registry.all_of<Hierarchy>(entity)) {
        const auto& hierarchy = m_registry.get<Hierarchy>(entity);
        if (!hierarchy.children.empty()) {
            json["children"] = nlohmann::json::array();
            for (auto child : hierarchy.children) {
                if (m_registry.valid(child)) {
                    json["children"].push_back(serialize_entity(child));
                }
            }
        }
    }

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

        // Update world transforms for new entities
        update_world_transforms(m_registry);

    } catch (const std::exception& e) {
        m_last_error = std::string("Exception while deserializing entities: ") + e.what();
    }

    return created_entities;
}

entt::entity SceneSerializer::deserialize_entity(const nlohmann::json& json, entt::entity parent) {
    // Create entity
    auto entity = m_registry.create();

    // EntityInfo
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

    // Hierarchy
    m_registry.emplace<Hierarchy>(entity);
    if (parent != entt::null) {
        set_parent(m_registry, entity, parent);
    }

    // Components - deserialize using reflection
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

            engine::reflection::deserialize_properties(*type_info, component_ptr, data);
        }
    }

    // Ensure engine::Transform exists
    if (!m_registry.all_of<engine::Transform>(entity)) {
        m_registry.emplace<engine::Transform>(entity);
    }

    // Scripts
    if (json.contains("scripts") && json["scripts"].is_array()) {
        auto& sc = m_registry.emplace<runtime::ScriptComponent>(entity);
        for (const auto& name : json["scripts"]) {
            sc.script_types.push_back(name.get<std::string>());
        }
    }

    // Children
    if (json.contains("children") && json["children"].is_array()) {
        for (const auto& child_json : json["children"]) {
            deserialize_entity(child_json, entity);
        }
    }

    return entity;
}

nlohmann::json SceneSerializer::serialize_component(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr) const {
    if (!component_ptr) return nlohmann::json();

    nlohmann::json json;
    json["type"] = type_info.name();
    json["data"] = nlohmann::json::object();
    engine::reflection::serialize_properties(type_info, component_ptr, json["data"]);
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
        return false;  // Failed to get/create component
    }

    engine::reflection::deserialize_properties(*type_info, component_ptr, data);
    return true;
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

        // Create entity with name
        std::string name = json.value("name", path.stem().string());
        auto entity = create_entity(m_registry, name);

        // Deserialize each component
        for (const auto& comp : json["components"]) {
            deserialize_component(entity, comp);
        }

        update_world_transforms(m_registry);

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

        nlohmann::json json;

        // Entity name
        if (m_registry.all_of<EntityInfo>(entity)) {
            json["name"] = m_registry.get<EntityInfo>(entity).name;
        } else {
            json["name"] = "prefab";
        }

        // Serialize components using reflection
        json["components"] = nlohmann::json::array();

        auto& type_registry = engine::reflection::TypeRegistry::instance();
        auto& component_registry = ComponentTypeRegistry::instance();
        const auto& all_types = type_registry.all_types();

        // Respect component order if available
        std::vector<const engine::reflection::TypeInfo*> present_types;
        for (const auto* ti : all_types) {
            if (!ti) continue;
            void* ptr = component_registry.get_component(m_registry, entity, ti->type_index());
            if (ptr) {
                present_types.push_back(ti);
            }
        }

        apply_component_order(m_registry, entity, present_types);

        for (const auto* ti : present_types) {
            void* ptr = component_registry.get_component(m_registry, entity, ti->type_index());
            if (ptr) {
                json["components"].push_back(serialize_component(entity, *ti, ptr));
            }
        }

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

        // Apply to target
        deserialize_component(target, comp_json);
    }

    update_world_transforms(m_registry);
}

} // namespace editor
