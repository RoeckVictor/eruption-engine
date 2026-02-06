#pragma once

#include "Gizmo.h"
#include "TranslateGizmo.h"
#include "RotateGizmo.h"
#include "ScaleGizmo.h"
#include <memory>

namespace editor {

class EditorContext;

/// Manages and renders all gizmos for the viewport.
/// Handles switching between gizmo modes and generating undo commands.
class GizmoRenderer {
public:
    explicit GizmoRenderer(EditorContext& context);
    ~GizmoRenderer() = default;

    /// Set the current gizmo mode.
    void set_mode(GizmoMode mode) { m_mode = mode; }

    /// Get the current gizmo mode.
    GizmoMode mode() const { return m_mode; }

    /// Set the coordinate space (local/world).
    void set_space(GizmoSpace space) { m_space = space; }

    /// Get the coordinate space.
    GizmoSpace space() const { return m_space; }

    /// Render gizmos for selected entities.
    /// Should be called after rendering the scene, during the overlay pass.
    /// @param draw_list ImGui draw list to render to
    /// @param viewport_pos Top-left corner of the viewport in screen space
    /// @param viewport_size Size of the viewport
    void render(ImDrawList* draw_list, ImVec2 viewport_pos, ImVec2 viewport_size);

    /// Check if any gizmo is currently being manipulated.
    bool is_active() const { return m_is_active; }

    /// Enable or disable gizmo rendering.
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

private:
    /// Generate a transform command when manipulation ends.
    void generate_command(entt::entity entity, const Transform& old_transform, const Transform& new_transform);

    EditorContext& m_context;

    GizmoMode m_mode = GizmoMode::Translate;
    GizmoSpace m_space = GizmoSpace::World;
    bool m_enabled = true;
    bool m_is_active = false;

    // Gizmo instances
    TranslateGizmo m_translate_gizmo;
    RotateGizmo m_rotate_gizmo;
    ScaleGizmo m_scale_gizmo;

    // Track which entity is being manipulated
    entt::entity m_active_entity = entt::null;
    Transform m_start_transform;
};

} // namespace editor
