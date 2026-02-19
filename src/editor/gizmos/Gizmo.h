#pragma once

#include "editor/core/EditorComponents.h"
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
    ImVec2 world_to_screen(
        float world_x, float world_y,
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        float screen_x = viewport_pos.x + viewport_size.x * 0.5f + (world_x - camera_x) * zoom;
        float screen_y = viewport_pos.y + viewport_size.y * 0.5f - (world_y - camera_y) * zoom;
        return ImVec2(screen_x, screen_y);
    }

    ImVec2 screen_to_world(
        float screen_x, float screen_y,
        ImVec2 viewport_pos, ImVec2 viewport_size,
        float camera_x, float camera_y, float zoom
    ) const {
        float world_x = camera_x + (screen_x - viewport_pos.x - viewport_size.x * 0.5f) / zoom;
        float world_y = camera_y - (screen_y - viewport_pos.y - viewport_size.y * 0.5f) / zoom;
        return ImVec2(world_x, world_y);
    }

    bool m_is_dragging = false;
    bool m_is_hovering = false;
    engine::Transform m_start_transform;
    ImVec2 m_drag_start_mouse;
    ImVec2 m_drag_start_world;
};

}
