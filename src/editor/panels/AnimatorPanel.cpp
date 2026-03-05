#include "AnimatorPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "editor/inspectors/AssetPicker.h"
#include "engine/animation/StateTransition.h"
#include "engine/asset/loaders/AnimatorControllerLoader.h"
#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include "engine/core/MathConstants.h"
#include "engine/platform/PlatformUtils.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace editor {

// Animation clip file extensions
static const std::vector<std::string> ANIM_EXTENSIONS = {
    ".anim"
};

// Node visual constants
static constexpr float NODE_WIDTH = 150.0f;
static constexpr float NODE_TITLE_HEIGHT = 24.0f;
static constexpr float NODE_CONTENT_HEIGHT = 20.0f;
static constexpr float NODE_ROUNDING = 4.0f;
static constexpr float GRID_SIZE = 32.0f;

// Colors
static const ImU32 COLOR_GRID_LINES = IM_COL32(50, 50, 50, 255);
static const ImU32 COLOR_GRID_LINES_MAJOR = IM_COL32(70, 70, 70, 255);
static const ImU32 COLOR_NODE_BG = IM_COL32(45, 45, 48, 255);
static const ImU32 COLOR_NODE_TITLE = IM_COL32(70, 70, 75, 255);
static const ImU32 COLOR_NODE_TITLE_DEFAULT = IM_COL32(80, 120, 80, 255);
static const ImU32 COLOR_NODE_TITLE_ANY = IM_COL32(160, 100, 50, 255);
static const ImU32 COLOR_NODE_BORDER = IM_COL32(100, 100, 105, 255);
static const ImU32 COLOR_NODE_BORDER_SELECTED = IM_COL32(255, 180, 80, 255);
static const ImU32 COLOR_NODE_TEXT = IM_COL32(220, 220, 220, 255);
static const ImU32 COLOR_NODE_TEXT_DIM = IM_COL32(150, 150, 150, 255);
static const ImU32 COLOR_TRANSITION = IM_COL32(180, 180, 180, 255);
static const ImU32 COLOR_TRANSITION_SELECTED = IM_COL32(255, 200, 100, 255);
static const ImU32 COLOR_TRANSITION_ANY = IM_COL32(200, 140, 80, 255);

AnimatorPanel::AnimatorPanel(EditorContext& context)
    : Panel("Animator", PanelVisibilityMode::OnDemand)
    , m_context(context)
{
}

AnimatorPanel::~AnimatorPanel() {
    cleanup_node_editor();
}

void AnimatorPanel::on_open() {
    setup_node_editor();
}

void AnimatorPanel::on_close() {
    cleanup_node_editor();
}

void AnimatorPanel::setup_node_editor() {
    if (!m_editor_initialized) {
        m_editor_initialized = true;
    }
}

void AnimatorPanel::cleanup_node_editor() {
    if (m_editor_initialized) {
        m_editor_initialized = false;
        m_positioned_nodes.clear();
        m_any_state_positioned = false;
    }
}

void AnimatorPanel::on_gui() {
    if (!m_editor_initialized) {
        setup_node_editor();
    }

    render_toolbar();

    // Split layout: parameters | node editor | properties
    float available_width = ImGui::GetContentRegionAvail().x;
    float node_editor_width = available_width - m_parameters_width - m_properties_width - 16.0f;

    // Left panel - Parameters
    ImGui::BeginChild("ParametersPanel", ImVec2(m_parameters_width, 0), true);
    render_parameters_panel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Center - Node Editor
    ImGui::BeginChild("NodeEditorPanel", ImVec2(node_editor_width, 0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    render_node_editor();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel - Properties
    ImGui::BeginChild("PropertiesPanel", ImVec2(m_properties_width, 0), true);
    render_properties_panel();
    ImGui::EndChild();

    // New state dialog
    if (m_show_new_state_dialog) {
        ImGui::OpenPopup("New State");
        m_show_new_state_dialog = false;
    }
    if (ImGui::BeginPopupModal("New State", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new animation state");
        ImGui::Separator();

        ImGui::InputText("Name", m_new_state_name, sizeof(m_new_state_name));

        // Animation Clip with asset picker button
        ImGui::Text("Animation Clip");
        AssetPickerConfig clip_config;
        clip_config.popup_id = "NewStateClipPicker";
        clip_config.title = "Select Animation Clip (.anim)";
        clip_config.extensions = ANIM_EXTENSIONS;
        clip_config.empty_message = "No .anim files found in Assets folder";
        clip_config.clear_button_label = "Clear Selection";
        clip_config.button_tooltip = "Click to select an animation clip file";

        const auto& project_path = m_context.scene_state().project_path();
        auto clip_result = AssetPicker::draw_button(m_new_state_clip, project_path, clip_config);
        if (clip_result.changed) {
            m_new_state_clip = clip_result.selected_path;
        }

        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(m_new_state_name) > 0) {
                create_state(m_new_state_name, m_context_menu_x, m_context_menu_y);
                if (!m_new_state_clip.empty()) {
                    auto* state = m_controller.get_state(m_new_state_name);
                    if (state) {
                        state->clip_path = m_new_state_clip;
                    }
                }
                m_new_state_name[0] = '\0';
                m_new_state_clip.clear();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_new_state_name[0] = '\0';
            m_new_state_clip.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // New parameter dialog
    if (m_show_new_param_dialog) {
        ImGui::OpenPopup("New Parameter");
        m_show_new_param_dialog = false;
    }
    if (ImGui::BeginPopupModal("New Parameter", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new parameter");
        ImGui::Separator();

        ImGui::InputText("Name", m_new_param_name, sizeof(m_new_param_name));

        const char* types[] = { "Bool", "Int", "Float", "Trigger" };
        ImGui::Combo("Type", &m_new_param_type, types, 4);

        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(m_new_param_name) > 0) {
                create_parameter(m_new_param_name, static_cast<engine::animation::ParameterType>(m_new_param_type));
                m_new_param_name[0] = '\0';
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AnimatorPanel::render_toolbar() {
    if (ImGui::Button(ICON_FA_FILE " New")) {
        new_controller();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open")) {
        show_open_dialog();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
        save_controller();
    }
    ImGui::SameLine();

    if (ImGui::Button(ICON_FA_FILE_EXPORT " Save As")) {
        show_save_as_dialog();
    }
    ImGui::SameLine();

    // Show current file name
    ImGui::Separator();
    ImGui::SameLine();
    if (!m_current_path.empty()) {
        std::string filename = m_current_path.substr(m_current_path.find_last_of("/\\") + 1);
        if (m_has_unsaved_changes) {
            ImGui::Text("%s*", filename.c_str());
        } else {
            ImGui::Text("%s", filename.c_str());
        }
    } else {
        ImGui::Text("Untitled%s", m_has_unsaved_changes ? "*" : "");
    }

    ImGui::Separator();
}

void AnimatorPanel::render_parameters_panel() {
    ImGui::Text(ICON_FA_SLIDERS " Parameters");
    ImGui::Separator();

    if (ImGui::Button(ICON_FA_PLUS " Add Parameter")) {
        m_show_new_param_dialog = true;
    }

    ImGui::Separator();

    // List parameters
    for (size_t i = 0; i < m_controller.parameters.size(); ++i) {
        auto& param = m_controller.parameters[i];

        ImGui::PushID(static_cast<int>(i));

        // Parameter type icon
        const char* icon = "";
        switch (param.type) {
            case engine::animation::ParameterType::Bool: icon = ICON_FA_TOGGLE_ON; break;
            case engine::animation::ParameterType::Int: icon = ICON_FA_HASHTAG; break;
            case engine::animation::ParameterType::Float: icon = ICON_FA_PERCENT; break;
            case engine::animation::ParameterType::Trigger: icon = ICON_FA_BOLT; break;
        }

        ImGui::Text("%s", icon);
        ImGui::SameLine();

        // Parameter name and value
        ImGui::Text("%s", param.name.c_str());

        // Delete button
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
        if (ImGui::Button(ICON_FA_TRASH_CAN)) {
            delete_parameter(param.name);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }
}

void AnimatorPanel::render_node_editor() {
    // Draw toolbar at the top first (before canvas so it gets input priority)
    ImGui::Text(ICON_FA_MAGNIFYING_GLASS " %.0f%%", m_zoom_level * 100.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        m_zoom_level = 1.0f;
        m_pan_x = 50.0f;
        m_pan_y = 50.0f;
    }
    if (m_waiting_for_transition_target) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Click on target state (ESC to cancel)");
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    // Reserve canvas area
    ImGui::InvisibleButton("canvas", canvas_size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool canvas_hovered = ImGui::IsItemHovered();
    bool canvas_active = ImGui::IsItemActive();

    // Handle zoom with mouse wheel
    if (canvas_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            ImVec2 mouse_canvas = ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

            // Get mouse position in grid space before zoom
            float grid_x = (mouse_canvas.x - m_pan_x) / m_zoom_level;
            float grid_y = (mouse_canvas.y - m_pan_y) / m_zoom_level;

            // Apply zoom
            m_zoom_level += wheel * 0.1f;
            m_zoom_level = std::clamp(m_zoom_level, MIN_ZOOM, MAX_ZOOM);

            // Adjust pan to keep mouse position fixed in grid space
            m_pan_x = mouse_canvas.x - grid_x * m_zoom_level;
            m_pan_y = mouse_canvas.y - grid_y * m_zoom_level;
        }
    }

    // Handle panning with middle mouse button or Alt+left mouse button
    if (canvas_active && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
        (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::GetIO().KeyAlt))) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_pan_x += delta.x;
        m_pan_y += delta.y;
    }

    // Clip to canvas
    draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    // Draw grid
    float grid_step = GRID_SIZE * m_zoom_level;
    float grid_offset_x = fmodf(m_pan_x, grid_step);
    float grid_offset_y = fmodf(m_pan_y, grid_step);

    for (float x = grid_offset_x; x < canvas_size.x; x += grid_step) {
        int grid_line = static_cast<int>((x - m_pan_x) / grid_step);
        ImU32 color = (grid_line % 5 == 0) ? COLOR_GRID_LINES_MAJOR : COLOR_GRID_LINES;
        draw_list->AddLine(
            ImVec2(canvas_pos.x + x, canvas_pos.y),
            ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y),
            color);
    }
    for (float y = grid_offset_y; y < canvas_size.y; y += grid_step) {
        int grid_line = static_cast<int>((y - m_pan_y) / grid_step);
        ImU32 color = (grid_line % 5 == 0) ? COLOR_GRID_LINES_MAJOR : COLOR_GRID_LINES;
        draw_list->AddLine(
            ImVec2(canvas_pos.x, canvas_pos.y + y),
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y),
            color);
    }

    // Clear node bounds - will be populated as we render nodes
    m_node_bounds.clear();

    // Helper lambda to convert grid position to screen position
    auto grid_to_screen = [&](float gx, float gy) -> ImVec2 {
        return ImVec2(
            canvas_pos.x + gx * m_zoom_level + m_pan_x,
            canvas_pos.y + gy * m_zoom_level + m_pan_y);
    };

    // Helper lambda to convert screen position to grid position
    auto screen_to_grid = [&](float sx, float sy) -> ImVec2 {
        return ImVec2(
            (sx - canvas_pos.x - m_pan_x) / m_zoom_level,
            (sy - canvas_pos.y - m_pan_y) / m_zoom_level);
    };

    // Render "Any State" node
    {
        ImVec2 node_pos = grid_to_screen(m_any_state_pos_x, m_any_state_pos_y);
        float node_w = NODE_WIDTH * m_zoom_level;
        float title_h = NODE_TITLE_HEIGHT * m_zoom_level;
        float total_h = title_h + 10.0f * m_zoom_level;

        // Store bounds
        m_node_bounds[ANY_STATE_NODE_ID] = {node_pos.x, node_pos.y, node_w, total_h};

        bool is_selected = (m_selected_node_id == ANY_STATE_NODE_ID);

        // Draw node background
        draw_list->AddRectFilled(node_pos, ImVec2(node_pos.x + node_w, node_pos.y + total_h),
            COLOR_NODE_BG, NODE_ROUNDING * m_zoom_level);

        // Draw title bar
        draw_list->AddRectFilled(node_pos, ImVec2(node_pos.x + node_w, node_pos.y + title_h),
            COLOR_NODE_TITLE_ANY, NODE_ROUNDING * m_zoom_level, ImDrawFlags_RoundCornersTop);

        // Draw border
        ImU32 border_color = is_selected ? COLOR_NODE_BORDER_SELECTED : COLOR_NODE_BORDER;
        draw_list->AddRect(node_pos, ImVec2(node_pos.x + node_w, node_pos.y + total_h),
            border_color, NODE_ROUNDING * m_zoom_level, 0, 2.0f * m_zoom_level);

        // Draw title text
        const char* title = "Any State";
        ImVec2 text_size = ImGui::CalcTextSize(title);
        float text_scale = m_zoom_level;
        ImVec2 text_pos(node_pos.x + (node_w - text_size.x * text_scale) / 2,
                        node_pos.y + (title_h - text_size.y * text_scale) / 2);
        draw_list->AddText(nullptr, ImGui::GetFontSize() * text_scale, text_pos, COLOR_NODE_TEXT, title);
    }

    // Render state nodes
    for (auto& state : m_controller.states) {
        int node_id = get_node_id(state.name);
        if (node_id < 0) {
            node_id = allocate_node_id(state.name);
        }

        ImVec2 node_pos = grid_to_screen(state.editor_position.x, state.editor_position.y);
        float node_w = NODE_WIDTH * m_zoom_level;
        float title_h = NODE_TITLE_HEIGHT * m_zoom_level;
        float content_h = NODE_CONTENT_HEIGHT * m_zoom_level;
        float total_h = title_h + content_h;

        // Store bounds
        m_node_bounds[node_id] = {node_pos.x, node_pos.y, node_w, total_h};

        bool is_selected = (m_selected_node_id == node_id);
        bool is_default = (state.name == m_controller.default_state);

        // Draw node background
        draw_list->AddRectFilled(node_pos, ImVec2(node_pos.x + node_w, node_pos.y + total_h),
            COLOR_NODE_BG, NODE_ROUNDING * m_zoom_level);

        // Draw title bar
        ImU32 title_color = is_default ? COLOR_NODE_TITLE_DEFAULT : COLOR_NODE_TITLE;
        draw_list->AddRectFilled(node_pos, ImVec2(node_pos.x + node_w, node_pos.y + title_h),
            title_color, NODE_ROUNDING * m_zoom_level, ImDrawFlags_RoundCornersTop);

        // Draw border
        ImU32 border_color = is_selected ? COLOR_NODE_BORDER_SELECTED : COLOR_NODE_BORDER;
        draw_list->AddRect(node_pos, ImVec2(node_pos.x + node_w, node_pos.y + total_h),
            border_color, NODE_ROUNDING * m_zoom_level, 0, 2.0f * m_zoom_level);

        // Draw title text
        std::string title = state.name;
        if (is_default) title += " (default)";
        ImVec2 text_size = ImGui::CalcTextSize(title.c_str());
        float text_scale = m_zoom_level;
        // Clamp title to node width
        float max_text_w = node_w - 8.0f * m_zoom_level;
        ImVec2 text_pos(node_pos.x + 4.0f * m_zoom_level,
                        node_pos.y + (title_h - text_size.y * text_scale) / 2);
        draw_list->PushClipRect(text_pos, ImVec2(text_pos.x + max_text_w, text_pos.y + text_size.y * text_scale + 2), true);
        draw_list->AddText(nullptr, ImGui::GetFontSize() * text_scale, text_pos, COLOR_NODE_TEXT, title.c_str());
        draw_list->PopClipRect();

        // Draw clip name
        std::string clip_text = state.clip_path.empty() ? "(no clip)" :
            state.clip_path.substr(state.clip_path.find_last_of("/\\") + 1);
        ImVec2 clip_pos(node_pos.x + 4.0f * m_zoom_level, node_pos.y + title_h + 2.0f * m_zoom_level);
        draw_list->AddText(nullptr, ImGui::GetFontSize() * text_scale * 0.9f, clip_pos, COLOR_NODE_TEXT_DIM, clip_text.c_str());
    }

    // Draw transitions
    render_custom_transitions();

    // Handle node dragging
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    bool mouse_in_canvas = mouse_pos.x >= canvas_pos.x && mouse_pos.x < canvas_pos.x + canvas_size.x &&
                           mouse_pos.y >= canvas_pos.y && mouse_pos.y < canvas_pos.y + canvas_size.y;

    // Start dragging on left mouse button (not when Alt is held for panning)
    if (mouse_in_canvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt) {
        int clicked_node = get_node_at_position(mouse_pos);

        if (ImGui::GetIO().KeyShift && clicked_node >= 0) {
            // Shift+click: Start drag-to-create transition
            m_dragging_transition = true;
            m_drag_from_node_id = clicked_node;
            m_drag_start_pos = mouse_pos;
            m_drag_current_pos = mouse_pos;
        } else if (clicked_node >= 0) {
            // Regular click: Select and start dragging
            m_selected_node_id = clicked_node;
            if (clicked_node == ANY_STATE_NODE_ID) {
                m_selected_state = engine::animation::ANY_STATE;
            } else {
                m_selected_state = get_state_name(clicked_node);
            }
            m_selected_transition_index = -1;
            m_dragging_node = true;
            m_drag_node_id = clicked_node;
            m_drag_offset = ImVec2(mouse_pos.x - m_node_bounds[clicked_node].x,
                                   mouse_pos.y - m_node_bounds[clicked_node].y);
        } else if (!m_waiting_for_transition_target) {
            // Clicked on empty space - check for transition click
            int clicked_transition = get_transition_at_position(mouse_pos);
            if (clicked_transition >= 0) {
                m_selected_transition_index = clicked_transition;
                m_selected_node_id = -1;
                m_selected_state.clear();
            } else {
                // Deselect
                m_selected_node_id = -1;
                m_selected_state.clear();
                m_selected_transition_index = -1;
            }
        }
    }

    // Handle node dragging motion
    if (m_dragging_node && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 grid_pos = screen_to_grid(mouse_pos.x - m_drag_offset.x, mouse_pos.y - m_drag_offset.y);

        if (m_drag_node_id == ANY_STATE_NODE_ID) {
            m_any_state_pos_x = grid_pos.x;
            m_any_state_pos_y = grid_pos.y;
        } else {
            std::string state_name = get_state_name(m_drag_node_id);
            auto* state = m_controller.get_state(state_name);
            if (state) {
                state->editor_position.x = grid_pos.x;
                state->editor_position.y = grid_pos.y;
                mark_dirty();
            }
        }
    }

    // End node dragging
    if (m_dragging_node && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_dragging_node = false;
        m_drag_node_id = -1;
    }

    // Handle drag-to-create transition
    if (m_dragging_transition) {
        m_drag_current_pos = mouse_pos;

        // Draw the in-progress transition line
        auto it = m_node_bounds.find(m_drag_from_node_id);
        if (it != m_node_bounds.end()) {
            const auto& bounds = it->second;
            ImVec2 from_center(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
            draw_transition_arrow(draw_list, from_center, m_drag_current_pos, IM_COL32(200, 200, 200, 200), 2.0f * m_zoom_level, false);
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            int target_node = get_node_at_position(mouse_pos);
            if (target_node >= 0 && target_node != m_drag_from_node_id && target_node != ANY_STATE_NODE_ID) {
                // Create the transition
                std::string from_state;
                if (m_drag_from_node_id == ANY_STATE_NODE_ID) {
                    from_state = engine::animation::ANY_STATE;
                } else {
                    from_state = get_state_name(m_drag_from_node_id);
                }
                std::string to_state = get_state_name(target_node);

                if (!from_state.empty() && !to_state.empty()) {
                    create_transition(from_state, to_state);
                }
            }
            m_dragging_transition = false;
            m_drag_from_node_id = -1;
        }
    }

    // Handle "waiting for transition target" mode
    if (m_waiting_for_transition_target) {
        // Draw a line from source node to mouse cursor
        auto it = m_node_bounds.find(m_transition_source_node_id);
        if (it != m_node_bounds.end()) {
            const auto& bounds = it->second;
            ImVec2 from_center(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
            draw_transition_arrow(draw_list, from_center, mouse_pos, IM_COL32(100, 200, 100, 200), 2.0f * m_zoom_level, false);
        }

        // Handle click to create transition
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            int target_node = get_node_at_position(mouse_pos);
            if (target_node >= 0 && target_node != m_transition_source_node_id && target_node != ANY_STATE_NODE_ID) {
                // Create the transition
                std::string from_state;
                if (m_transition_source_node_id == ANY_STATE_NODE_ID) {
                    from_state = engine::animation::ANY_STATE;
                } else {
                    from_state = get_state_name(m_transition_source_node_id);
                }
                std::string to_state = get_state_name(target_node);

                if (!from_state.empty() && !to_state.empty()) {
                    create_transition(from_state, to_state);
                }
            }
            // Always exit the mode on click
            m_waiting_for_transition_target = false;
            m_transition_source_node_id = -1;
        }

        // Cancel with ESC or right-click
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_waiting_for_transition_target = false;
            m_transition_source_node_id = -1;
        }
    }

    // Right-click context menu
    if (mouse_in_canvas && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !m_waiting_for_transition_target) {
        int clicked_node = get_node_at_position(mouse_pos);
        if (clicked_node >= 0) {
            // Right-clicked on a node
            m_show_node_context_menu = true;
            m_context_menu_node_id = clicked_node;
        } else {
            // Right-clicked on empty space
            m_show_context_menu = true;
            ImVec2 grid_pos = screen_to_grid(mouse_pos.x, mouse_pos.y);
            m_context_menu_x = grid_pos.x;
            m_context_menu_y = grid_pos.y;
        }
    }

    draw_list->PopClipRect();

    // Empty space context menu
    if (m_show_context_menu) {
        ImGui::OpenPopup("EmptySpaceContextMenu");
        m_show_context_menu = false;
    }
    if (ImGui::BeginPopup("EmptySpaceContextMenu")) {
        if (ImGui::MenuItem("Create State")) {
            m_show_new_state_dialog = true;
        }
        ImGui::EndPopup();
    }

    // Node context menu
    if (m_show_node_context_menu) {
        ImGui::OpenPopup("NodeContextMenu");
        m_show_node_context_menu = false;
    }
    if (ImGui::BeginPopup("NodeContextMenu")) {
        if (ImGui::MenuItem("Create Transition")) {
            m_waiting_for_transition_target = true;
            m_transition_source_node_id = m_context_menu_node_id;
        }
        ImGui::Separator();
        // Don't show delete for "Any State" node
        if (m_context_menu_node_id != ANY_STATE_NODE_ID) {
            if (ImGui::MenuItem("Set as Default")) {
                std::string state_name = get_state_name(m_context_menu_node_id);
                if (!state_name.empty()) {
                    m_controller.default_state = state_name;
                    mark_dirty();
                }
            }
            if (ImGui::MenuItem("Delete State")) {
                std::string state_name = get_state_name(m_context_menu_node_id);
                if (!state_name.empty()) {
                    delete_state(state_name);
                }
            }
        }
        ImGui::EndPopup();
    }

    // Handle delete key
    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (!m_selected_state.empty() && m_selected_state != engine::animation::ANY_STATE) {
            delete_state(m_selected_state);
        } else if (m_selected_transition_index >= 0) {
            delete_transition(m_selected_transition_index);
        }
    }
}

void AnimatorPanel::render_properties_panel() {
    ImGui::Text(ICON_FA_GEAR " Properties");
    ImGui::Separator();

    if (!m_selected_state.empty()) {
        // Check if "Any" state is selected
        if (m_selected_state == engine::animation::ANY_STATE) {
            ImGui::Text("Any State");
            ImGui::Separator();
            ImGui::TextWrapped("The 'Any State' node allows creating transitions that can trigger from any state in the state machine.");
            ImGui::Spacing();
            ImGui::TextDisabled("This node cannot be deleted.");
        } else {
            auto* state = m_controller.get_state(m_selected_state);
            if (state) {
                render_state_properties(*state);
            }
        }
    } else if (m_selected_transition_index >= 0 &&
               m_selected_transition_index < static_cast<int>(m_controller.transitions.size())) {
        auto& transition = m_controller.transitions[m_selected_transition_index];
        render_transition_properties(transition);
    } else {
        ImGui::TextDisabled("Select a state or transition");
    }
}

void AnimatorPanel::render_state_properties(engine::animation::AnimatorState& state) {
    ImGui::Text("State: %s", state.name.c_str());
    ImGui::Separator();

    // Rename
    char name_buf[64];
    strncpy(name_buf, state.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        rename_state(state.name, name_buf);
    }

    // Clip path with asset picker
    ImGui::Text("Clip");
    AssetPickerConfig clip_config;
    clip_config.popup_id = "StateClipPicker";
    clip_config.title = "Select Animation Clip (.anim)";
    clip_config.extensions = ANIM_EXTENSIONS;
    clip_config.empty_message = "No .anim files found in Assets folder";
    clip_config.clear_button_label = "Clear Selection";
    clip_config.button_tooltip = "Click to select an animation clip file";

    const auto& project_path = m_context.scene_state().project_path();
    auto clip_result = AssetPicker::draw_button(state.clip_path, project_path, clip_config);
    if (clip_result.changed) {
        state.clip_path = clip_result.selected_path;
        mark_dirty();
    }

    // Speed
    if (ImGui::DragFloat("Speed", &state.speed, 0.01f, 0.0f, 10.0f)) {
        mark_dirty();
    }

    ImGui::Separator();

    // Set as default
    bool is_default = (state.name == m_controller.default_state);
    if (ImGui::Checkbox("Default State", &is_default)) {
        if (is_default) {
            m_controller.default_state = state.name;
            mark_dirty();
        }
    }

    ImGui::Separator();

    // Delete button
    if (ImGui::Button(ICON_FA_TRASH_CAN " Delete State")) {
        delete_state(state.name);
    }
}

void AnimatorPanel::render_transition_properties(engine::animation::StateTransition& transition) {
    // Display "Any State" instead of "__any__" for better readability
    const char* from_display = transition.is_any_state_transition() ? "Any State" : transition.from_state.c_str();
    ImGui::Text("Transition: %s -> %s", from_display, transition.to_state.c_str());
    ImGui::Separator();

    // Blend duration
    if (ImGui::DragFloat("Blend Duration", &transition.blend_duration, 0.01f, 0.0f, 5.0f)) {
        mark_dirty();
    }

    // Exit time
    if (ImGui::Checkbox("Has Exit Time", &transition.has_exit_time)) {
        mark_dirty();
    }
    if (transition.has_exit_time) {
        if (ImGui::DragFloat("Exit Time", &transition.exit_time, 0.01f, 0.0f, 1.0f)) {
            mark_dirty();
        }
    }

    ImGui::Separator();
    ImGui::Text("Conditions:");

    // Add condition button
    if (ImGui::Button(ICON_FA_PLUS " Add Condition")) {
        engine::animation::TransitionCondition cond;
        if (!m_controller.parameters.empty()) {
            cond.parameter_name = m_controller.parameters[0].name;
        }
        transition.conditions.push_back(cond);
        mark_dirty();
    }

    // List conditions
    for (int i = 0; i < static_cast<int>(transition.conditions.size()); ++i) {
        ImGui::PushID(i);
        render_condition_editor(transition.conditions[i], i);

        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH_CAN)) {
            transition.conditions.erase(transition.conditions.begin() + i);
            mark_dirty();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    ImGui::Separator();

    // Delete transition button
    if (ImGui::Button(ICON_FA_TRASH_CAN " Delete Transition")) {
        delete_transition(m_selected_transition_index);
    }
}

void AnimatorPanel::render_condition_editor(engine::animation::TransitionCondition& condition, int index) {
    ImGui::PushItemWidth(80);

    // Parameter dropdown
    if (ImGui::BeginCombo("##param", condition.parameter_name.c_str())) {
        for (const auto& param : m_controller.parameters) {
            bool selected = (param.name == condition.parameter_name);
            if (ImGui::Selectable(param.name.c_str(), selected)) {
                condition.parameter_name = param.name;
                mark_dirty();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Operator dropdown - order must match CompareOp enum:
    // Equal=0, NotEqual=1, Greater=2, Less=3, GreaterEqual=4, LessEqual=5
    const char* op_names[] = { "==", "!=", ">", "<", ">=", "<=" };
    int op_index = static_cast<int>(condition.op);
    if (ImGui::BeginCombo("##op", op_names[op_index])) {
        for (int i = 0; i < 6; ++i) {
            if (ImGui::Selectable(op_names[i], i == op_index)) {
                condition.op = static_cast<engine::animation::CompareOp>(i);
                mark_dirty();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Value input based on parameter type
    const engine::animation::AnimatorParameter* param = nullptr;
    for (const auto& p : m_controller.parameters) {
        if (p.name == condition.parameter_name) {
            param = &p;
            break;
        }
    }

    if (param) {
        switch (param->type) {
            case engine::animation::ParameterType::Bool:
            case engine::animation::ParameterType::Trigger: {
                bool val = std::holds_alternative<bool>(condition.value) ? std::get<bool>(condition.value) : false;
                if (ImGui::Checkbox("##val", &val)) {
                    condition.value = val;
                    mark_dirty();
                }
                break;
            }
            case engine::animation::ParameterType::Int: {
                int val = std::holds_alternative<int>(condition.value) ? std::get<int>(condition.value) : 0;
                if (ImGui::InputInt("##val", &val)) {
                    condition.value = val;
                    mark_dirty();
                }
                break;
            }
            case engine::animation::ParameterType::Float: {
                float val = std::holds_alternative<float>(condition.value) ? std::get<float>(condition.value) : 0.0f;
                if (ImGui::InputFloat("##val", &val)) {
                    condition.value = val;
                    mark_dirty();
                }
                break;
            }
        }
    }

    ImGui::PopItemWidth();
}

int AnimatorPanel::get_node_id(const std::string& state_name) const {
    auto it = m_state_to_node_id.find(state_name);
    if (it != m_state_to_node_id.end()) {
        return it->second;
    }
    return -1;
}

std::string AnimatorPanel::get_state_name(int node_id) const {
    auto it = m_node_id_to_state.find(node_id);
    if (it != m_node_id_to_state.end()) {
        return it->second;
    }
    return "";
}

int AnimatorPanel::allocate_node_id(const std::string& state_name) {
    int id = m_next_node_id++;
    m_state_to_node_id[state_name] = id;
    m_node_id_to_state[id] = state_name;
    return id;
}

int AnimatorPanel::allocate_link_id() {
    return m_next_link_id++;
}

void AnimatorPanel::create_state(const std::string& name, float x, float y) {
    // Check if name already exists
    if (m_controller.get_state(name)) {
        engine::Logger::instance().warning("Animator", "State '%s' already exists", name.c_str());
        return;
    }

    engine::animation::AnimatorState state;
    state.name = name;
    state.editor_position.x = x;
    state.editor_position.y = y;
    m_controller.states.push_back(std::move(state));

    // Set as default if first state
    if (m_controller.states.size() == 1) {
        m_controller.default_state = name;
    }

    mark_dirty();
}

void AnimatorPanel::delete_state(const std::string& name) {
    // Remove from states
    m_controller.states.erase(
        std::remove_if(m_controller.states.begin(), m_controller.states.end(),
            [&](const auto& s) { return s.name == name; }),
        m_controller.states.end());

    // Remove transitions involving this state
    m_controller.transitions.erase(
        std::remove_if(m_controller.transitions.begin(), m_controller.transitions.end(),
            [&](const auto& t) { return t.from_state == name || t.to_state == name; }),
        m_controller.transitions.end());

    // Clear node mapping
    int node_id = get_node_id(name);
    if (node_id >= 0) {
        m_state_to_node_id.erase(name);
        m_node_id_to_state.erase(node_id);
    }

    // Update default state if needed
    if (m_controller.default_state == name && !m_controller.states.empty()) {
        m_controller.default_state = m_controller.states[0].name;
    }

    m_selected_state.clear();
    m_selected_node_id = -1;
    mark_dirty();
}

void AnimatorPanel::rename_state(const std::string& old_name, const std::string& new_name) {
    if (old_name == new_name) return;

    // Check if new name already exists
    if (m_controller.get_state(new_name)) {
        engine::Logger::instance().warning("Animator", "State '%s' already exists", new_name.c_str());
        return;
    }

    // Update state
    auto* state = m_controller.get_state(old_name);
    if (state) {
        state->name = new_name;
    }

    // Update transitions
    for (auto& trans : m_controller.transitions) {
        if (trans.from_state == old_name) trans.from_state = new_name;
        if (trans.to_state == old_name) trans.to_state = new_name;
    }

    // Update default state
    if (m_controller.default_state == old_name) {
        m_controller.default_state = new_name;
    }

    // Update node mapping
    int node_id = get_node_id(old_name);
    if (node_id >= 0) {
        m_state_to_node_id.erase(old_name);
        m_state_to_node_id[new_name] = node_id;
        m_node_id_to_state[node_id] = new_name;
    }

    m_selected_state = new_name;
    mark_dirty();
}

void AnimatorPanel::create_transition(const std::string& from, const std::string& to) {
    engine::animation::StateTransition trans;
    trans.from_state = from;
    trans.to_state = to;
    m_controller.transitions.push_back(std::move(trans));
    mark_dirty();
}

void AnimatorPanel::delete_transition(size_t index) {
    if (index < m_controller.transitions.size()) {
        m_controller.transitions.erase(m_controller.transitions.begin() + index);
        m_selected_transition_index = -1;
        m_selected_link_id = -1;
        mark_dirty();
    }
}

void AnimatorPanel::create_parameter(const std::string& name, engine::animation::ParameterType type) {
    engine::animation::AnimatorParameter param;
    param.name = name;
    param.type = type;
    m_controller.parameters.push_back(std::move(param));
    mark_dirty();
}

void AnimatorPanel::delete_parameter(const std::string& name) {
    m_controller.parameters.erase(
        std::remove_if(m_controller.parameters.begin(), m_controller.parameters.end(),
            [&](const auto& p) { return p.name == name; }),
        m_controller.parameters.end());
    mark_dirty();
}

void AnimatorPanel::open_controller(const std::string& path) {
    auto* runtime = m_context.runtime();
    if (!runtime || !runtime->engine()) {
        engine::Logger::instance().error("Animator", "Cannot open controller: engine not available");
        return;
    }
    auto& assets = runtime->engine()->assets();
    auto handle = assets.load<engine::animation::AnimatorController>(path);
    const auto* controller = assets.get(handle);

    if (controller) {
        m_controller = *controller;
        m_current_path = path;
        m_has_unsaved_changes = false;

        // Clear node mappings
        m_state_to_node_id.clear();
        m_node_id_to_state.clear();
        m_positioned_nodes.clear();
        m_next_node_id = 100;  // Start at 100, reserve lower IDs for special nodes
        m_any_state_positioned = false;

        // Reset pan/zoom to reasonable defaults
        m_pan_x = 50.0f;
        m_pan_y = 50.0f;
        m_zoom_level = 1.0f;

        // Clear selection
        m_selected_state.clear();
        m_selected_node_id = -1;
        m_selected_link_id = -1;
        m_selected_transition_index = -1;

        engine::Logger::instance().info("Animator", "Opened controller: %s", path.c_str());
    } else {
        engine::Logger::instance().error("Animator", "Failed to load controller: %s", path.c_str());
    }
}

void AnimatorPanel::new_controller() {
    m_controller = engine::animation::AnimatorController();
    m_current_path.clear();
    m_has_unsaved_changes = false;

    // Clear node mappings
    m_state_to_node_id.clear();
    m_node_id_to_state.clear();
    m_positioned_nodes.clear();
    m_next_node_id = 100;  // Start at 100, reserve lower IDs for special nodes
    m_any_state_positioned = false;

    // Reset pan/zoom
    m_pan_x = 50.0f;
    m_pan_y = 50.0f;
    m_zoom_level = 1.0f;

    // Clear selection
    m_selected_state.clear();
    m_selected_node_id = -1;
    m_selected_link_id = -1;
    m_selected_transition_index = -1;
}

bool AnimatorPanel::save_controller() {
    if (m_current_path.empty()) {
        // Show save dialog, default to project's Assets folder
        std::string initial_dir;
        const auto& project_path = m_context.scene_state().project_path();
        if (!project_path.empty()) {
            initial_dir = (std::filesystem::path(project_path) / "Assets").string();
        }

        auto path = engine::platform::save_file_dialog(
            "Save Animator Controller",
            {{"Animator Controller (*.animstate)", "*.animstate"}},
            ".animstate",
            initial_dir);
        if (path.empty()) return false;
        return save_controller_as(path);
    }
    return save_controller_as(m_current_path);
}

bool AnimatorPanel::save_controller_as(const std::string& path) {
    bool success = engine::animation::save_controller(m_controller, path);
    if (success) {
        m_current_path = path;
        m_has_unsaved_changes = false;
        engine::Logger::instance().info("Animator", "Saved controller: %s", path.c_str());
    } else {
        engine::Logger::instance().error("Animator", "Failed to save controller: %s", path.c_str());
    }
    return success;
}

void AnimatorPanel::show_open_dialog() {
    auto path = engine::platform::open_file_dialog(
        "Open Animator Controller",
        {{"Animator Controller (*.animstate)", "*.animstate"}});
    if (!path.empty()) {
        open_controller(path);
    }
}

void AnimatorPanel::show_save_as_dialog() {
    // Show save dialog, default to project's Assets folder
    std::string initial_dir;
    const auto& project_path = m_context.scene_state().project_path();
    if (!project_path.empty()) {
        initial_dir = (std::filesystem::path(project_path) / "Assets").string();
    }

    auto path = engine::platform::save_file_dialog(
        "Save Animator Controller As",
        {{"Animator Controller (*.animstate)", "*.animstate"}},
        ".animstate",
        initial_dir);
    if (!path.empty()) {
        save_controller_as(path);
    }
}

void AnimatorPanel::mark_dirty() {
    m_has_unsaved_changes = true;
}

AnimatorPanel::CardinalDirection AnimatorPanel::get_connection_direction(int from_node, int to_node) const {
    auto from_it = m_node_bounds.find(from_node);
    auto to_it = m_node_bounds.find(to_node);

    if (from_it == m_node_bounds.end() || to_it == m_node_bounds.end()) {
        return CardinalDirection::East;  // Default
    }

    const auto& from = from_it->second;
    const auto& to = to_it->second;

    // Calculate centers
    float from_cx = from.x + from.width / 2;
    float from_cy = from.y + from.height / 2;
    float to_cx = to.x + to.width / 2;
    float to_cy = to.y + to.height / 2;

    float dx = to_cx - from_cx;
    float dy = to_cy - from_cy;

    // Determine primary direction based on which axis has larger difference
    if (std::abs(dx) > std::abs(dy)) {
        return dx > 0 ? CardinalDirection::East : CardinalDirection::West;
    } else {
        return dy > 0 ? CardinalDirection::South : CardinalDirection::North;
    }
}

ImVec2 AnimatorPanel::get_connection_point(int node_id, CardinalDirection dir) const {
    auto it = m_node_bounds.find(node_id);
    if (it == m_node_bounds.end()) {
        return ImVec2(0, 0);
    }

    const auto& bounds = it->second;
    float cx = bounds.x + bounds.width / 2;
    float cy = bounds.y + bounds.height / 2;

    switch (dir) {
        case CardinalDirection::North:
            return ImVec2(cx, bounds.y);
        case CardinalDirection::South:
            return ImVec2(cx, bounds.y + bounds.height);
        case CardinalDirection::East:
            return ImVec2(bounds.x + bounds.width, cy);
        case CardinalDirection::West:
            return ImVec2(bounds.x, cy);
    }
    return ImVec2(cx, cy);
}

void AnimatorPanel::draw_transition_arrow(ImDrawList* draw_list, ImVec2 from, ImVec2 to, ImU32 color, float thickness, bool is_selected) {
    // Draw the line
    draw_list->AddLine(from, to, color, thickness);

    // Calculate arrow head
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;

    // Normalize
    dx /= len;
    dy /= len;

    // Arrow head size
    float arrow_size = 8.0f * m_zoom_level;
    if (is_selected) arrow_size *= 1.2f;

    // Arrow head position (slightly back from the end point)
    ImVec2 arrow_tip = to;

    // Perpendicular vector
    float px = -dy;
    float py = dx;

    // Arrow head points
    ImVec2 p1(arrow_tip.x - dx * arrow_size + px * arrow_size * 0.5f,
              arrow_tip.y - dy * arrow_size + py * arrow_size * 0.5f);
    ImVec2 p2(arrow_tip.x - dx * arrow_size - px * arrow_size * 0.5f,
              arrow_tip.y - dy * arrow_size - py * arrow_size * 0.5f);

    // Draw filled triangle
    draw_list->AddTriangleFilled(arrow_tip, p1, p2, color);
}

void AnimatorPanel::render_custom_transitions() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (size_t i = 0; i < m_controller.transitions.size(); ++i) {
        const auto& trans = m_controller.transitions[i];

        // Get node IDs
        int from_node = (trans.from_state == engine::animation::ANY_STATE)
            ? ANY_STATE_NODE_ID : get_node_id(trans.from_state);
        int to_node = get_node_id(trans.to_state);

        if (from_node < 0 || to_node < 0) continue;

        // Get connection directions based on relative positions
        CardinalDirection from_dir = get_connection_direction(from_node, to_node);
        CardinalDirection to_dir = get_connection_direction(to_node, from_node);

        ImVec2 from_pos = get_connection_point(from_node, from_dir);
        ImVec2 to_pos = get_connection_point(to_node, to_dir);

        // Check if there's a reverse transition (bidirectional)
        bool has_reverse = false;
        for (size_t j = 0; j < m_controller.transitions.size(); ++j) {
            if (j == i) continue;
            const auto& other = m_controller.transitions[j];
            if (other.from_state == trans.to_state && other.to_state == trans.from_state) {
                has_reverse = true;
                break;
            }
        }

        // Offset bidirectional transitions so they don't overlap
        if (has_reverse) {
            // Calculate perpendicular offset
            float dx = to_pos.x - from_pos.x;
            float dy = to_pos.y - from_pos.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0) {
                float offset = 4.0f * m_zoom_level;
                float px = -dy / len * offset;
                float py = dx / len * offset;
                from_pos.x += px;
                from_pos.y += py;
                to_pos.x += px;
                to_pos.y += py;
            }
        }

        // Determine color
        bool is_selected = (static_cast<int>(i) == m_selected_transition_index);
        ImU32 color;
        if (is_selected) {
            color = COLOR_TRANSITION_SELECTED;
        } else if (trans.from_state == engine::animation::ANY_STATE) {
            color = COLOR_TRANSITION_ANY;
        } else {
            color = COLOR_TRANSITION;
        }

        float thickness = is_selected ? 3.0f * m_zoom_level : 2.0f * m_zoom_level;

        draw_transition_arrow(draw_list, from_pos, to_pos, color, thickness, is_selected);
    }
}

int AnimatorPanel::get_node_at_position(const ImVec2& pos) const {
    for (const auto& [node_id, bounds] : m_node_bounds) {
        if (pos.x >= bounds.x && pos.x <= bounds.x + bounds.width &&
            pos.y >= bounds.y && pos.y <= bounds.y + bounds.height) {
            return node_id;
        }
    }
    return -1;
}

int AnimatorPanel::get_transition_at_position(const ImVec2& pos) const {
    float min_dist = 8.0f * m_zoom_level;  // Click threshold
    int clicked_transition = -1;

    for (size_t i = 0; i < m_controller.transitions.size(); ++i) {
        const auto& trans = m_controller.transitions[i];

        int from_node = (trans.from_state == engine::animation::ANY_STATE)
            ? ANY_STATE_NODE_ID : get_node_id(trans.from_state);
        int to_node = get_node_id(trans.to_state);

        if (from_node < 0 || to_node < 0) continue;

        auto from_it = m_node_bounds.find(from_node);
        auto to_it = m_node_bounds.find(to_node);
        if (from_it == m_node_bounds.end() || to_it == m_node_bounds.end()) continue;

        CardinalDirection from_dir = get_connection_direction(from_node, to_node);
        CardinalDirection to_dir = get_connection_direction(to_node, from_node);

        ImVec2 from_pos = get_connection_point(from_node, from_dir);
        ImVec2 to_pos = get_connection_point(to_node, to_dir);

        // Check for bidirectional offset
        bool has_reverse = false;
        for (const auto& other : m_controller.transitions) {
            if (other.from_state == trans.to_state && other.to_state == trans.from_state) {
                has_reverse = true;
                break;
            }
        }

        if (has_reverse) {
            float dx = to_pos.x - from_pos.x;
            float dy = to_pos.y - from_pos.y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0) {
                float offset = 4.0f * m_zoom_level;
                float px = -dy / len * offset;
                float py = dx / len * offset;
                from_pos.x += px;
                from_pos.y += py;
                to_pos.x += px;
                to_pos.y += py;
            }
        }

        // Distance from point to line segment
        float dx = to_pos.x - from_pos.x;
        float dy = to_pos.y - from_pos.y;
        float t = std::clamp(((pos.x - from_pos.x) * dx + (pos.y - from_pos.y) * dy) / (dx * dx + dy * dy + engine::EPSILON), 0.0f, 1.0f);
        float closest_x = from_pos.x + t * dx;
        float closest_y = from_pos.y + t * dy;
        float dist = sqrtf((pos.x - closest_x) * (pos.x - closest_x) + (pos.y - closest_y) * (pos.y - closest_y));

        if (dist < min_dist) {
            min_dist = dist;
            clicked_transition = static_cast<int>(i);
        }
    }

    return clicked_transition;
}

} // namespace editor
