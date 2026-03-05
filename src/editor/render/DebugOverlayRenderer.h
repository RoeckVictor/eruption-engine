#pragma once

#include "editor/core/CoordinateUtils.h"
#include "editor/core/EditorContext.h"
#include <entt/entt.hpp>
#include <imgui.h>
#include <functional>

namespace editor {

class EditorPixelGridLoader;
class RuntimeContext;

// Configuration for debug overlay rendering
// Allows shared rendering code between ViewportPanel and PrefabEditorPanel
struct DebugOverlayConfig {
    entt::registry* registry = nullptr;

    const GizmoVisibilitySettings* visibility = nullptr;
    const CoordinateTransform* transform = nullptr;

    ImDrawList* draw_list = nullptr;
    std::function<bool(GizmoVisibility, entt::entity)> should_draw;

    EditorPixelGridLoader* pixel_grid_loader = nullptr;

    RuntimeContext* runtime = nullptr;

    bool is_playing = false;
};

// Shared debug overlay renderer for editor viewports
// Renders colliders, origins, names, camera bounds, etc.
namespace DebugOverlayRenderer {

    constexpr ImU32 COLLIDER_COLOR    = IM_COL32(0, 200, 0, 180);
    constexpr ImU32 TRIGGER_COLOR     = IM_COL32(200, 200, 0, 180);
    constexpr ImU32 ORIGIN_COLOR      = IM_COL32(255, 255, 255, 200);
    constexpr ImU32 NAME_COLOR        = IM_COL32(220, 220, 220, 200);
    constexpr ImU32 CAMERA_COLOR      = IM_COL32(220, 50, 220, 180);
    constexpr ImU32 VELOCITY_COLOR    = IM_COL32(255, 220, 50, 220);
    constexpr ImU32 GRID_BOUNDS_COLOR = IM_COL32(0, 220, 220, 150);
    constexpr ImU32 LINK_COLOR        = IM_COL32(150, 150, 150, 120);
    constexpr ImU32 TERRAIN_COLOR     = IM_COL32(0, 200, 220, 180);

    void render(const DebugOverlayConfig& config);

    void render_colliders(const DebugOverlayConfig& config);
    void render_terrain_colliders(const DebugOverlayConfig& config);
    void render_object_origins(const DebugOverlayConfig& config);
    void render_object_names(const DebugOverlayConfig& config);
    void render_camera_bounds(const DebugOverlayConfig& config);
    void render_rigidbody_velocity(const DebugOverlayConfig& config);
    void render_pixel_grid_bounds(const DebugOverlayConfig& config);
    void render_parent_child_links(const DebugOverlayConfig& config);

}

}
