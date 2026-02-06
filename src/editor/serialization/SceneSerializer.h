#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

namespace editor {

struct SceneSettings {
    int grid_width = 512;
    int grid_height = 512;
    float gravity_x = 0.0f;
    float gravity_y = -9.81f;
    float bg_color[4] = {0.1f, 0.1f, 0.15f, 1.0f};
};

/// Serializes and deserializes scenes to/from JSON files.
class SceneSerializer {
public:
    SceneSerializer(entt::registry& registry);

    /// Save the scene to a JSON file.
    bool save(const std::filesystem::path& path);

    /// Load the scene from a JSON file.
    bool load(const std::filesystem::path& path);

    /// Save the scene to a string (for snapshots).
    std::string save_to_string();

    /// Load the scene from a string (for snapshots).
    bool load_from_string(const std::string& data);

    /// Create JSON from current scene (for preview/clipboard).
    nlohmann::json serialize() const;

    /// Load scene from JSON object.
    bool deserialize(const nlohmann::json& json);

    /// Serialize specific entities to JSON (for clipboard).
    nlohmann::json serialize_entities(const std::vector<entt::entity>& entities) const;

    /// Deserialize entities from JSON without clearing registry (for paste).
    /// Returns the newly created root entities.
    std::vector<entt::entity> deserialize_entities(const nlohmann::json& json);

    /// Get/set scene settings.
    SceneSettings& settings() { return m_settings; }
    const SceneSettings& settings() const { return m_settings; }

    /// Get the last error message.
    const std::string& last_error() const { return m_last_error; }

private:
    // Serialization helpers
    nlohmann::json serialize_entity(entt::entity entity) const;
    entt::entity deserialize_entity(const nlohmann::json& json, entt::entity parent = entt::null);

    entt::registry& m_registry;
    SceneSettings m_settings;
    std::string m_last_error;
};

} // namespace editor
