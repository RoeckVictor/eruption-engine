#include "TransformSystem.h"
#include "Transform.h"
#include "MathConstants.h"
#include <algorithm>
#include <cmath>

namespace engine {

void TransformSystem::update(entt::registry& registry) {
    // Get all root entities and recursively update their transforms
    auto roots = get_root_entities(registry);

    for (auto root : roots) {
        // Root entities have no parent transform (identity)
        update_recursive(registry, root, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }
}

void TransformSystem::update_recursive(
    entt::registry& registry,
    entt::entity entity,
    float parent_x, float parent_y, float parent_rot,
    float parent_sx, float parent_sy)
{
    if (!registry.all_of<Transform>(entity)) return;

    auto& transform = registry.get<Transform>(entity);

    // Apply parent transform to local transform
    // 1. Scale local position by parent scale
    // 2. Rotate by parent rotation
    // 3. Add parent position
    float cos_rot = std::cos(parent_rot * DEG_TO_RAD);
    float sin_rot = std::sin(parent_rot * DEG_TO_RAD);

    float local_x = transform.x * parent_sx;
    float local_y = transform.y * parent_sy;

    transform.world_x = parent_x + local_x * cos_rot - local_y * sin_rot;
    transform.world_y = parent_y + local_x * sin_rot + local_y * cos_rot;
    transform.world_rotation = parent_rot + transform.rotation;
    transform.world_scale_x = parent_sx * transform.scale_x;
    transform.world_scale_y = parent_sy * transform.scale_y;

    // Recursively update all children using Hierarchy component
    if (registry.all_of<Hierarchy>(entity)) {
        for (auto child : registry.get<Hierarchy>(entity).children) {
            if (registry.valid(child)) {
                update_recursive(registry, child,
                    transform.world_x, transform.world_y,
                    transform.world_rotation,
                    transform.world_scale_x, transform.world_scale_y);
            }
        }
    }
}

void TransformSystem::set_parent(
    entt::registry& registry,
    entt::entity child,
    entt::entity parent)
{
    // Prevent self-parenting
    if (child == parent) return;

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

    // Add to new parent's children
    if (parent != entt::null) {
        auto& parent_h = registry.get_or_emplace<Hierarchy>(parent);
        parent_h.children.push_back(child);
    }
}

void TransformSystem::remove_parent(
    entt::registry& registry,
    entt::entity child)
{
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

std::vector<entt::entity> TransformSystem::get_children(
    entt::registry& registry,
    entt::entity parent)
{
    if (registry.all_of<Hierarchy>(parent)) {
        return registry.get<Hierarchy>(parent).children;
    }
    return {};
}

std::vector<entt::entity> TransformSystem::get_root_entities(
    entt::registry& registry)
{
    std::vector<entt::entity> roots;

    // All entities with Transform are potential roots
    auto view = registry.view<Transform>();

    for (auto entity : view) {
        // Entity is a root if:
        // 1. It doesn't have a Hierarchy component, OR
        // 2. It has a Hierarchy but parent is null or invalid
        if (!registry.all_of<Hierarchy>(entity)) {
            roots.push_back(entity);
        } else {
            const auto& h = registry.get<Hierarchy>(entity);
            if (h.parent == entt::null || !registry.valid(h.parent)) {
                roots.push_back(entity);
            }
        }
    }

    return roots;
}

} // namespace engine
