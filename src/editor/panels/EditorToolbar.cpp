#include "EditorToolbar.h"
#include "editor/core/EditorContext.h"
#include "editor/core/RuntimeContext.h"
#include "editor/scripting/ScriptManager.h"
#include "editor/panels/PanelManager.h"
#include "editor/panels/ViewportPanel.h"
#include "editor/gizmos/GizmoRenderer.h"
#include "editor/icons/IconsFontAwesome6.h"

#include "engine/rhi/RHIDevice.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace editor {

EditorToolbar::EditorToolbar(EditorContext& context, RuntimeContext& runtime,
                             ScriptManager& scripts, PanelManager& panels)
    : m_context(context)
    , m_runtime(runtime)
    , m_scripts(scripts)
    , m_panels(panels)
{
}

void EditorToolbar::render() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    float toolbar_height = 40.0f;
    m_panels.set_toolbar_height(toolbar_height);

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbar_height));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        render_play_controls();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        render_gizmo_mode_buttons();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        render_grid_snap_controls();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        render_gizmo_visibility_popup();

        render_backend_info();
        render_script_status();
    }
    ImGui::End();

    ImGui::PopStyleVar();
}

void EditorToolbar::render_play_controls() {
    bool is_playing = m_runtime.is_playing();
    bool is_paused = m_runtime.is_paused();

    // Play button - changes to Resume when paused
    if (!is_playing) {
        if (ImGui::Button(ICON_FA_PLAY)) {
            m_runtime.play(m_context.scene_settings());
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play");
    } else if (is_paused) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
        if (ImGui::Button(ICON_FA_PLAY)) {
            m_runtime.resume();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resume");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
        ImGui::Button(ICON_FA_PLAY);
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Pause button
    ImGui::BeginDisabled(!is_playing || is_paused);
    if (ImGui::Button(ICON_FA_PAUSE)) {
        m_runtime.pause();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause");
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Stop button
    ImGui::BeginDisabled(!is_playing);
    if (ImGui::Button(ICON_FA_STOP)) {
        m_runtime.stop();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Step button (only when paused)
    ImGui::BeginDisabled(!is_paused);
    if (ImGui::Button(ICON_FA_FORWARD_STEP)) {
        m_runtime.step_frame();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step Frame");
    ImGui::EndDisabled();

    // Show play mode info
    if (is_playing) {
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (is_paused) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "PAUSED");
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "PLAYING");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%.1fs | %llu frames", m_runtime.play_time(), m_runtime.frame_count());
    }
}

void EditorToolbar::render_gizmo_mode_buttons() {
    auto* viewport_panel = m_panels.get_panel<ViewportPanel>();
    GizmoMode current_mode = viewport_panel ? viewport_panel->gizmo_renderer().mode() : GizmoMode::Translate;

    ImVec4 active_color(0.3f, 0.5f, 0.8f, 1.0f);
    ImVec4 active_hovered(0.4f, 0.6f, 0.9f, 1.0f);

    // Move button (W)
    bool is_translate = (current_mode == GizmoMode::Translate);
    if (is_translate) {
        ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
    }
    if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT)) {
        if (viewport_panel) viewport_panel->gizmo_renderer().set_mode(GizmoMode::Translate);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate Tool (W)");
    if (is_translate) ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 2);

    // Rotate button (E)
    bool is_rotate = (current_mode == GizmoMode::Rotate);
    if (is_rotate) {
        ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
    }
    if (ImGui::Button(ICON_FA_ROTATE)) {
        if (viewport_panel) viewport_panel->gizmo_renderer().set_mode(GizmoMode::Rotate);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate Tool (E)");
    if (is_rotate) ImGui::PopStyleColor(2);

    ImGui::SameLine(0, 2);

    // Scale button (R)
    bool is_scale = (current_mode == GizmoMode::Scale);
    if (is_scale) {
        ImGui::PushStyleColor(ImGuiCol_Button, active_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active_hovered);
    }
    if (ImGui::Button(ICON_FA_EXPAND)) {
        if (viewport_panel) viewport_panel->gizmo_renderer().set_mode(GizmoMode::Scale);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale Tool (R)");
    if (is_scale) ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Coordinate space toggle (Local/World)
    bool is_local = m_context.viewport().local_space;
    if (ImGui::Button(is_local ? ICON_FA_CUBE " Local" : ICON_FA_GLOBE " World")) {
        m_context.viewport().local_space = !is_local;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle coordinate space for gizmos");
}

void EditorToolbar::render_grid_snap_controls() {
    bool grid_visible = m_context.viewport().grid_visible;
    if (ImGui::Checkbox(ICON_FA_BORDER_ALL " Grid", &grid_visible)) {
        m_context.viewport().grid_visible = grid_visible;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle grid visibility (G)");

    ImGui::SameLine();

    bool snap_enabled = m_context.viewport().snap_enabled;
    if (ImGui::Checkbox(ICON_FA_MAGNET " Snap", &snap_enabled)) {
        m_context.viewport().snap_enabled = snap_enabled;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle snap to grid");

    // Grid size input (only visible when snap is enabled)
    if (snap_enabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        float grid_size = m_context.viewport().grid_size;
        if (ImGui::DragFloat("##GridSize", &grid_size, 1.0f, 1.0f, 256.0f, "%.0f")) {
            m_context.viewport().grid_size = grid_size;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid size for snapping");
    }
}

void EditorToolbar::render_gizmo_visibility_popup() {
    if (ImGui::Button(ICON_FA_EYE " Gizmos")) {
        ImGui::OpenPopup("GizmoVisibilityPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Configure debug overlay visibility");

    if (ImGui::BeginPopup("GizmoVisibilityPopup")) {
        auto& vis = m_context.gizmo_visibility();

        ImGui::TextUnformatted("Debug Overlays");
        ImGui::Separator();

        if (ImGui::BeginTable("##GizmoVisTable", 4, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Overlay", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Off");
            ImGui::TableSetupColumn("Sel");
            ImGui::TableSetupColumn("All");
            ImGui::TableHeadersRow();

            auto visibility_row = [](const char* label, GizmoVisibility& v) {
                ImGui::PushID(label);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(label);
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("##none", v == GizmoVisibility::None))
                    v = GizmoVisibility::None;
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("##sel", v == GizmoVisibility::SelectedOnly))
                    v = GizmoVisibility::SelectedOnly;
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("##all", v == GizmoVisibility::All))
                    v = GizmoVisibility::All;
                ImGui::PopID();
            };

            visibility_row("Colliders", vis.colliders);
            visibility_row("Terrain Colliders", vis.terrain_colliders);
            visibility_row("Object Origin", vis.object_origin);
            visibility_row("Object Name", vis.object_name);
            visibility_row("Camera Bounds", vis.camera_bounds);
            visibility_row("Rigidbody Velocity", vis.rigidbody_velocity);
            visibility_row("Pixel Grid Bounds", vis.pixel_grid_bounds);
            visibility_row("Parent-Child Links", vis.parent_child_links);

            ImGui::EndTable();
        }

        ImGui::EndPopup();
    }
}

void EditorToolbar::render_backend_info() {
    auto* device = engine::rhi::get_current_device();
    if (!device) return;

    const char* api_name = "Unknown";
    switch (device->backend()) {
        case engine::rhi::Backend::OpenGL: api_name = "OpenGL"; break;
        case engine::rhi::Backend::Vulkan: api_name = "Vulkan"; break;
        case engine::rhi::Backend::D3D12:  api_name = "D3D12";  break;
        case engine::rhi::Backend::Metal:  api_name = "Metal";  break;
    }

    char label[128];
    snprintf(label, sizeof(label), "%s | %s", api_name, device->renderer_name());

    float text_width = ImGui::CalcTextSize(label).x;
    float script_status_width = 200.0f;
    float x = ImGui::GetWindowWidth() - script_status_width - text_width - 20.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(x);
    ImGui::TextDisabled("%s", label);
}

void EditorToolbar::render_script_status() {
    ImGui::SameLine();
    float script_status_width = 200.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - script_status_width - 10.0f);

    if (m_scripts.is_building()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Building scripts...");
    } else if (m_scripts.are_scripts_loaded()) {
        ImGui::TextDisabled("Scripts: %zu types", m_scripts.dll_manager().script_types().size());
    } else {
        ImGui::TextDisabled("No scripts");
    }
}

}