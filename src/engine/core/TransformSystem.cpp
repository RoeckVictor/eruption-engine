#include "TransformSystem.h"
#include "Transform.h"
#include "HierarchyUtils.h"
#include "MathConstants.h"
#include "engine/core/Logger.h"
#include <cmath>

namespace engine {

void TransformSystem::update(entt::registry& registry) {
    // Iterate all Transform entities inline to find roots, avoiding a
    // temporary std::vector allocation every frame.
    auto view = registry.view<Transform>();
    for (auto entity : view) {
        if (hierarchy::is_root(registry, entity)) {
            update_recursive(registry, entity, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        }
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
        auto& children = registry.get<Hierarchy>(entity).children;
        hierarchy::cleanup_children(registry, children);

        for (auto child : children) {
            update_recursive(registry, child,
                transform.world_x, transform.world_y,
                transform.world_rotation,
                transform.world_scale_x, transform.world_scale_y);
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

    // Cycle detection
    if (hierarchy::would_create_cycle(registry, child, parent)) {
        Logger::instance().warning("TransformSystem",
            "set_parent rejected: would create cycle (entity %u is ancestor of %u)",
            static_cast<unsigned>(child), static_cast<unsigned>(parent));
        return;
    }

    hierarchy::set_parent_internal(registry, child, parent);
}

void TransformSystem::remove_parent(
    entt::registry& registry,
    entt::entity child)
{
    hierarchy::remove_parent(registry, child);
}

std::vector<entt::entity> TransformSystem::get_children(
    entt::registry& registry,
    entt::entity parent)
{
    return hierarchy::get_children(registry, parent);
}

std::vector<entt::entity> TransformSystem::get_root_entities(
    entt::registry& registry)
{
    return hierarchy::get_root_entities<Transform>(registry);
}

} // namespace engine
