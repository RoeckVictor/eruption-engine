#include "GizmoRenderer.h"
#include "editor/core/EditorContext.h"
#include "editor/commands/TransformCommand.h"

namespace editor {

GizmoRenderer::GizmoRenderer(EditorContext& context)
    : m_context(context)
{
}

void GizmoRenderer::render(ImDrawList* draw_list, ImVec2 viewport_pos, ImVec2 viewport_size) {
    if (!m_enabled) {
        return;
    }

    // Don't show gizmos in play mode
    if (m_context.is_playing()) {
        return;
    }

    auto* registry = m_context.registry();
    if (!registry) {
        return;
    }

    const auto& selection = m_context.selection();
    if (selection.empty()) {
        m_is_active = false;
        return;
    }

    // Get camera info
    const auto& camera = m_context.camera();

    // Get the active gizmo based on mode
    Gizmo* active_gizmo = nullptr;
    switch (m_mode) {
        case GizmoMode::Translate:
            active_gizmo = &m_translate_gizmo;
            break;
        case GizmoMode::Rotate:
            active_gizmo = &m_rotate_gizmo;
            break;
        case GizmoMode::Scale:
            active_gizmo = &m_scale_gizmo;
            break;
    }

    if (!active_gizmo) {
        return;
    }

    // For now, only render gizmo for the first selected entity
    // TODO: Support multi-selection gizmo (average position, etc.)
    entt::entity entity = selection.front();

    if (!registry->valid(entity) || !registry->all_of<Transform>(entity)) {
        m_is_active = false;
        return;
    }

    auto& transform = registry->get<Transform>(entity);

    // Render the gizmo
    GizmoResult result = active_gizmo->render(
        draw_list,
        viewport_pos,
        viewport_size,
        entity,
        transform,
        camera.x,
        camera.y,
        camera.zoom
    );

    m_is_active = result.is_active;

    // Apply snap-to-grid if enabled and transform changed (for translate mode)
    if (result.value_changed && m_context.is_snap_enabled() && m_mode == GizmoMode::Translate) {
        transform.x = m_context.snap_to_grid(transform.x);
        transform.y = m_context.snap_to_grid(transform.y);
    }

    // Track manipulation start
    if (result.just_started) {
        m_active_entity = entity;
        m_start_transform = active_gizmo->start_transform();
    }

    // Generate command when manipulation ends
    if (result.just_finished && m_active_entity != entt::null) {
        generate_command(m_active_entity, m_start_transform, transform);
        m_active_entity = entt::null;
    }

    // Mark dirty if value changed
    if (result.value_changed) {
        m_context.mark_dirty();
    }
}

void GizmoRenderer::generate_command(entt::entity entity, const Transform& old_transform, const Transform& new_transform) {
    auto* registry = m_context.registry();
    if (!registry) {
        return;
    }

    // Only create command if transform actually changed
    if (old_transform.x == new_transform.x &&
        old_transform.y == new_transform.y &&
        old_transform.rotation == new_transform.rotation &&
        old_transform.scale_x == new_transform.scale_x &&
        old_transform.scale_y == new_transform.scale_y) {
        return;
    }

    auto cmd = std::make_unique<TransformCommand>(registry, entity, old_transform, new_transform);
    // Note: execute() is not called here because the transform is already applied
    // We add it to history directly for undo support
    m_context.history().add_executed(std::move(cmd));
}

} // namespace editor
