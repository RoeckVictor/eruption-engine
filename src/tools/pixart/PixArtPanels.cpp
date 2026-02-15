#include "PixArtApp.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace pixart {

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void PixArtApp::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New...", "Ctrl+N")) {
                try_new_document();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                try_open_file();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                try_save_document();
            }
            if (ImGui::MenuItem("Save As...")) {
                try_save_document_as();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                try_exit();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_undo.can_undo())) {
                if (m_undo.undo(m_doc, &m_active_layer)) {
                    m_canvas_dirty = true;
                    m_has_unsaved_changes = true;
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_undo.can_redo())) {
                if (m_undo.redo(m_doc, &m_active_layer)) {
                    m_canvas_dirty = true;
                    m_has_unsaved_changes = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Resize Grid...")) {
                m_show_resize_dialog = true;
                m_resize_width = m_doc.width();
                m_resize_height = m_doc.height();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void PixArtApp::render_toolbar() {
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse);

    const char* tool_names[] = {"Pencil", "Bucket", "Line"};
    for (int i = 0; i < 3; ++i) {
        if (i > 0) ImGui::SameLine();
        bool selected = (static_cast<int>(m_tools.active_tool) == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        if (ImGui::Button(tool_names[i], ImVec2(70, 0))) {
            m_tools.active_tool = static_cast<Tool>(i);
            m_tools.cancel();
        }
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::SliderInt("Brush", &m_tools.draw_state.brush_size, 1, 32);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    bool origin_was_active = m_setting_origin;
    if (origin_was_active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.2f, 1.0f));
    if (ImGui::Button("Origin", ImVec2(70, 0))) {
        m_setting_origin = !m_setting_origin;
    }
    if (origin_was_active) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click on the canvas to set the origin/pivot point");

    // Status info
    ImGui::Separator();
    if (m_doc.valid()) {
        ImGui::Text("Grid: %dx%d  Origin: (%d, %d)", m_doc.width(), m_doc.height(),
                     m_doc.origin_x(), m_doc.origin_y());
        ImGui::SameLine();
        ImGui::Text("| Zoom: %.0fx", m_view.zoom);
        if (m_hover_px >= 0 && m_hover_py >= 0) {
            ImGui::SameLine();
            ImGui::Text("| Pixel: (%d, %d)", m_hover_px, m_hover_py);
        }
        if (!m_current_path.empty()) {
            ImGui::SameLine();
            ImGui::Text("| %s%s", m_current_path.c_str(),
                        m_has_unsaved_changes ? " *" : "");
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Layer panel
// ---------------------------------------------------------------------------

void PixArtApp::render_layer_panel() {
    ImGui::Begin("Layers", nullptr, ImGuiWindowFlags_NoCollapse);

    if (!m_doc.valid()) {
        ImGui::Text("No document.");
        ImGui::End();
        return;
    }

    // Render layers from top to bottom (highest index = top of stack)
    for (int i = m_doc.layer_count() - 1; i >= 0; --i) {
        auto& layer = m_doc.layer(i);
        ImGui::PushID(i);

        // Eye toggle (visibility)
        bool vis = layer.visible;
        if (ImGui::SmallButton(vis ? "O" : "-")) {
            layer.visible = !layer.visible;
            m_canvas_dirty = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(vis ? "Hide layer" : "Show layer");
        ImGui::SameLine();

        // Selectable layer name
        bool selected = (i == m_active_layer);
        float name_width = ImGui::GetContentRegionAvail().x - 130.0f;
        if (name_width < 40.0f) name_width = 40.0f;
        ImGui::PushItemWidth(name_width);
        if (ImGui::Selectable(layer.name.c_str(), selected, 0, ImVec2(name_width, 0))) {
            m_active_layer = i;
        }
        ImGui::PopItemWidth();

        // Type label
        ImGui::SameLine();
        switch (layer.type) {
            case LayerType::Color: ImGui::TextDisabled("[RGBA]"); break;
            case LayerType::UInt8: ImGui::TextDisabled("[u8]"); break;
            case LayerType::Enum:  ImGui::TextDisabled("[E]"); break;
        }

        // Reorder buttons
        ImGui::SameLine();
        bool can_move_up = (i < m_doc.layer_count() - 1);
        bool can_move_down = (i > 0);
        if (!can_move_up) ImGui::BeginDisabled();
        if (ImGui::SmallButton("^")) {
            m_undo.push_layer_swap(m_doc, i, i + 1);
            m_doc.swap_layers(i, i + 1);
            if (m_active_layer == i) m_active_layer = i + 1;
            else if (m_active_layer == i + 1) m_active_layer = i;
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
        if (!can_move_up) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move up");

        ImGui::SameLine();
        if (!can_move_down) ImGui::BeginDisabled();
        if (ImGui::SmallButton("v")) {
            m_undo.push_layer_swap(m_doc, i, i - 1);
            m_doc.swap_layers(i, i - 1);
            if (m_active_layer == i) m_active_layer = i - 1;
            else if (m_active_layer == i - 1) m_active_layer = i;
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
        if (!can_move_down) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move down");

        // Delete button (not for engine-required layers)
        if (m_doc.layer_count() > 1 && !layer.engine_required) {
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                m_active_layer = m_doc.remove_layer(i, m_active_layer);
                m_canvas_dirty = true;
                ImGui::PopID();
                break;
            }
        }

        // Opacity slider
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::SliderFloat("##opacity", &layer.opacity, 0.0f, 1.0f, "%.2f")) {
            m_canvas_dirty = true;
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("+ Add Layer")) {
        m_show_add_layer_dialog = true;
        m_new_layer_name[0] = '\0';
        m_new_layer_type = 0;
        m_enum_values_buf[0] = '\0';
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Color / Value picker
// ---------------------------------------------------------------------------

void PixArtApp::render_value_picker() {
    ImGui::Begin("Picker", nullptr, ImGuiWindowFlags_NoCollapse);

    if (!m_doc.valid() || m_active_layer < 0 ||
        m_active_layer >= m_doc.layer_count()) {
        ImGui::End();
        return;
    }

    auto& layer = m_doc.layer(m_active_layer);

    switch (layer.type) {
    case LayerType::Color:
        ImGui::ColorPicker4("##color", m_tools.draw_state.color,
            ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoSidePreview |
            ImGuiColorEditFlags_PickerHueBar);
        break;

    case LayerType::UInt8:
        ImGui::Text("Value (0-255):");
        ImGui::SliderInt("##dataval", &m_tools.draw_state.data_value, 0, 255);
        break;

    case LayerType::Enum:
        ImGui::Text("Enum value:");
        if (!layer.enum_names.empty()) {
            if (m_tools.draw_state.enum_value >= static_cast<int>(layer.enum_names.size()))
                m_tools.draw_state.enum_value = 0;

            if (ImGui::BeginCombo("##enumval",
                    layer.enum_names[m_tools.draw_state.enum_value].c_str())) {
                for (int i = 0; i < static_cast<int>(layer.enum_names.size()); ++i) {
                    bool sel = (i == m_tools.draw_state.enum_value);
                    if (ImGui::Selectable(layer.enum_names[i].c_str(), sel))
                        m_tools.draw_state.enum_value = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::SliderInt("##enumidx", &m_tools.draw_state.enum_value, 0, 255);
        }
        break;
    }

    // Show pixel info under cursor
    if (m_hover_px >= 0 && m_hover_py >= 0) {
        ImGui::Separator();
        ImGui::Text("Pixel (%d, %d):", m_hover_px, m_hover_py);

        auto& active = m_doc.layer(m_active_layer);
        if (active.type == LayerType::Color) {
            uint8_t rgba[4];
            m_doc.get_pixel(m_active_layer, m_hover_px, m_hover_py, rgba);
            ImVec4 c(rgba[0] / 255.0f, rgba[1] / 255.0f,
                     rgba[2] / 255.0f, rgba[3] / 255.0f);
            ImGui::ColorButton("##hovercol", c, 0, ImVec2(24, 24));
            ImGui::SameLine();
            ImGui::Text("(%d, %d, %d, %d)", rgba[0], rgba[1], rgba[2], rgba[3]);
        } else {
            uint8_t val;
            m_doc.get_pixel(m_active_layer, m_hover_px, m_hover_py, &val);
            if (active.type == LayerType::Enum &&
                val < static_cast<uint8_t>(active.enum_names.size())) {
                ImGui::Text("%d (%s)", val, active.enum_names[val].c_str());
            } else {
                ImGui::Text("%d", val);
            }
        }

        // Eyedropper: right-click to pick color/value from canvas
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
            m_tools.active_tool != Tool::Line) {
            m_tools.pick_from_pixel(m_doc, m_active_layer, m_hover_px, m_hover_py);
        }
    }

    ImGui::End();
}

} // namespace pixart
