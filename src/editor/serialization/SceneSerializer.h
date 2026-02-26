#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

namespace engine::reflection {
    class TypeInfo;
}

namespace editor {

struct SceneSettings {
    float bg_color[4] = {0.1f, 0.1f, 0.15f, 1.0f};

    float reference_width = 1920.0f;
    float reference_height = 1080.0f;

    float gravity_x = 0.0f;
    float gravity_y = 980.0f;
    float pixels_per_meter = 16.0f;
    int physics_substeps = 4;
};

static constexpr int SCENE_FORMAT_VERSION = 1;

// Serializes and deserializes scenes to/from JSON files.
class SceneSerializer {
public:
    SceneSerializer(entt::registry& registry);

    bool save(const std::filesystem::path& path);
    bool load(const std::filesystem::path& path);

    std::string save_to_string();
    bool load_from_string(const std::string& data);

    nlohmann::json serialize() const;
    bool deserialize(const nlohmann::json& json);
    nlohmann::json serialize_entities(const std::vector<entt::entity>& entities) const;
    std::vector<entt::entity> deserialize_entities(const nlohmann::json& json);
    nlohmann::json serialize_component(entt::entity entity, const engine::reflection::TypeInfo& type_info, void* component_ptr) const;
    bool deserialize_component(entt::entity entity, const nlohmann::json& json);

    entt::entity load_prefab(const std::filesystem::path& path);
    bool save_prefab(const std::filesystem::path& path, entt::entity entity);
    void sync_entity_from_prefab(entt::entity target, entt::registry& source_registry, entt::entity source);

    SceneSettings& settings() { return m_settings; }
    const SceneSettings& settings() const { return m_settings; }

    const std::string& last_error() const { return m_last_error; }

private:
    nlohmann::json serialize_entity(entt::entity entity) const;
    entt::entity deserialize_entity(const nlohmann::json& json, entt::entity parent = entt::null);

    entt::registry& m_registry;
    SceneSettings m_settings;
    std::string m_last_error;
};

}
