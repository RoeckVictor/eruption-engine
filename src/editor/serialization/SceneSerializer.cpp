#include "SceneSerializer.h"
#include "editor/core/EditorComponents.h"
#include "engine/core/Logger.h"
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
        {"gridWidth", m_settings.grid_width},
        {"gridHeight", m_settings.grid_height},
        {"gravity", {m_settings.gravity_x, m_settings.gravity_y}},
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

            if (settings.contains("gridWidth")) {
                m_settings.grid_width = settings["gridWidth"].get<int>();
            }
            if (settings.contains("gridHeight")) {
                m_settings.grid_height = settings["gridHeight"].get<int>();
            }
            if (settings.contains("gravity") && settings["gravity"].is_array()) {
                auto& gravity = settings["gravity"];
                if (gravity.size() >= 2) {
                    m_settings.gravity_x = gravity[0].get<float>();
                    m_settings.gravity_y = gravity[1].get<float>();
                }
            }
            if (settings.contains("backgroundColor") && settings["backgroundColor"].is_array()) {
                auto& bg = settings["backgroundColor"];
                for (size_t i = 0; i < std::min(bg.size(), size_t(4)); ++i) {
                    m_settings.bg_color[i] = bg[i].get<float>();
                }
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
    } else {
        json["name"] = "Entity";
        json["enabled"] = true;
    }

    // Components
    json["components"] = nlohmann::json::array();

    // Transform
    if (m_registry.all_of<Transform>(entity)) {
        const auto& transform = m_registry.get<Transform>(entity);
        json["components"].push_back({
            {"type", "Transform"},
            {"data", {
                {"x", transform.x},
                {"y", transform.y},
                {"rotation", transform.rotation},
                {"scaleX", transform.scale_x},
                {"scaleY", transform.scale_y}
            }}
        });
    }

    // TODO: Add serialization for other component types

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
    m_registry.emplace<EntityInfo>(entity, info);

    // Hierarchy
    m_registry.emplace<Hierarchy>(entity);
    if (parent != entt::null) {
        set_parent(m_registry, entity, parent);
    }

    // Components
    if (json.contains("components") && json["components"].is_array()) {
        for (const auto& comp : json["components"]) {
            std::string type = comp.value("type", "");
            const auto& data = comp.value("data", nlohmann::json::object());

            if (type == "Transform") {
                Transform transform;
                transform.x = data.value("x", 0.0f);
                transform.y = data.value("y", 0.0f);
                transform.rotation = data.value("rotation", 0.0f);
                transform.scale_x = data.value("scaleX", 1.0f);
                transform.scale_y = data.value("scaleY", 1.0f);
                m_registry.emplace<Transform>(entity, transform);
            }
            // TODO: Add deserialization for other component types
        }
    }

    // Ensure Transform exists
    if (!m_registry.all_of<Transform>(entity)) {
        m_registry.emplace<Transform>(entity);
    }

    // Children
    if (json.contains("children") && json["children"].is_array()) {
        for (const auto& child_json : json["children"]) {
            deserialize_entity(child_json, entity);
        }
    }

    return entity;
}

} // namespace editor
