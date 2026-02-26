#pragma once

#include "editor/core/EditorComponents.h"
#include "editor/core/CoordinateUtils.h"
#include <entt/entt.hpp>
#include <imgui.h>

namespace editor {

class EditorContext;

enum class GizmoMode {
    Translate,
    Rotate,
    Scale
};

enum class GizmoSpace {
    Local,
    World
};

struct GizmoResult {
    bool is_active = false;
    bool value_changed = false;
    bool just_started = false;
    bool just_finished = false;
};

class Gizmo {
public:
    virtual ~Gizmo() = default;

    virtual GizmoResult update(
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        entt::entity entity,
        engine::Transform& transform,
        float camera_x,
        float camera_y,
        float zoom,
        GizmoSpace space = GizmoSpace::World
    ) = 0;

    virtual void render(
        ImDrawList* draw_list,
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        const engine::Transform& transform,
        float camera_x,
        float camera_y,
        float zoom,
        GizmoSpace space = GizmoSpace::World
    ) = 0;

    bool is_dragging() const { return m_is_dragging; }
    bool is_hovering() const { return m_is_hovering; }

    const engine::Transform& start_transform() const { return m_start_transform; }

protected:
    /// Create a coordinate transform for the viewport.
    CoordinateTransform make_transform(
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        return CoordinateTransform(viewport_pos, viewport_size, camera_x, camera_y, zoom);
    }

    /// Convenience wrapper for world_to_screen using individual parameters.
    ImVec2 world_to_screen(
        float world_x, float world_y,
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        return make_transform(viewport_pos, viewport_size, camera_x, camera_y, zoom)
            .world_to_screen(world_x, world_y);
    }

    /// Convenience wrapper for screen_to_world using individual parameters.
    ImVec2 screen_to_world(
        float screen_x, float screen_y,
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        return make_transform(viewport_pos, viewport_size, camera_x, camera_y, zoom)
            .screen_to_world(screen_x, screen_y);
    }

    /// Check if gizmo center is visible within viewport bounds (with margin).
    bool is_visible_in_viewport(
        ImVec2 center,
        ImVec2 viewport_pos,
        ImVec2 viewport_size,
        float margin
    ) const {
        return center.x >= viewport_pos.x - margin &&
               center.x <= viewport_pos.x + viewport_size.x + margin &&
               center.y >= viewport_pos.y - margin &&
               center.y <= viewport_pos.y + viewport_size.y + margin;
    }

    /// Get rotation cache for screen-space axis directions.
    RotationCache get_rotation_cache(const engine::Transform& transform, GizmoSpace space) const {
        float rotation_deg = (space == GizmoSpace::Local) ? transform.world_rotation : 0.0f;
        return RotationCache::from_degrees(rotation_deg);
    }

    bool m_is_dragging = false;
    bool m_is_hovering = false;
    engine::Transform m_start_transform;
    ImVec2 m_drag_start_mouse;
    ImVec2 m_drag_start_world;
};

}
