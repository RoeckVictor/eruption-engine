#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace editor {

/// Component that stores entity metadata for the editor.
struct EntityInfo {
    std::string name = "Entity";
    std::string guid;  // Unique identifier for serialization
    bool enabled = true;

    // Prefab information (if this entity came from a prefab)
    std::string prefab_path;
    bool is_prefab_instance = false;
};

/// Component for parent-child hierarchy.
struct Hierarchy {
    entt::entity parent = entt::null;
    std::vector<entt::entity> children;
};

/// Transform component for position, rotation, and scale.
/// Used by the editor for all entities.
struct Transform {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;  // In degrees
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    // Computed world transform (for hierarchical transforms)
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_rotation = 0.0f;
    float world_scale_x = 1.0f;
    float world_scale_y = 1.0f;
};

// --- Hierarchy Helper Functions ---

/// Set an entity's parent. Updates both parent and child hierarchy components.
void set_parent(entt::registry& registry, entt::entity child, entt::entity new_parent);

/// Remove an entity from its parent.
void remove_from_parent(entt::registry& registry, entt::entity child);

/// Get the root entities (entities with no parent).
std::vector<entt::entity> get_root_entities(entt::registry& registry);

/// Update world transforms based on hierarchy.
void update_world_transforms(entt::registry& registry);

/// Create a new entity with default components.
entt::entity create_entity(entt::registry& registry, const std::string& name = "Entity");

/// Destroy an entity and all its children.
void destroy_entity_recursive(entt::registry& registry, entt::entity entity);

} // namespace editor
