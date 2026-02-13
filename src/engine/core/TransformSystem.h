#pragma once

#include "Hierarchy.h"
#include <entt/entt.hpp>
#include <vector>

namespace engine {

/// System for updating hierarchical world transforms.
/// Computes world_x, world_y, world_rotation, world_scale_x, world_scale_y
/// for all entities with Transform components based on parent-child relationships
/// stored in the Hierarchy component.
class TransformSystem {
public:
    /// Update all world transforms based on hierarchy.
    /// Iterates all root entities (no parent or invalid parent) and recursively
    /// computes world transforms for all children.
    static void update(entt::registry& registry);

    /// Set parent-child relationship.
    /// Updates the Hierarchy component on both child and parent entities,
    /// managing parent references and children vectors.
    /// Does nothing if child == parent (prevents self-parenting).
    static void set_parent(entt::registry& registry, entt::entity child, entt::entity parent);

    /// Remove parent relationship.
    /// Sets the child's parent to entt::null and removes from old parent's children.
    static void remove_parent(entt::registry& registry, entt::entity child);

    /// Get direct children of an entity.
    /// Returns the children vector from the Hierarchy component (O(1)).
    static std::vector<entt::entity> get_children(entt::registry& registry, entt::entity parent);

    /// Get root entities (entities with Transform but no parent).
    /// Returns all entities with Transform component that either:
    /// - Don't have a Hierarchy component, OR
    /// - Have a Hierarchy with parent == entt::null or invalid parent
    static std::vector<entt::entity> get_root_entities(entt::registry& registry);

private:
    /// Recursive helper for updating world transforms.
    /// Applies parent transform (position, rotation, scale) to entity's local transform
    /// and recursively updates all children.
    static void update_recursive(
        entt::registry& registry,
        entt::entity entity,
        float parent_x, float parent_y, float parent_rot,
        float parent_sx, float parent_sy
    );
};

} // namespace engine
