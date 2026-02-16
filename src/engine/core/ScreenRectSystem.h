#pragma once

#include "Hierarchy.h"
#include <entt/entt.hpp>
#include <vector>

namespace engine {

/// System for updating hierarchical screen-space transforms.
/// Computes computed_x, computed_y, computed_width, computed_height
/// for all entities with ScreenRect components based on parent-child relationships
/// stored in the Hierarchy component.
class ScreenRectSystem {
public:
    /// Update all computed screen positions based on hierarchy.
    /// Requires screen dimensions to compute anchor positions for root entities.
    /// Iterates all root screen entities and recursively computes positions for children.
    static void update(entt::registry& registry, float screen_width, float screen_height);

    /// Set parent-child relationship with same-space validation.
    /// Only allows parenting if both entities have ScreenRect (or child becomes root).
    /// Returns true if successful, false if rejected (mixed-space parenting or cycle).
    static bool set_parent(entt::registry& registry, entt::entity child, entt::entity parent);

    /// Remove parent relationship.
    /// Sets the child's parent to entt::null and removes from old parent's children.
    static void remove_parent(entt::registry& registry, entt::entity child);

    /// Get direct children of an entity.
    /// Returns the children vector from the Hierarchy component (O(1)).
    static std::vector<entt::entity> get_children(entt::registry& registry, entt::entity parent);

    /// Get screen-space root entities (entities with ScreenRect but no parent).
    /// Returns all entities with ScreenRect component that either:
    /// - Don't have a Hierarchy component, OR
    /// - Have a Hierarchy with parent == entt::null or invalid parent
    static std::vector<entt::entity> get_root_entities(entt::registry& registry);

    /// Check if an entity is a screen-space entity (has ScreenRect, not Transform).
    static bool is_screen_entity(entt::registry& registry, entt::entity entity);

private:
    /// Recursive helper for updating computed positions.
    /// Applies parent bounds to compute anchor position, then applies offset and pivot.
    static void update_recursive(
        entt::registry& registry,
        entt::entity entity,
        float parent_x, float parent_y,
        float parent_width, float parent_height
    );
};

} // namespace engine
