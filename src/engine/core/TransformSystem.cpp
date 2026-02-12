#include "TransformSystem.h"
#include "Transform.h"
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
    float cos_rot = std::cos(parent_rot * 3.14159265f / 180.0f);
    float sin_rot = std::sin(parent_rot * 3.14159265f / 180.0f);

    float local_x = transform.x * parent_sx;
    float local_y = transform.y * parent_sy;

    transform.world_x = parent_x + local_x * cos_rot - local_y * sin_rot;
    transform.world_y = parent_y + local_x * sin_rot + local_y * cos_rot;
    transform.world_rotation = parent_rot + transform.rotation;
    transform.world_scale_x = parent_sx * transform.scale_x;
    transform.world_scale_y = parent_sy * transform.scale_y;

    // Recursively update all children
    auto children = get_children(registry, entity);
    for (auto child : children) {
        if (registry.valid(child)) {
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

    // Add or update TransformParent component
    if (!registry.all_of<TransformParent>(child)) {
        registry.emplace<TransformParent>(child, parent);
    } else {
        registry.get<TransformParent>(child).parent = parent;
    }
}

void TransformSystem::remove_parent(
    entt::registry& registry,
    entt::entity child)
{
    if (registry.all_of<TransformParent>(child)) {
        registry.remove<TransformParent>(child);
    }
}

std::vector<entt::entity> TransformSystem::get_children(
    entt::registry& registry,
    entt::entity parent)
{
    std::vector<entt::entity> children;

    // Query all entities with TransformParent component
    auto view = registry.view<TransformParent>();

    for (auto entity : view) {
        const auto& tp = view.get<TransformParent>(entity);
        if (tp.parent == parent) {
            children.push_back(entity);
        }
    }

    return children;
}

std::vector<entt::entity> TransformSystem::get_root_entities(
    entt::registry& registry)
{
    std::vector<entt::entity> roots;

    // All entities with Transform are potential roots
    auto view = registry.view<Transform>();

    for (auto entity : view) {
        // Entity is a root if:
        // 1. It doesn't have a TransformParent component, OR
        // 2. It has a TransformParent but parent is null or invalid
        if (!registry.all_of<TransformParent>(entity)) {
            roots.push_back(entity);
        } else {
            const auto& tp = registry.get<TransformParent>(entity);
            if (tp.parent == entt::null || !registry.valid(tp.parent)) {
                roots.push_back(entity);
            }
        }
    }

    return roots;
}

} // namespace engine
