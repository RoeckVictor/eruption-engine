#include "ScreenRectSystem.h"
#include "ScreenRect.h"
#include "Transform.h"
#include "HierarchyUtils.h"
#include "engine/core/Logger.h"

namespace engine {

void ScreenRectSystem::update(entt::registry& registry, float screen_width, float screen_height) {
    // Iterate all ScreenRect entities inline to find roots, avoiding a
    // temporary std::vector allocation every frame.
    auto view = registry.view<ScreenRect>();
    for (auto entity : view) {
        if (hierarchy::is_root(registry, entity)) {
            // Root screen entities use full screen as parent bounds
            update_recursive(registry, entity, 0.0f, 0.0f, screen_width, screen_height);
        }
    }
}

void ScreenRectSystem::update_recursive(
    entt::registry& registry,
    entt::entity entity,
    float parent_x, float parent_y,
    float parent_width, float parent_height)
{
    if (!registry.all_of<ScreenRect>(entity)) return;

    auto& rect = registry.get<ScreenRect>(entity);

    // Compute anchor position within parent bounds
    float anchor_pos_x = parent_x + (rect.anchor_x * parent_width);
    float anchor_pos_y = parent_y + (rect.anchor_y * parent_height);

    // Apply offset from anchor
    float element_x = anchor_pos_x + rect.offset_x;
    float element_y = anchor_pos_y + rect.offset_y;

    // Adjust for pivot (pivot is where position is measured from)
    // The final position is the top-left corner of the rect
    rect.computed_x = element_x - (rect.pivot_x * rect.width);
    rect.computed_y = element_y - (rect.pivot_y * rect.height);
    rect.computed_width = rect.width;
    rect.computed_height = rect.height;

    // Recursively update all children using Hierarchy component
    if (registry.all_of<Hierarchy>(entity)) {
        auto& children = registry.get<Hierarchy>(entity).children;
        hierarchy::cleanup_children(registry, children);

        for (auto child : children) {
            update_recursive(registry, child,
                rect.computed_x, rect.computed_y,
                rect.computed_width, rect.computed_height);
        }
    }
}

bool ScreenRectSystem::set_parent(
    entt::registry& registry,
    entt::entity child,
    entt::entity parent)
{
    // Prevent self-parenting
    if (child == parent) return false;

    // Validate same-space constraint: child must have ScreenRect
    if (!registry.all_of<ScreenRect>(child)) {
        Logger::instance().warning("ScreenRectSystem",
            "set_parent rejected: child entity %u is not a screen entity",
            static_cast<unsigned>(child));
        return false;
    }

    if (parent != entt::null) {
        // Parent must also be a screen entity
        if (!registry.all_of<ScreenRect>(parent)) {
            Logger::instance().warning("ScreenRectSystem",
                "set_parent rejected: mixed world/screen space (entity %u -> %u)",
                static_cast<unsigned>(child), static_cast<unsigned>(parent));
            return false;
        }

        // Cycle detection
        if (hierarchy::would_create_cycle(registry, child, parent)) {
            Logger::instance().warning("ScreenRectSystem",
                "set_parent rejected: would create cycle (entity %u is ancestor of %u)",
                static_cast<unsigned>(child), static_cast<unsigned>(parent));
            return false;
        }
    }

    hierarchy::set_parent_internal(registry, child, parent);
    return true;
}

void ScreenRectSystem::remove_parent(
    entt::registry& registry,
    entt::entity child)
{
    hierarchy::remove_parent(registry, child);
}

std::vector<entt::entity> ScreenRectSystem::get_children(
    entt::registry& registry,
    entt::entity parent)
{
    return hierarchy::get_children(registry, parent);
}

std::vector<entt::entity> ScreenRectSystem::get_root_entities(
    entt::registry& registry)
{
    return hierarchy::get_root_entities<ScreenRect>(registry);
}

bool ScreenRectSystem::is_screen_entity(entt::registry& registry, entt::entity entity) {
    return registry.all_of<ScreenRect>(entity) && !registry.all_of<Transform>(entity);
}

} // namespace engine
