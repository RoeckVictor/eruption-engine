#include "EditorComponents.h"
#include "ComponentTypeRegistry.h"
#include "engine/render/Camera2D.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/SimSurface.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/gameplay/PlayerController.h"
#include "engine/gameplay/CameraFollower.h"
#include <algorithm>
#include <cmath>

namespace editor {

void set_parent(entt::registry& registry, entt::entity child, entt::entity new_parent) {
    if (child == new_parent) return;  // Can't parent to self

    // Save child's current world transform so we can preserve it
    float saved_world_x = 0, saved_world_y = 0, saved_world_rot = 0;
    float saved_world_sx = 1, saved_world_sy = 1;
    if (registry.all_of<engine::Transform>(child)) {
        const auto& t = registry.get<engine::Transform>(child);
        saved_world_x = t.world_x;
        saved_world_y = t.world_y;
        saved_world_rot = t.world_rotation;
        saved_world_sx = t.world_scale_x;
        saved_world_sy = t.world_scale_y;
    }

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

    // ALSO update engine::TransformParent for runtime transform hierarchy
    engine::TransformSystem::set_parent(registry, child, new_parent);

    // Recalculate local transform to preserve world position (Unity-style)
    if (registry.all_of<engine::Transform>(child)) {
        auto& t = registry.get<engine::Transform>(child);

        if (new_parent != entt::null && registry.all_of<engine::Transform>(new_parent)) {
            // Has new parent: compute local = inverse(parent_world) * saved_world
            const auto& pt = registry.get<engine::Transform>(new_parent);
            float pw_x = pt.world_x;
            float pw_y = pt.world_y;
            float pw_rot = pt.world_rotation;
            float pw_sx = pt.world_scale_x;
            float pw_sy = pt.world_scale_y;

            // Inverse-rotate the position delta by parent's world rotation
            float dx = saved_world_x - pw_x;
            float dy = saved_world_y - pw_y;
            float rad = -pw_rot * 3.14159265f / 180.0f;
            float cos_r = std::cos(rad);
            float sin_r = std::sin(rad);

            // Divide by parent scale to get local position
            t.x = (pw_sx != 0.0f) ? (dx * cos_r - dy * sin_r) / pw_sx : 0.0f;
            t.y = (pw_sy != 0.0f) ? (dx * sin_r + dy * cos_r) / pw_sy : 0.0f;

            t.rotation = saved_world_rot - pw_rot;
            t.scale_x = (pw_sx != 0.0f) ? saved_world_sx / pw_sx : 1.0f;
            t.scale_y = (pw_sy != 0.0f) ? saved_world_sy / pw_sy : 1.0f;
        } else {
            // Un-parented: local values become world values
            t.x = saved_world_x;
            t.y = saved_world_y;
            t.rotation = saved_world_rot;
            t.scale_x = saved_world_sx;
            t.scale_y = saved_world_sy;
        }
    }

    // Update enabled_in_hierarchy for child and its descendants (parent changed)
    update_enabled_in_hierarchy(registry, child);
}

void remove_from_parent(entt::registry& registry, entt::entity child) {
    set_parent(registry, child, entt::null);
    // Note: set_parent already calls engine::TransformSystem::set_parent with entt::null
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
    // Use engine TransformSystem for world transform calculation
    engine::TransformSystem::update(registry);
}

void set_entity_enabled(entt::registry& registry, entt::entity entity, bool enabled) {
    if (!registry.valid(entity)) return;

    if (registry.all_of<EntityInfo>(entity)) {
        auto& info = registry.get<EntityInfo>(entity);
        info.enabled = enabled;
        update_enabled_in_hierarchy(registry, entity);
    }
}

void update_enabled_in_hierarchy(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity)) return;

    if (!registry.all_of<EntityInfo>(entity)) return;

    auto& info = registry.get<EntityInfo>(entity);

    // Calculate effective enabled state (local enabled AND parent enabled_in_hierarchy)
    bool parent_enabled = true;
    if (registry.all_of<Hierarchy>(entity)) {
        auto parent = registry.get<Hierarchy>(entity).parent;
        if (parent != entt::null && registry.valid(parent)) {
            if (registry.all_of<EntityInfo>(parent)) {
                parent_enabled = registry.get<EntityInfo>(parent).enabled_in_hierarchy;
            }
        }
    }

    info.enabled_in_hierarchy = info.enabled && parent_enabled;

    // Recursively update children
    if (registry.all_of<Hierarchy>(entity)) {
        const auto& hierarchy = registry.get<Hierarchy>(entity);
        for (auto child : hierarchy.children) {
            update_enabled_in_hierarchy(registry, child);
        }
    }
}

entt::entity create_entity(entt::registry& registry, const std::string& name) {
    auto entity = registry.create();
    registry.emplace<EntityInfo>(entity, EntityInfo{name, "", true, true, "", false});
    registry.emplace<engine::Transform>(entity);
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

void init_component_type_registry() {
    auto& registry = ComponentTypeRegistry::instance();

    // Register all engine component types
    registry.register_component<engine::Transform>();
    registry.register_component<engine::render::Camera2D>();
    registry.register_component<engine::render::PixelGridRenderer>();
    registry.register_component<engine::animation::Animator>();
    registry.register_component<engine::simulation::PixelGridComponent>();
    registry.register_component<engine::simulation::SimSurface>();
    registry.register_component<engine::physics::Rigidbody>();
    registry.register_component<engine::physics::BoxCollider>();
    registry.register_component<engine::physics::CapsuleCollider>();
    registry.register_component<engine::physics::CircleCollider>();
    registry.register_component<engine::physics::DynamicCollider>();
    registry.register_component<engine::gameplay::PlayerController>();
    registry.register_component<engine::gameplay::CameraFollower>();
}

} // namespace editor
