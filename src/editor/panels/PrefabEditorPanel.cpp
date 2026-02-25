#include "PrefabEditorPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "editor/ui/UnsavedDialog.h"
#include "editor/render/SceneRenderUtils.h"
#include "editor/render/EntityHitDetector.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Transform.h"
#include "engine/core/TransformSystem.h"
#include "engine/core/Logger.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/render/Camera2D.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace editor {

PrefabEditorPanel::PrefabEditorPanel(EditorContext& main_context)
    : Panel("Prefab Editor")
    , m_main_context(main_context)
{
}

PrefabEditorPanel::~PrefabEditorPanel() {
    m_grid_textures.clear();
}

void PrefabEditorPanel::on_open() {
}

void PrefabEditorPanel::on_close() {
    // Revert to scene context if we had the override active
    deactivate_editing_override();
    m_grid_textures.clear();
}

void PrefabEditorPanel::on_gui() {
    if (!m_has_prefab) {
        ImGui::TextDisabled("No prefab open.");
        ImGui::TextDisabled("Double-click a .prefab file to edit it.");
        return;
    }

    // When this panel is focused, activate the editing override so that
    // the Inspector and Hierarchy panels show the prefab's entities/selection
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        activate_editing_override();
    }

    render_toolbar();
    ImGui::Separator();
    render_viewport();
}

void PrefabEditorPanel::render_unsaved_dialog() {
    if (m_show_unsaved_dialog) {
        ImGui::OpenPopup("Prefab Unsaved Changes");
        m_show_unsaved_dialog = false;
    }

    std::string filename = std::filesystem::path(m_prefab_path).filename().string();
    std::string message = "The prefab '" + filename + "' has unsaved changes.";

    auto result = ui::render_unsaved_popup("Prefab Unsaved Changes", message.c_str());
    switch (result) {
        case ui::UnsavedAction::Save:
            save_prefab_file();
            if (m_pending_action) { auto a = std::move(m_pending_action); m_pending_action = nullptr; a(); }
            break;
        case ui::UnsavedAction::DontSave:
            if (m_pending_action) { auto a = std::move(m_pending_action); m_pending_action = nullptr; a(); }
            break;
        case ui::UnsavedAction::Cancel:
            m_pending_action = nullptr;
            break;
        default: break;
    }
}

void PrefabEditorPanel::open_prefab(const std::string& path) {
    auto do_open = [this, path]() {
        // Clear existing state
        m_prefab_registry.clear();
        m_selection.clear();
        m_grid_textures.clear();

        if (load_prefab_file(path)) {
            m_prefab_path = path;
            m_has_prefab = true;
            m_dirty = false;

            // Reset camera to origin
            m_camera.x = 0.0f;
            m_camera.y = 0.0f;
            m_camera.zoom = 2.0f;

            // Make panel visible
            set_visible(true);

            engine::Logger::instance().info("PrefabEditor", "Opened prefab: %s", path.c_str());
        } else {
            m_has_prefab = false;
            engine::Logger::instance().error("PrefabEditor", "Failed to open prefab: %s", path.c_str());
        }
    };

    // If current prefab is dirty, confirm before replacing
    if (m_has_prefab && m_dirty) {
        confirm_prefab_discard_or_save(do_open);
    } else {
        do_open();
    }
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void PrefabEditorPanel::render_toolbar() {
    // Save button
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
        save_prefab_file();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save prefab (overwrites file)");

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Gizmo mode buttons
    ImVec4 active_color(0.3f, 0.5f, 0.8f, 1.0f);
    ImVec4 active_hovered(0.4f, 0.6f, 0.9f, 1.0f);

    bool is_translate = (m_gizmo_mode == GizmoMode::Translate);
    if (is_translate) {
        ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
    }
    if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT "##PrefabT")) {
        m_gizmo_mode = GizmoMode::Translate;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate (W)");
    if (is_translate) ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 2);

    bool is_rotate = (m_gizmo_mode == GizmoMode::Rotate);
    if (is_rotate) {
        ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
    }
    if (ImGui::Button(ICON_FA_ROTATE "##PrefabR")) {
        m_gizmo_mode = GizmoMode::Rotate;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate (E)");
    if (is_rotate) ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 2);

    bool is_scale = (m_gizmo_mode == GizmoMode::Scale);
    if (is_scale) {
        ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
    }
    if (ImGui::Button(ICON_FA_EXPAND "##PrefabS")) {
        m_gizmo_mode = GizmoMode::Scale;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale (R)");
    if (is_scale) ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Prefab name and dirty indicator
    std::string filename = std::filesystem::path(m_prefab_path).filename().string();
    if (m_dirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s *", filename.c_str());
    } else {
        ImGui::TextDisabled("%s", filename.c_str());
    }
}

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------

void PrefabEditorPanel::render_viewport() {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Invisible button to capture input - fills all remaining space
    ImVec2 viewport_start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##PrefabViewport", avail,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool viewport_hovered = ImGui::IsItemHovered();

    m_viewport_pos = viewport_start;
    m_viewport_size = avail;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Clip to viewport area
    ImVec2 clip_max(m_viewport_pos.x + m_viewport_size.x, m_viewport_pos.y + m_viewport_size.y);
    draw_list->PushClipRect(m_viewport_pos, clip_max, true);

    // Draw background
    draw_list->AddRectFilled(m_viewport_pos, clip_max, IM_COL32(30, 30, 35, 255));

    WorldToScreen wts(m_viewport_pos, m_viewport_size, m_camera.x, m_camera.y, m_camera.zoom);

    // Draw origin crosshair
    {
        ImVec2 origin = wts(0, 0);
        float arm = 20.0f;
        draw_list->AddLine(ImVec2(origin.x - arm, origin.y), ImVec2(origin.x + arm, origin.y),
                           IM_COL32(180, 80, 80, 150), 1.0f);
        draw_list->AddLine(ImVec2(origin.x, origin.y - arm), ImVec2(origin.x, origin.y + arm),
                           IM_COL32(80, 180, 80, 150), 1.0f);
    }

    // Draw entities
    auto view = m_prefab_registry.view<engine::Transform>();
    for (auto entity : view) {
        auto& transform = view.get<engine::Transform>(entity);

        bool has_sprite = m_prefab_registry.all_of<engine::simulation::PixelGridComponent,
                                                    engine::render::PixelGridRenderer>(entity);
        if (has_sprite) {
            auto& grid_comp = m_prefab_registry.get<engine::simulation::PixelGridComponent>(entity);
            auto& renderer = m_prefab_registry.get<engine::render::PixelGridRenderer>(entity);

            if (!renderer.enabled) continue;

            auto quad = compute_pixel_grid_quad(transform, grid_comp, renderer, wts);
            void* grid_tex = m_grid_textures.get(entity, grid_comp.pixel_grid_path);
            draw_pixel_grid_quad(draw_list, quad, grid_tex);

            if (m_main_context.is_selected(entity)) {
                draw_selection_outline(draw_list, quad);
            }
        } else {
            // Non-sprite entity: draw a labeled marker
            ImVec2 screen_pos = wts(transform.world_x, transform.world_y);
            float marker_size = 6.0f;

            ImU32 marker_color = m_main_context.is_selected(entity) ? IM_COL32(255, 200, 50, 220) : IM_COL32(200, 200, 200, 180);
            draw_list->AddQuadFilled(
                ImVec2(screen_pos.x, screen_pos.y - marker_size),
                ImVec2(screen_pos.x + marker_size, screen_pos.y),
                ImVec2(screen_pos.x, screen_pos.y + marker_size),
                ImVec2(screen_pos.x - marker_size, screen_pos.y),
                marker_color
            );

            if (m_prefab_registry.all_of<EntityInfo>(entity)) {
                const auto& info = m_prefab_registry.get<EntityInfo>(entity);
                draw_list->AddText(ImVec2(screen_pos.x + marker_size + 4, screen_pos.y - 7),
                                   IM_COL32(220, 220, 220, 200), info.name.c_str());
            }
        }
    }

    m_grid_textures.cleanup(&m_prefab_registry);

    // Draw camera info
    char info[128];
    snprintf(info, sizeof(info), "Zoom: %.1fx", m_camera.zoom);
    draw_list->AddText(ImVec2(m_viewport_pos.x + 8, m_viewport_pos.y + 8),
                       IM_COL32(180, 180, 180, 180), info);

    draw_list->PopClipRect();

    // Update gizmo state BEFORE handling input (so gizmo takes priority)
    update_gizmos(m_viewport_pos, m_viewport_size);

    // Render gizmos (outside clip rect so they can draw over edges)
    render_gizmos(draw_list, m_viewport_pos, m_viewport_size);

    // Handle input
    if (viewport_hovered && !m_gizmo_active && !m_gizmo_hovering) {
        handle_viewport_input();

        // Click to select entity
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImGuiIO& io = ImGui::GetIO();
            ImVec2 mouse_pos = io.MousePos;

            HitResult hit = EntityHitDetector::hit_test(
                m_prefab_registry,
                mouse_pos,
                m_viewport_pos,
                m_viewport_size,
                m_camera.x,
                m_camera.y,
                m_camera.zoom
            );

            EntityHitDetector::process_click_selection(
                m_main_context, hit, mouse_pos, m_click_cycle_state,
                io.KeyCtrl, io.KeyShift
            );
        }
    }

    // Gizmo mode shortcuts (when viewport is hovered)
    if (viewport_hovered && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmo_mode = GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmo_mode = GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmo_mode = GizmoMode::Scale;
    }
}

// ---------------------------------------------------------------------------
// Gizmos
// ---------------------------------------------------------------------------

void PrefabEditorPanel::update_gizmos(ImVec2 vp_pos, ImVec2 vp_size) {
    m_gizmo_hovering = false;

    const auto& sel = m_selection;
    if (sel.empty()) {
        m_gizmo_active = false;
        return;
    }

    Gizmo* active_gizmo = nullptr;
    switch (m_gizmo_mode) {
        case GizmoMode::Translate: active_gizmo = &m_translate_gizmo; break;
        case GizmoMode::Rotate:    active_gizmo = &m_rotate_gizmo; break;
        case GizmoMode::Scale:     active_gizmo = &m_scale_gizmo; break;
    }
    if (!active_gizmo) return;

    entt::entity entity = sel.front();
    if (!m_prefab_registry.valid(entity) || !m_prefab_registry.all_of<engine::Transform>(entity)) {
        m_gizmo_active = false;
        return;
    }

    auto& transform = m_prefab_registry.get<engine::Transform>(entity);

    GizmoResult result = active_gizmo->update(
        vp_pos, vp_size,
        entity, transform,
        m_camera.x, m_camera.y, m_camera.zoom,
        GizmoSpace::World
    );

    m_gizmo_active = result.is_active;
    m_gizmo_hovering = active_gizmo->is_hovering();

    if (result.value_changed) {
        m_dirty = true;
        update_world_transforms(m_prefab_registry);
    }
}

void PrefabEditorPanel::render_gizmos(ImDrawList* draw_list, ImVec2 vp_pos, ImVec2 vp_size) {
    const auto& sel = m_selection;
    if (sel.empty()) {
        return;
    }

    Gizmo* active_gizmo = nullptr;
    switch (m_gizmo_mode) {
        case GizmoMode::Translate: active_gizmo = &m_translate_gizmo; break;
        case GizmoMode::Rotate:    active_gizmo = &m_rotate_gizmo; break;
        case GizmoMode::Scale:     active_gizmo = &m_scale_gizmo; break;
    }
    if (!active_gizmo) return;

    entt::entity entity = sel.front();
    if (!m_prefab_registry.valid(entity) || !m_prefab_registry.all_of<engine::Transform>(entity)) {
        return;
    }

    const auto& transform = m_prefab_registry.get<engine::Transform>(entity);

    active_gizmo->render(
        draw_list, vp_pos, vp_size,
        transform,
        m_camera.x, m_camera.y, m_camera.zoom,
        GizmoSpace::World
    );
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void PrefabEditorPanel::handle_viewport_input() {
    m_camera.handle_input();
}

void PrefabEditorPanel::activate_editing_override() {
    m_main_context.set_editing_override({
        &m_prefab_registry,
        &m_selection,
        [this]() { m_dirty = true; }
    });
}

void PrefabEditorPanel::deactivate_editing_override() {
    // Only clear if we are the active override
    if (m_main_context.has_editing_override() &&
        m_main_context.registry() == &m_prefab_registry) {
        m_main_context.clear_editing_override();
    }
}

bool PrefabEditorPanel::on_close_requested() {
    if (!m_dirty) {
        deactivate_editing_override();
        return true;
    }

    // Dirty - show confirmation dialog, prevent close for now
    confirm_prefab_discard_or_save([this]() {
        deactivate_editing_override();
        m_has_prefab = false;
        m_dirty = false;
        set_visible(false);
    });
    return false;
}

void PrefabEditorPanel::confirm_prefab_discard_or_save(std::function<void()> action) {
    if (!m_dirty) {
        action();
        return;
    }

    m_pending_action = std::move(action);
    m_show_unsaved_dialog = true;
}

// ---------------------------------------------------------------------------
// Prefab I/O
// ---------------------------------------------------------------------------

bool PrefabEditorPanel::load_prefab_file(const std::string& path) {
    SceneSerializer serializer(m_prefab_registry);
    entt::entity entity = serializer.load_prefab(path);
    return entity != entt::null;
}

bool PrefabEditorPanel::save_prefab_file() {
    if (m_prefab_path.empty()) return false;

    // Find the root entity (first entity with EntityInfo)
    entt::entity root = entt::null;
    auto view = m_prefab_registry.view<EntityInfo>();
    if (view.begin() != view.end()) {
        root = *view.begin();
    }

    if (root == entt::null) {
        engine::Logger::instance().error("PrefabEditor", "No entity to save");
        return false;
    }

    SceneSerializer serializer(m_prefab_registry);
    if (serializer.save_prefab(m_prefab_path, root)) {
        m_dirty = false;
        engine::Logger::instance().info("PrefabEditor", "Saved prefab: %s", m_prefab_path.c_str());

        // Sync changes to all prefab instances in the scene
        m_main_context.sync_prefab_to_instances(m_prefab_path);

        return true;
    }

    return false;
}

} // namespace editor
