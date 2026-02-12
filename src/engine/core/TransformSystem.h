#pragma once

#include <entt/entt.hpp>
#include <vector>

namespace engine {

/// Optional parent reference for hierarchical transforms.
/// When present, Transform.world_* values are computed relative to parent.
struct TransformParent {
    entt::entity parent = entt::null;
};

/// System for updating hierarchical world transforms.
/// Computes world_x, world_y, world_rotation, world_scale_x, world_scale_y
/// for all entities with Transform components based on parent-child relationships.
class TransformSystem {
public:
    /// Update all world transforms based on hierarchy.
    /// Iterates all root entities (no parent or invalid parent) and recursively
    /// computes world transforms for all children.
    static void update(entt::registry& registry);

    /// Set parent-child relationship.
    /// Adds or updates the TransformParent component on the child entity.
    /// Does nothing if child == parent (prevents self-parenting).
    static void set_parent(entt::registry& registry, entt::entity child, entt::entity parent);

    /// Remove parent relationship.
    /// Removes the TransformParent component from the child entity.
    static void remove_parent(entt::registry& registry, entt::entity child);

    /// Get direct children of an entity.
    /// Queries all entities with TransformParent component and returns those
    /// whose parent matches the given entity.
    static std::vector<entt::entity> get_children(entt::registry& registry, entt::entity parent);

    /// Get root entities (entities with Transform but no parent).
    /// Returns all entities with Transform component that either:
    /// - Don't have a TransformParent component, OR
    /// - Have a TransformParent with parent == entt::null or invalid parent
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
