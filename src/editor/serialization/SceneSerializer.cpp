#include "SceneSerializer.h"
#include "editor/core/EditorComponents.h"
#include "editor/core/ComponentTypeRegistry.h"
#include "engine/core/Logger.h"
#include "engine/reflection/TypeRegistry.h"
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
    json["version"] = "1.0";

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
        // Clear existing entities
        m_registry.clear();

        // Load settings
        if (json.contains("settings")) {
            const auto& settings = json["settings"];

            if (settings.contains("gravity") && settings["gravity"].is_array()) {
                auto& gravity = settings["gravity"];
                if (gravity.size() >= 2) {
                    m_settings.gravity_x = gravity[0].get<float>();
                    m_settings.gravity_y = gravity[1].get<float>();
                }
            }
            if (settings.contains("pixelsPerMeter")) {
                m_settings.pixels_per_meter = settings["pixelsPerMeter"].get<float>();
            }
            if (settings.contains("physicsSubsteps")) {
                m_settings.physics_substeps = settings["physicsSubsteps"].get<int>();
            }
            if (settings.contains("backgroundColor") && settings["backgroundColor"].is_array()) {
                auto& bg = settings["backgroundColor"];
                for (size_t i = 0; i < std::min(bg.size(), size_t(4)); ++i) {
                    m_settings.bg_color[i] = bg[i].get<float>();
                }
            }
        }

        // Validate that the reflection system is initialized
        {
            auto& type_registry = engine::reflection::TypeRegistry::instance();
            if (type_registry.get_all_types().empty()) {
                engine::Logger::instance().error("SceneSerializer",
                    "TypeRegistry is empty — init_engine_reflections() may not have been called. "
                    "Components will not be deserialized.");
            }
        }

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
    auto all_types = type_registry.get_all_types();

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
    if (m_registry.all_of<EntityInfo>(entity)) {
        const auto& info = m_registry.get<EntityInfo>(entity);
        if (!info.component_order.empty()) {
            std::vector<const engine::reflection::TypeInfo*> ordered;
            ordered.reserve(present_types.size());
            for (const auto& type_name : info.component_order) {
                for (auto it = present_types.begin(); it != present_types.end(); ++it) {
                    if ((*it)->name() == type_name) {
                        ordered.push_back(*it);
                        present_types.erase(it);
                        break;
                    }
                }
            }
            for (const auto* ti : present_types) {
                ordered.push_back(ti);
            }
            present_types = std::move(ordered);
        }
    }

    for (const auto* type_info_ptr : present_types) {
        const auto& type_info = *type_info_ptr;

        void* component_ptr = component_registry.get_component(m_registry, entity, type_info.type_index());
        if (!component_ptr) continue;

        nlohmann::json comp_json;
        comp_json["type"] = type_info.name();
        comp_json["data"] = nlohmann::json::object();

        // Serialize each property using reflection
        for (const auto& prop : type_info.properties()) {
            // Skip read-only properties
            if (engine::reflection::has_flag(prop.flags, engine::reflection::PropertyFlags::ReadOnly)) {
                continue;
            }

            // Serialize property based on its type
            void* prop_ptr = static_cast<char*>(component_ptr) + prop.offset;

            switch (prop.type) {
                case engine::reflection::PropertyType::Bool:
                    comp_json["data"][prop.name] = *reinterpret_cast<bool*>(prop_ptr);
                    break;
                case engine::reflection::PropertyType::Int:
                    comp_json["data"][prop.name] = *reinterpret_cast<int*>(prop_ptr);
                    break;
                case engine::reflection::PropertyType::Float:
                    comp_json["data"][prop.name] = *reinterpret_cast<float*>(prop_ptr);
                    break;
                case engine::reflection::PropertyType::Double:
                    comp_json["data"][prop.name] = *reinterpret_cast<double*>(prop_ptr);
                    break;
                case engine::reflection::PropertyType::String:
                    comp_json["data"][prop.name] = *reinterpret_cast<std::string*>(prop_ptr);
                    break;
                // TODO: Add Vec2, Vec3, Vec4, Color, etc. when needed
                default:
                    // Skip unknown types
                    break;
            }
        }

        json["components"].push_back(comp_json);
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
    json["version"] = "1.0";
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

            // Find TypeInfo for this component type
            const engine::reflection::TypeInfo* type_info = nullptr;
            auto all_types = type_registry.get_all_types();
            for (const auto* ti : all_types) {
                if (ti && ti->name() == type) {
                    type_info = ti;
                    break;
                }
            }

            if (!type_info) {
                // Unknown component type, skip
                continue;
            }

            // Create component dynamically using ComponentTypeRegistry
            void* component_ptr = component_registry.create_component(m_registry, entity, type_info->type_index());

            // Deserialize properties if we created the component
            if (component_ptr && type_info) {
                for (const auto& prop : type_info->properties()) {
                    // Skip read-only properties
                    if (engine::reflection::has_flag(prop.flags, engine::reflection::PropertyFlags::ReadOnly)) {
                        continue;
                    }

                    // Skip if property not in JSON
                    if (!data.contains(prop.name)) {
                        continue;
                    }

                    // Deserialize property based on type
                    void* prop_ptr = static_cast<char*>(component_ptr) + prop.offset;

                    switch (prop.type) {
                        case engine::reflection::PropertyType::Bool:
                            *reinterpret_cast<bool*>(prop_ptr) = data[prop.name].get<bool>();
                            break;
                        case engine::reflection::PropertyType::Int:
                            *reinterpret_cast<int*>(prop_ptr) = data[prop.name].get<int>();
                            break;
                        case engine::reflection::PropertyType::Float:
                            *reinterpret_cast<float*>(prop_ptr) = data[prop.name].get<float>();
                            break;
                        case engine::reflection::PropertyType::Double:
                            *reinterpret_cast<double*>(prop_ptr) = data[prop.name].get<double>();
                            break;
                        case engine::reflection::PropertyType::String:
                            *reinterpret_cast<std::string*>(prop_ptr) = data[prop.name].get<std::string>();
                            break;
                        // TODO: Add Vec2, Vec3, Vec4, Color, etc. when needed
                        default:
                            // Skip unknown types
                            break;
                    }
                }
            }
        }
    }

    // Ensure engine::Transform exists
    if (!m_registry.all_of<engine::Transform>(entity)) {
        m_registry.emplace<engine::Transform>(entity);
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

    // Serialize each property using reflection
    for (const auto& prop : type_info.properties()) {
        // Skip read-only properties
        if (engine::reflection::has_flag(prop.flags, engine::reflection::PropertyFlags::ReadOnly)) {
            continue;
        }

        // Serialize property based on its type
        void* prop_ptr = static_cast<char*>(component_ptr) + prop.offset;

        switch (prop.type) {
            case engine::reflection::PropertyType::Bool:
                json["data"][prop.name] = *reinterpret_cast<bool*>(prop_ptr);
                break;
            case engine::reflection::PropertyType::Int:
                json["data"][prop.name] = *reinterpret_cast<int*>(prop_ptr);
                break;
            case engine::reflection::PropertyType::Float:
                json["data"][prop.name] = *reinterpret_cast<float*>(prop_ptr);
                break;
            case engine::reflection::PropertyType::Double:
                json["data"][prop.name] = *reinterpret_cast<double*>(prop_ptr);
                break;
            case engine::reflection::PropertyType::String:
                json["data"][prop.name] = *reinterpret_cast<std::string*>(prop_ptr);
                break;
            default:
                // Skip unknown types
                break;
        }
    }

    return json;
}

bool SceneSerializer::deserialize_component(entt::entity entity, const nlohmann::json& json) {
    if (!json.contains("type") || !json.contains("data")) {
        return false;
    }

    std::string type_name = json["type"].get<std::string>();
    const auto& data = json["data"];

    // Get component pointer and type info
    void* component_ptr = nullptr;
    const engine::reflection::TypeInfo* type_info = nullptr;

    auto& type_registry = engine::reflection::TypeRegistry::instance();
    auto& component_registry = ComponentTypeRegistry::instance();
    auto all_types = type_registry.get_all_types();

    // Find the type info for this component
    for (const auto* ti : all_types) {
        if (ti && ti->name() == type_name) {
            type_info = ti;
            break;
        }
    }

    if (!type_info) {
        return false;  // Unknown component type
    }

    // Create or get component dynamically using ComponentTypeRegistry
    component_ptr = component_registry.create_component(m_registry, entity, type_info->type_index());

    if (!component_ptr) {
        return false;  // Failed to get/create component
    }

    // Deserialize properties
    for (const auto& prop : type_info->properties()) {
        // Skip read-only properties
        if (engine::reflection::has_flag(prop.flags, engine::reflection::PropertyFlags::ReadOnly)) {
            continue;
        }

        if (!data.contains(prop.name)) {
            continue;  // Property not in data
        }

        void* prop_ptr = static_cast<char*>(component_ptr) + prop.offset;

        switch (prop.type) {
            case engine::reflection::PropertyType::Bool:
                *reinterpret_cast<bool*>(prop_ptr) = data[prop.name].get<bool>();
                break;
            case engine::reflection::PropertyType::Int:
                *reinterpret_cast<int*>(prop_ptr) = data[prop.name].get<int>();
                break;
            case engine::reflection::PropertyType::Float:
                *reinterpret_cast<float*>(prop_ptr) = data[prop.name].get<float>();
                break;
            case engine::reflection::PropertyType::Double:
                *reinterpret_cast<double*>(prop_ptr) = data[prop.name].get<double>();
                break;
            case engine::reflection::PropertyType::String:
                *reinterpret_cast<std::string*>(prop_ptr) = data[prop.name].get<std::string>();
                break;
            default:
                // Skip unknown types
                break;
        }
    }

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
        auto all_types = type_registry.get_all_types();

        // Respect component order if available
        std::vector<const engine::reflection::TypeInfo*> present_types;
        for (const auto* ti : all_types) {
            if (!ti) continue;
            void* ptr = component_registry.get_component(m_registry, entity, ti->type_index());
            if (ptr) {
                present_types.push_back(ti);
            }
        }

        if (m_registry.all_of<EntityInfo>(entity)) {
            const auto& info = m_registry.get<EntityInfo>(entity);
            if (!info.component_order.empty()) {
                std::vector<const engine::reflection::TypeInfo*> ordered;
                ordered.reserve(present_types.size());
                for (const auto& type_name : info.component_order) {
                    for (auto it = present_types.begin(); it != present_types.end(); ++it) {
                        if ((*it)->name() == type_name) {
                            ordered.push_back(*it);
                            present_types.erase(it);
                            break;
                        }
                    }
                }
                for (const auto* ti : present_types) {
                    ordered.push_back(ti);
                }
                present_types = std::move(ordered);
            }
        }

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

} // namespace editor
