#include "GizmoRenderer.h"
#include "editor/core/EditorContext.h"
#include "editor/commands/TransformCommand.h"

namespace editor {

GizmoRenderer::GizmoRenderer(EditorContext& context)
    : m_context(context)
{
}

void GizmoRenderer::update(ImVec2 viewport_pos, ImVec2 viewport_size) {
    // Cache viewport info for render()
    m_viewport_pos = viewport_pos;
    m_viewport_size = viewport_size;
    m_is_hovering = false;

    if (!m_enabled) {
        m_is_active = false;
        return;
    }

    // Don't show gizmos in play mode
    if (m_context.is_playing()) {
        m_is_active = false;
        return;
    }

    auto* registry = m_context.registry();
    if (!registry) {
        m_is_active = false;
        return;
    }

    const auto& selection = m_context.selection().selection();
    if (selection.empty()) {
        m_is_active = false;
        return;
    }

    // Get camera info
    const auto& camera = m_context.viewport().camera;

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
        m_is_active = false;
        return;
    }

    // For now, only handle gizmo for the first selected entity
    // TODO: Support multi-selection gizmo (average position, etc.)
    entt::entity entity = selection.front();

    if (!registry->valid(entity) || !registry->all_of<engine::Transform>(entity)) {
        m_is_active = false;
        return;
    }

    auto& transform = registry->get<engine::Transform>(entity);

    // Update the gizmo (process input)
    GizmoResult result = active_gizmo->update(
        viewport_pos,
        viewport_size,
        entity,
        transform,
        camera.x,
        camera.y,
        camera.zoom,
        m_space
    );

    m_is_active = result.is_active;
    m_is_hovering = active_gizmo->is_hovering();

    // Apply snap-to-grid if enabled and transform changed (for translate mode)
    if (result.value_changed && m_context.viewport().snap_enabled && m_mode == GizmoMode::Translate) {
        transform.x = m_context.viewport().snap_to_grid(transform.x);
        transform.y = m_context.viewport().snap_to_grid(transform.y);
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
        m_context.scene_state().mark_dirty();
    }
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

    const auto& selection = m_context.selection().selection();
    if (selection.empty()) {
        return;
    }

    // Get camera info
    const auto& camera = m_context.viewport().camera;

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
    entt::entity entity = selection.front();

    if (!registry->valid(entity) || !registry->all_of<engine::Transform>(entity)) {
        return;
    }

    const auto& transform = registry->get<engine::Transform>(entity);

    // Render the gizmo (draw only, no input processing)
    active_gizmo->render(
        draw_list,
        viewport_pos,
        viewport_size,
        transform,
        camera.x,
        camera.y,
        camera.zoom,
        m_space
    );
}

void GizmoRenderer::generate_command(entt::entity entity, const engine::Transform& old_transform, const engine::Transform& new_transform) {
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

}
