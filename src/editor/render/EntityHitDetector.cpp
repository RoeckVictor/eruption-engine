#include "EntityHitDetector.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "engine/core/Transform.h"
#include "engine/core/MathConstants.h"
#include "engine/core/ScreenRect.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Colliders.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"

#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace editor {

namespace {

// Check if point is inside a PixelGrid entity bounds
bool point_in_pixel_grid_bounds(
    float world_x, float world_y,
    const engine::Transform& transform,
    const engine::simulation::PixelGridComponent& grid
) {
    if (grid.width <= 0 || grid.height <= 0) return false;

    // Transform point into local grid space
    float dx = world_x - transform.world_x;
    float dy = world_y - transform.world_y;

    // Inverse rotation
    float rot_rad = -transform.world_rotation * engine::DEG_TO_RAD;
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    float local_x = dx * cos_r - dy * sin_r;
    float local_y = dx * sin_r + dy * cos_r;

    // Apply inverse scale
    if (std::abs(transform.world_scale_x) > 0.0001f)
        local_x /= transform.world_scale_x;
    if (std::abs(transform.world_scale_y) > 0.0001f)
        local_y /= transform.world_scale_y;

    // Check bounds (origin offset)
    float ox = static_cast<float>(grid.origin_x);
    float oy = static_cast<float>(grid.origin_y);
    float w = static_cast<float>(grid.width);
    float h = static_cast<float>(grid.height);

    return local_x >= -ox && local_x <= (w - ox) &&
           local_y >= -oy && local_y <= (h - oy);
}

/// Check if point is inside a BoxCollider
bool point_in_box_collider(
    float world_x, float world_y,
    const engine::Transform& transform,
    const engine::physics::BoxCollider& box
) {
    // Transform to local space (inverse entity rotation)
    float dx = world_x - transform.world_x;
    float dy = world_y - transform.world_y;

    float rot_rad = -transform.world_rotation * engine::DEG_TO_RAD;
    float e_cos = std::cos(rot_rad);
    float e_sin = std::sin(rot_rad);

    float local_x = dx * e_cos - dy * e_sin;
    float local_y = dx * e_sin + dy * e_cos;

    // Apply offset (scaled)
    local_x -= box.offset_x * transform.world_scale_x;
    local_y -= box.offset_y * transform.world_scale_y;

    // Apply inverse collider rotation
    float col_rad = -box.rotation * engine::DEG_TO_RAD;
    float c_cos = std::cos(col_rad);
    float c_sin = std::sin(col_rad);

    float col_x = local_x * c_cos - local_y * c_sin;
    float col_y = local_x * c_sin + local_y * c_cos;

    // Check half-extents
    float hw = box.width * 0.5f * std::abs(transform.world_scale_x);
    float hh = box.height * 0.5f * std::abs(transform.world_scale_y);

    return std::abs(col_x) <= hw && std::abs(col_y) <= hh;
}

/// Check if point is inside a CircleCollider
bool point_in_circle_collider(
    float world_x, float world_y,
    const engine::Transform& transform,
    const engine::physics::CircleCollider& circle
) {
    // Calculate circle center in world space
    float ox = circle.offset_x * transform.world_scale_x;
    float oy = circle.offset_y * transform.world_scale_y;

    float rot_rad = transform.world_rotation * engine::DEG_TO_RAD;
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    float center_x = transform.world_x + ox * cos_r - oy * sin_r;
    float center_y = transform.world_y + ox * sin_r + oy * cos_r;

    // Check distance
    float dx = world_x - center_x;
    float dy = world_y - center_y;
    float avg_scale = (std::abs(transform.world_scale_x) + std::abs(transform.world_scale_y)) * 0.5f;
    float r = circle.radius * avg_scale;

    return (dx * dx + dy * dy) <= (r * r);
}

/// Check if point is inside a CapsuleCollider
/// A capsule is a rectangle with semicircular caps at each end.
bool point_in_capsule_collider(
    float world_x, float world_y,
    const engine::Transform& transform,
    const engine::physics::CapsuleCollider& capsule
) {
    // Transform point to entity local space
    float dx = world_x - transform.world_x;
    float dy = world_y - transform.world_y;

    float entity_rot_rad = -transform.world_rotation * engine::DEG_TO_RAD;
    float e_cos = std::cos(entity_rot_rad);
    float e_sin = std::sin(entity_rot_rad);

    float local_x = dx * e_cos - dy * e_sin;
    float local_y = dx * e_sin + dy * e_cos;

    // Apply offset (scaled)
    local_x -= capsule.offset_x * transform.world_scale_x;
    local_y -= capsule.offset_y * transform.world_scale_y;

    // Apply inverse capsule rotation
    float cap_rot_rad = -capsule.rotation * engine::DEG_TO_RAD;
    float c_cos = std::cos(cap_rot_rad);
    float c_sin = std::sin(cap_rot_rad);

    float cap_x = local_x * c_cos - local_y * c_sin;
    float cap_y = local_x * c_sin + local_y * c_cos;

    // Capsule dimensions in local space (after rotation, capsule is vertical)
    float avg_scale = (std::abs(transform.world_scale_x) + std::abs(transform.world_scale_y)) * 0.5f;
    float half_length = capsule.length * 0.5f * avg_scale;
    float radius = capsule.radius * avg_scale;

    // Clamp to the capsule's central axis segment
    float clamped_y = std::max(-half_length, std::min(half_length, cap_y));

    // Distance from point to nearest point on axis
    float dist_x = cap_x;
    float dist_y = cap_y - clamped_y;

    return (dist_x * dist_x + dist_y * dist_y) <= (radius * radius);
}

bool point_near_origin(
    float world_x, float world_y,
    const engine::Transform& transform,
    float threshold
) {
    float dx = world_x - transform.world_x;
    float dy = world_y - transform.world_y;
    return (dx * dx + dy * dy) <= (threshold * threshold);
}

bool point_in_image_bounds(
    float world_x, float world_y,
    const engine::Transform& transform,
    const engine::render::Image& image
) {
    // Use cached dimensions if available, otherwise use a default
    float tex_w = (image._cached_width > 0) ? static_cast<float>(image._cached_width) : 100.0f;
    float tex_h = (image._cached_height > 0) ? static_cast<float>(image._cached_height) : 100.0f;

    float w = tex_w * std::abs(transform.world_scale_x);
    float h = tex_h * std::abs(transform.world_scale_y);
    float hw = w * 0.5f;
    float hh = h * 0.5f;

    // Transform point into local space (centered origin)
    float dx = world_x - transform.world_x;
    float dy = world_y - transform.world_y;

    // Inverse rotation
    float rot_rad = -transform.world_rotation * engine::DEG_TO_RAD;
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    float local_x = dx * cos_r - dy * sin_r;
    float local_y = dx * sin_r + dy * cos_r;

    // Check centered bounds
    return local_x >= -hw && local_x <= hw &&
           local_y >= -hh && local_y <= hh;
}

bool point_in_text_bounds(
    float world_x, float world_y,
    const engine::Transform& transform,
    const engine::render::Text& text
) {
    if (text.content.empty()) return false;

    float char_width = text.font_size * 0.6f;
    float line_height = text.font_size * text.line_height;

    // Count lines
    int line_count = 1;
    size_t max_line_length = 0;
    size_t current_line_length = 0;
    for (char c : text.content) {
        if (c == '\n') {
            line_count++;
            max_line_length = std::max(max_line_length, current_line_length);
            current_line_length = 0;
        } else {
            current_line_length++;
        }
    }
    max_line_length = std::max(max_line_length, current_line_length);

    float w = static_cast<float>(max_line_length) * char_width * std::abs(transform.world_scale_x);
    float h = static_cast<float>(line_count) * line_height * std::abs(transform.world_scale_y);
    float hw = w * 0.5f;
    float hh = h * 0.5f;

    // Transform point into local space (centered origin)
    float dx = world_x - transform.world_x;
    float dy = world_y - transform.world_y;

    // Inverse rotation
    float rot_rad = -transform.world_rotation * engine::DEG_TO_RAD;
    float cos_r = std::cos(rot_rad);
    float sin_r = std::sin(rot_rad);

    float local_x = dx * cos_r - dy * sin_r;
    float local_y = dx * sin_r + dy * cos_r;

    // Check centered bounds (with some padding for click tolerance)
    float padding = 5.0f;
    return local_x >= -hw - padding && local_x <= hw + padding &&
           local_y >= -hh - padding && local_y <= hh + padding;
}

} // anonymous namespace

HitResult EntityHitDetector::hit_test(
    entt::registry& registry,
    ImVec2 screen_pos,
    ImVec2 viewport_pos,
    ImVec2 viewport_size,
    float cam_x,
    float cam_y,
    float zoom
) {
    HitResult result;

    // Track already-added entities for O(1) duplicate checking
    std::unordered_set<entt::entity> added_entities;

    // Helper to add entity if not already present
    auto try_add_entity = [&](entt::entity entity) {
        if (added_entities.insert(entity).second) {
            result.entities.push_back(entity);
        }
    };

    // Convert screen to world coordinates
    float world_x = cam_x + (screen_pos.x - viewport_pos.x - viewport_size.x * 0.5f) / zoom;
    float world_y = cam_y - (screen_pos.y - viewport_pos.y - viewport_size.y * 0.5f) / zoom;

    // Screen-space hit threshold (for origin-only entities)
    float origin_threshold = 8.0f / zoom;

    // Check PixelGrid entities first (most common visual entity)
    {
        auto view = registry.view<engine::Transform, engine::simulation::PixelGridComponent>();
        for (auto entity : view) {
            // Skip disabled entities
            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);
            auto& grid = view.get<engine::simulation::PixelGridComponent>(entity);

            if (point_in_pixel_grid_bounds(world_x, world_y, transform, grid)) {
                try_add_entity(entity);
            }
        }
    }

    // Check entities with box colliders
    {
        auto view = registry.view<engine::Transform, engine::physics::BoxCollider>();
        for (auto entity : view) {
            // Skip if already added
            if (added_entities.count(entity)) continue;

            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);
            auto& box = view.get<engine::physics::BoxCollider>(entity);

            if (box.material.enabled && point_in_box_collider(world_x, world_y, transform, box)) {
                try_add_entity(entity);
            }
        }
    }

    // Check entities with circle colliders
    {
        auto view = registry.view<engine::Transform, engine::physics::CircleCollider>();
        for (auto entity : view) {
            if (added_entities.count(entity)) continue;

            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);
            auto& circle = view.get<engine::physics::CircleCollider>(entity);

            if (circle.material.enabled && point_in_circle_collider(world_x, world_y, transform, circle)) {
                try_add_entity(entity);
            }
        }
    }

    // Check entities with capsule colliders
    {
        auto view = registry.view<engine::Transform, engine::physics::CapsuleCollider>();
        for (auto entity : view) {
            if (added_entities.count(entity)) continue;

            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);
            auto& capsule = view.get<engine::physics::CapsuleCollider>(entity);

            if (capsule.material.enabled && point_in_capsule_collider(world_x, world_y, transform, capsule)) {
                try_add_entity(entity);
            }
        }
    }

    // Check world-space Image entities (Transform + Image, no ScreenRect)
    {
        auto view = registry.view<engine::Transform, engine::render::Image>();
        for (auto entity : view) {
            if (added_entities.count(entity)) continue;

            // Skip screen-space entities
            if (registry.all_of<engine::ScreenRect>(entity)) continue;

            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);
            auto& image = view.get<engine::render::Image>(entity);

            if (image.enabled && point_in_image_bounds(world_x, world_y, transform, image)) {
                try_add_entity(entity);
            }
        }
    }

    // Check world-space Text entities (Transform + Text, no ScreenRect)
    {
        auto view = registry.view<engine::Transform, engine::render::Text>();
        for (auto entity : view) {
            if (added_entities.count(entity)) continue;

            // Skip screen-space entities
            if (registry.all_of<engine::ScreenRect>(entity)) continue;

            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);
            auto& text = view.get<engine::render::Text>(entity);

            if (text.enabled && point_in_text_bounds(world_x, world_y, transform, text)) {
                try_add_entity(entity);
            }
        }
    }

    // Finally, check all transform entities by origin proximity (for entities with no visual bounds)
    {
        auto view = registry.view<engine::Transform>();
        for (auto entity : view) {
            if (added_entities.count(entity)) continue;

            if (registry.all_of<EntityInfo>(entity)) {
                if (!registry.get<EntityInfo>(entity).enabled_in_hierarchy) continue;
            }

            auto& transform = view.get<engine::Transform>(entity);

            if (point_near_origin(world_x, world_y, transform, origin_threshold)) {
                try_add_entity(entity);
            }
        }
    }

    return result;
}

void EntityHitDetector::process_click_selection(
    EditorContext& context,
    const HitResult& hit,
    ImVec2 mouse_pos,
    ClickCycleState& cycle_state,
    bool ctrl_held,
    bool shift_held
) {
    if (hit.entities.empty()) {
        // Click on empty space - clear selection unless modifier held
        if (!ctrl_held && !shift_held) {
            context.selection().clear_selection();
        }
        cycle_state.reset();
        return;
    }

    // Check if we're clicking at the same position (for cycling through overlapping entities)
    float dist_sq = (mouse_pos.x - cycle_state.last_click_pos.x) * (mouse_pos.x - cycle_state.last_click_pos.x) +
                   (mouse_pos.y - cycle_state.last_click_pos.y) * (mouse_pos.y - cycle_state.last_click_pos.y);

    bool same_position = dist_sq < 4.0f;  // Within 2 pixels
    bool same_entities = (hit.entities == cycle_state.last_hit_entities);

    if (same_position && same_entities && hit.entities.size() > 1) {
        // Cycle to next entity
        cycle_state.last_hit_index = (cycle_state.last_hit_index + 1) % static_cast<int>(hit.entities.size());
    } else {
        // New click location or different entities - reset cycle
        cycle_state.last_hit_entities = hit.entities;
        cycle_state.last_hit_index = 0;
    }

    // Bounds check to prevent undefined behavior
    if (cycle_state.last_hit_index < 0 ||
        cycle_state.last_hit_index >= static_cast<int>(hit.entities.size())) {
        cycle_state.last_hit_index = 0;
    }

    entt::entity entity_to_select = hit.entities[cycle_state.last_hit_index];

    if (ctrl_held) {
        if (context.selection().is_selected(entity_to_select)) {
            context.selection().remove_from_selection(entity_to_select);
        } else {
            context.selection().add_to_selection(entity_to_select);
        }
    } else if (shift_held) {
        context.selection().add_to_selection(entity_to_select);
    } else {
        context.selection().select(entity_to_select);
    }

    cycle_state.last_click_pos = mouse_pos;
}

}