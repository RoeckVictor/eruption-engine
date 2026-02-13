#pragma once

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <typeindex>
#include "engine/core/Transform.h"
#include "engine/core/TransformSystem.h"

namespace editor {

/// Component that stores entity metadata for the editor.
struct EntityInfo {
    std::string name = "Entity";
    std::string guid;  // Unique identifier for serialization
    bool enabled = true;              // Like Unity's "activeSelf" (local enabled state)
    bool enabled_in_hierarchy = true; // Like Unity's "activeInHierarchy" (cached effective state)

    // Prefab information (if this entity came from a prefab)
    std::string prefab_path;
    bool is_prefab_instance = false;

    // Component display order in inspector (type names).
    // If empty, components are shown in default registration order.
    std::vector<std::string> component_order;
};

// Unified hierarchy component lives in the engine.
// Import into editor namespace so existing code keeps working unqualified.
using engine::Hierarchy;

// --- Hierarchy Helper Functions ---

/// Set an entity's parent. Updates both parent and child hierarchy components.
void set_parent(entt::registry& registry, entt::entity child, entt::entity new_parent);

/// Remove an entity from its parent.
void remove_from_parent(entt::registry& registry, entt::entity child);

/// Get the root entities (entities with no parent).
std::vector<entt::entity> get_root_entities(entt::registry& registry);

/// Update world transforms based on hierarchy.
void update_world_transforms(entt::registry& registry);

/// Set entity enabled state and propagate to children (Unity-style).
/// Updates both 'enabled' (local) and 'enabled_in_hierarchy' (effective) fields.
void set_entity_enabled(entt::registry& registry, entt::entity entity, bool enabled);

/// Recursively update enabled_in_hierarchy for entity and all children.
/// Called internally by set_entity_enabled().
void update_enabled_in_hierarchy(entt::registry& registry, entt::entity entity);

/// Initialize component type registry for dynamic component access.
/// Must be called once at editor startup (after reflection initialization).
void init_component_type_registry();

/// Create a new entity with default components.
entt::entity create_entity(entt::registry& registry, const std::string& name = "Entity");

/// Destroy an entity and all its children.
void destroy_entity_recursive(entt::registry& registry, entt::entity entity);

} // namespace editor
