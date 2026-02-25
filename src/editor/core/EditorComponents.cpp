#include "EditorComponents.h"
#include "ComponentTypeRegistry.h"
#include "engine/reflection/TypeInfo.h"
#include "engine/reflection/EngineComponentList.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Logger.h"
#include "engine/core/ScreenRect.h"
#include "engine/core/ScreenRectSystem.h"
#include "engine/render/Camera2D.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/SimSurface.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include <cmath>
#include <unordered_map>
#include <random>
#include <sstream>
#include <iomanip>

namespace editor {

void set_parent(entt::registry& registry, entt::entity child, entt::entity new_parent) {
    if (child == new_parent) return;

    // Same-space validation: screen entities can only parent to screen entities
    if (new_parent != entt::null) {
        bool child_is_screen = is_screen_space_entity(registry, child);
        bool parent_is_screen = is_screen_space_entity(registry, new_parent);
        if (child_is_screen != parent_is_screen) {
            engine::Logger::instance().warning("Editor",
                "Cannot set parent: mixed world/screen space hierarchy not allowed");
            return;
        }
    }

    // Detect circular hierarchy: walk from new_parent to root, reject if child is encountered
    if (new_parent != entt::null) {
        entt::entity ancestor = new_parent;
        while (ancestor != entt::null) {
            if (ancestor == child) {
                engine::Logger::instance().warning("Editor",
                    "Cannot set parent: would create circular hierarchy (entity %u)",
                    static_cast<unsigned>(child));
                return;
            }
            if (!registry.valid(ancestor) || !registry.all_of<Hierarchy>(ancestor)) break;
            ancestor = registry.get<Hierarchy>(ancestor).parent;
        }
    }

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

    engine::TransformSystem::set_parent(registry, child, new_parent);

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
            float rad = -pw_rot * engine::DEG_TO_RAD;
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

    update_enabled_in_hierarchy(registry, child);
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
    engine::TransformSystem::update(registry);
}

void update_screen_rects(entt::registry& registry, float screen_width, float screen_height) {
    engine::ScreenRectSystem::update(registry, screen_width, screen_height);
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

std::string generate_entity_guid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    auto hex = [&](int bytes) {
        std::ostringstream oss;
        uint32_t val = dist(gen);
        oss << std::hex << std::setfill('0') << std::setw(bytes * 2) << (val & ((1ull << (bytes * 8)) - 1));
        return oss.str();
    };

    return hex(4) + "-" + hex(2) + "-" + hex(2) + "-" + hex(2) + "-" + hex(4) + hex(2);
}

entt::entity create_entity(entt::registry& registry, const std::string& name) {
    auto entity = registry.create();
    std::string guid = generate_entity_guid();
    registry.emplace<EntityInfo>(entity, EntityInfo{name, guid, true, true, "", false});
    registry.emplace<engine::Transform>(entity);
    registry.emplace<Hierarchy>(entity);
    return entity;
}

entt::entity create_screen_entity(entt::registry& registry, const std::string& name) {
    auto entity = registry.create();
    std::string guid = generate_entity_guid();
    registry.emplace<EntityInfo>(entity, EntityInfo{name, guid, true, true, "", false});
    registry.emplace<engine::ScreenRect>(entity);
    registry.emplace<Hierarchy>(entity);
    return entity;
}

bool is_screen_space_entity(entt::registry& registry, entt::entity entity) {
    return engine::ScreenRectSystem::is_screen_entity(registry, entity);
}

bool is_world_space_entity(entt::registry& registry, entt::entity entity) {
    return registry.all_of<engine::Transform>(entity) &&
           !registry.all_of<engine::ScreenRect>(entity);
}

std::vector<entt::entity> get_world_root_entities(entt::registry& registry) {
    std::vector<entt::entity> roots;

    auto view = registry.view<EntityInfo, engine::Transform>();
    for (auto entity : view) {
        // Skip entities that also have ScreenRect (shouldn't happen, but be safe)
        if (registry.all_of<engine::ScreenRect>(entity)) continue;

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

std::vector<entt::entity> get_screen_root_entities(entt::registry& registry) {
    std::vector<entt::entity> roots;

    auto view = registry.view<EntityInfo, engine::ScreenRect>();
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

    remove_from_parent(registry, entity);

    registry.destroy(entity);
}

void init_component_type_registry() {
    auto& registry = ComponentTypeRegistry::instance();

    // Register all engine components from the central list (EngineComponentList.h)
    #define REGISTER_EDITOR(T) registry.register_component<T>();
    ENGINE_COMPONENT_LIST(REGISTER_EDITOR)
    #undef REGISTER_EDITOR
}

void apply_component_order(
    entt::registry& registry, entt::entity entity,
    std::vector<const engine::reflection::TypeInfo*>& types)
{
    if (!registry.all_of<EntityInfo>(entity)) return;
    const auto& info = registry.get<EntityInfo>(entity);
    if (info.component_order.empty()) return;

    // Build a map from type name -> TypeInfo* for O(1) lookup
    std::unordered_map<std::string, const engine::reflection::TypeInfo*> type_map;
    type_map.reserve(types.size());
    for (const auto* ti : types) {
        type_map[ti->name()] = ti;
    }

    std::vector<const engine::reflection::TypeInfo*> ordered;
    ordered.reserve(types.size());

    // Add types in stored order
    for (const auto& type_name : info.component_order) {
        auto it = type_map.find(type_name);
        if (it != type_map.end()) {
            ordered.push_back(it->second);
            type_map.erase(it);
        }
    }
    // Append any remaining types not in the stored order
    for (const auto* ti : types) {
        if (type_map.count(ti->name())) {
            ordered.push_back(ti);
        }
    }
    types = std::move(ordered);
}

}
