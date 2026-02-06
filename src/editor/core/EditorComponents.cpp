#include "EditorComponents.h"
#include <algorithm>
#include <cmath>

namespace editor {

void set_parent(entt::registry& registry, entt::entity child, entt::entity new_parent) {
    if (child == new_parent) return;  // Can't parent to self

    // Ensure child has Hierarchy component
    if (!registry.all_of<Hierarchy>(child)) {
        registry.emplace<Hierarchy>(child);
    }

    auto& child_hierarchy = registry.get<Hierarchy>(child);

    // Remove from old parent
    if (child_hierarchy.parent != entt::null && registry.valid(child_hierarchy.parent)) {
        if (registry.all_of<Hierarchy>(child_hierarchy.parent)) {
            auto& old_parent_hierarchy = registry.get<Hierarchy>(child_hierarchy.parent);
            auto& children = old_parent_hierarchy.children;
            children.erase(std::remove(children.begin(), children.end(), child), children.end());
        }
    }

    // Set new parent
    child_hierarchy.parent = new_parent;

    // Add to new parent's children
    if (new_parent != entt::null) {
        if (!registry.all_of<Hierarchy>(new_parent)) {
            registry.emplace<Hierarchy>(new_parent);
        }
        auto& parent_hierarchy = registry.get<Hierarchy>(new_parent);
        parent_hierarchy.children.push_back(child);
    }
}

void remove_from_parent(entt::registry& registry, entt::entity child) {
    set_parent(registry, child, entt::null);
}

std::vector<entt::entity> get_root_entities(entt::registry& registry) {
    std::vector<entt::entity> roots;

    auto view = registry.view<EntityInfo>();
    for (auto entity : view) {
        bool is_root = true;
        if (registry.all_of<Hierarchy>(entity)) {
            const auto& hierarchy = registry.get<Hierarchy>(entity);
            if (hierarchy.parent != entt::null && registry.valid(hierarchy.parent)) {
                is_root = false;
            }
        }
        if (is_root) {
            roots.push_back(entity);
        }
    }

    return roots;
}

void update_world_transforms(entt::registry& registry) {
    // First, compute world transforms for root entities
    auto roots = get_root_entities(registry);

    std::function<void(entt::entity, float, float, float, float, float)> update_recursive =
        [&](entt::entity entity, float parent_x, float parent_y, float parent_rot, float parent_sx, float parent_sy) {
            if (!registry.all_of<Transform>(entity)) return;

            auto& transform = registry.get<Transform>(entity);

            // Apply parent transform
            float cos_rot = std::cos(parent_rot * 3.14159265f / 180.0f);
            float sin_rot = std::sin(parent_rot * 3.14159265f / 180.0f);

            float local_x = transform.x * parent_sx;
            float local_y = transform.y * parent_sy;

            transform.world_x = parent_x + local_x * cos_rot - local_y * sin_rot;
            transform.world_y = parent_y + local_x * sin_rot + local_y * cos_rot;
            transform.world_rotation = parent_rot + transform.rotation;
            transform.world_scale_x = parent_sx * transform.scale_x;
            transform.world_scale_y = parent_sy * transform.scale_y;

            // Update children
            if (registry.all_of<Hierarchy>(entity)) {
                const auto& hierarchy = registry.get<Hierarchy>(entity);
                for (auto child : hierarchy.children) {
                    if (registry.valid(child)) {
                        update_recursive(child,
                            transform.world_x, transform.world_y,
                            transform.world_rotation,
                            transform.world_scale_x, transform.world_scale_y);
                    }
                }
            }
        };

    for (auto root : roots) {
        update_recursive(root, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }
}

entt::entity create_entity(entt::registry& registry, const std::string& name) {
    auto entity = registry.create();
    registry.emplace<EntityInfo>(entity, EntityInfo{name, "", true, "", false});
    registry.emplace<Transform>(entity);
    registry.emplace<Hierarchy>(entity);
    return entity;
}

void destroy_entity_recursive(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity)) return;

    // First destroy all children
    if (registry.all_of<Hierarchy>(entity)) {
        auto& hierarchy = registry.get<Hierarchy>(entity);
        // Copy children list since we'll be modifying it
        auto children = hierarchy.children;
        for (auto child : children) {
            destroy_entity_recursive(registry, child);
        }
    }

    // Remove from parent
    remove_from_parent(registry, entity);

    // Destroy the entity
    registry.destroy(entity);
}

} // namespace editor
