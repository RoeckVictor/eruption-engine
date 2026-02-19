#pragma once

#include "Gizmo.h"
#include "TranslateGizmo.h"
#include "RotateGizmo.h"
#include "ScaleGizmo.h"
#include <memory>

namespace editor {

class EditorContext;

class GizmoRenderer {
public:
    explicit GizmoRenderer(EditorContext& context);
    ~GizmoRenderer() = default;

    void set_mode(GizmoMode mode) { m_mode = mode; }
    GizmoMode mode() const { return m_mode; }

    void set_space(GizmoSpace space) { m_space = space; }
    GizmoSpace space() const { return m_space; }

    void update(ImVec2 viewport_pos, ImVec2 viewport_size);
    void render(ImDrawList* draw_list, ImVec2 viewport_pos, ImVec2 viewport_size);

    bool is_active() const { return m_is_active; }
    bool is_hovering() const { return m_is_hovering; }

    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

private:
    void generate_command(entt::entity entity, const engine::Transform& old_transform, const engine::Transform& new_transform);

    EditorContext& m_context;

    GizmoMode m_mode = GizmoMode::Translate;
    GizmoSpace m_space = GizmoSpace::World;
    bool m_enabled = true;
    bool m_is_active = false;
    bool m_is_hovering = false;

    ImVec2 m_viewport_pos;
    ImVec2 m_viewport_size;

    TranslateGizmo m_translate_gizmo;
    RotateGizmo m_rotate_gizmo;
    ScaleGizmo m_scale_gizmo;

    entt::entity m_active_entity = entt::null;
    engine::Transform m_start_transform;
};

}
