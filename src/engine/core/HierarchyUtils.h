#pragma once

#include "Hierarchy.h"
#include <entt/entt.hpp>
#include <vector>
#include <algorithm>

namespace engine::hierarchy {

/// Check if an entity is a root (has no valid parent).
inline bool is_root(const entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<Hierarchy>(entity)) return true;
    const auto& h = registry.get<Hierarchy>(entity);
    return h.parent == entt::null || !registry.valid(h.parent);
}

/// Check if setting new_parent as parent of child would create a cycle.
/// Returns true if a cycle would be created.
inline bool would_create_cycle(const entt::registry& registry, entt::entity child, entt::entity new_parent) {
    if (new_parent == entt::null) return false;

    entt::entity cursor = new_parent;
    while (cursor != entt::null && registry.valid(cursor)) {
        if (cursor == child) {
            return true;
        }
        if (registry.all_of<Hierarchy>(cursor)) {
            cursor = registry.get<Hierarchy>(cursor).parent;
        } else {
            break;
        }
    }
    return false;
}

/// Internal function to set parent-child relationship without validation.
/// Call would_create_cycle() first if validation is needed.
inline void set_parent_internal(entt::registry& registry, entt::entity child, entt::entity parent) {
    auto& child_h = registry.get_or_emplace<Hierarchy>(child);

    // Remove from old parent's children
    if (child_h.parent != entt::null && registry.valid(child_h.parent)) {
        if (registry.all_of<Hierarchy>(child_h.parent)) {
            auto& old_children = registry.get<Hierarchy>(child_h.parent).children;
            old_children.erase(
                std::remove(old_children.begin(), old_children.end(), child),
                old_children.end());
        }
    }

    // Set new parent
    child_h.parent = parent;

    // Add to new parent's children (guard against duplicates)
    if (parent != entt::null) {
        auto& parent_h = registry.get_or_emplace<Hierarchy>(parent);
        if (std::find(parent_h.children.begin(), parent_h.children.end(), child) == parent_h.children.end()) {
            parent_h.children.push_back(child);
        }
    }
}

/// Remove parent relationship, making the entity a root.
inline void remove_parent(entt::registry& registry, entt::entity child) {
    if (!registry.all_of<Hierarchy>(child)) return;

    auto& child_h = registry.get<Hierarchy>(child);

    // Remove from old parent's children
    if (child_h.parent != entt::null && registry.valid(child_h.parent)) {
        if (registry.all_of<Hierarchy>(child_h.parent)) {
            auto& old_children = registry.get<Hierarchy>(child_h.parent).children;
            old_children.erase(
                std::remove(old_children.begin(), old_children.end(), child),
                old_children.end());
        }
    }

    child_h.parent = entt::null;
}

/// Get direct children of an entity.
inline std::vector<entt::entity> get_children(entt::registry& registry, entt::entity parent) {
    if (registry.all_of<Hierarchy>(parent)) {
        return registry.get<Hierarchy>(parent).children;
    }
    return {};
}

/// Get root entities that have a specific component type.
template<typename ComponentType>
std::vector<entt::entity> get_root_entities(entt::registry& registry) {
    std::vector<entt::entity> roots;

    auto view = registry.view<ComponentType>();
    roots.reserve(registry.storage<ComponentType>().size());

    for (auto entity : view) {
        if (is_root(registry, entity)) {
            roots.push_back(entity);
        }
    }

    return roots;
}

/// Remove dead entities from a children list.
inline void cleanup_children(entt::registry& registry, std::vector<entt::entity>& children) {
    children.erase(
        std::remove_if(children.begin(), children.end(),
            [&](entt::entity child) { return !registry.valid(child); }),
        children.end());
}

} // namespace engine::hierarchy
