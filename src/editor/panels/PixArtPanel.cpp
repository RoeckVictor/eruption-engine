#include "PixArtPanel.h"
#include "editor/core/EditorContext.h"
#include "editor/core/Constants.h"
#include "editor/icons/IconsFontAwesome6.h"
#include "editor/EditorFileDialogs.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/core/Logger.h"

#include <imgui.h>
#include <stb_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace editor {

PixArtPanel::PixArtPanel(EditorContext& context)
    : Panel("PixArt", PanelVisibilityMode::OnDemand)
    , m_context(context) {
    refresh_materials();
}

PixArtPanel::~PixArtPanel() {
    m_canvas_tex.destroy();
}

void PixArtPanel::on_open() {
    m_materials_dirty = true;
}

void PixArtPanel::on_close() {}

bool PixArtPanel::on_close_requested() {
    if (!has_any_unsaved_changes()) {
        return true;
    }

    m_pending_action = [this]() {
        m_tabs.clear();
        m_active_tab = -1;
        set_visible(false);
    };
    m_show_unsaved_dialog = true;
    return false;
}

void PixArtPanel::on_gui() {
    if (m_materials_dirty) {
        refresh_materials();
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;
    if (!ImGui::Begin(name(), nullptr, flags)) {
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                m_show_new_dialog = true;
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                auto path = open_pixel_grid();
                if (!path.empty()) {
                    open_file(path);
                }
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", false, current_tab() != nullptr)) {
                save_current_document();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, current_tab() != nullptr)) {
                save_document_as();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import .pxg as Layer...", nullptr, false, current_tab() != nullptr)) {
                import_pxg_as_layer();
            }
            if (ImGui::MenuItem("Import Image as Layer...", nullptr, false, current_tab() != nullptr)) {
                import_image_as_layer();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close Tab", nullptr, false, current_tab() != nullptr)) {
                if (m_active_tab >= 0) {
                    close_tab(m_active_tab);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            auto* tab = current_tab();
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, tab && tab->undo.can_undo())) {
                tab->undo.undo(*tab->doc, &tab->active_layer);
                m_canvas_dirty = true;
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, tab && tab->undo.can_redo())) {
                tab->undo.redo(*tab->doc, &tab->active_layer);
                m_canvas_dirty = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selection.valid())) {
                copy_selection();
            }
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, m_selection.valid())) {
                cut_selection();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, m_clipboard.valid())) {
                paste_selection();
            }
            if (ImGui::MenuItem("Delete", "Del", false, m_selection.valid())) {
                delete_selection();
            }
            if (ImGui::MenuItem("Deselect", "Esc", false, m_selection.valid())) {
                deselect();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Resize...", nullptr, false, tab != nullptr)) {
                if (tab) {
                    m_resize_width = tab->doc->width();
                    m_resize_height = tab->doc->height();
                    m_show_resize_dialog = true;
                }
            }
            if (ImGui::MenuItem("Flatten Layers", nullptr, false, tab != nullptr)) {
                if (tab) {
                    tab->doc->flatten_art_layers();
                    tab->active_layer = 0;
                    mark_dirty();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Color Only", nullptr, m_view_mode == ViewMode::Color)) {
                m_view_mode = ViewMode::Color;
                m_canvas_dirty = true;
            }
            if (ImGui::MenuItem("Material Only", nullptr, m_view_mode == ViewMode::Material)) {
                m_view_mode = ViewMode::Material;
                m_canvas_dirty = true;
            }
            if (ImGui::MenuItem("Both", nullptr, m_view_mode == ViewMode::Both)) {
                m_view_mode = ViewMode::Both;
                m_canvas_dirty = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset View")) {
                if (auto* tab = current_tab()) {
                    tab->view.reset();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    handle_shortcuts();

    // Top bar with file operations, grid size, undo/redo
    render_top_bar();

    // Tab bar
    render_tab_bar();

    if (m_tabs.empty()) {
        ImGui::TextWrapped("No documents open. Use File > New or File > Open to create/open a pixel grid.");
        ImGui::End();

        render_new_dialog();
        render_resize_dialog();
        render_unsaved_dialog();
        return;
    }

    // Main content area with splitter
    float side_panel_width = 250.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Canvas area
    ImGui::BeginChild("##canvas_area", ImVec2(avail.x - side_panel_width - 8, 0), false);
    render_toolbar();
    render_canvas();
    ImGui::EndChild();

    ImGui::SameLine();

    // Side panel
    ImGui::BeginChild("##side_panel", ImVec2(side_panel_width, 0), true);
    render_layer_panel();
    ImGui::Separator();
    render_color_picker();
    ImGui::Separator();
    render_material_picker();
    ImGui::Separator();
    render_hover_info();
    ImGui::EndChild();

    ImGui::End();

    render_new_dialog();
    render_resize_dialog();
    render_unsaved_dialog();
}

// ---------------------------------------------------------------------------
// Top bar (file operations, grid size, undo/redo)
// ---------------------------------------------------------------------------

void PixArtPanel::render_top_bar() {
    auto* tab = current_tab();

    // File operations section
    if (ImGui::Button(ICON_FA_FILE " New")) {
        m_show_new_dialog = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("New document (Ctrl+N)");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN " Open")) {
        auto path = open_pixel_grid();
        if (!path.empty()) {
            open_file(path);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open file (Ctrl+O)");

    ImGui::SameLine();
    bool can_save = tab != nullptr;
    if (!can_save) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
        save_current_document();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save (Ctrl+S)");
    if (!can_save) ImGui::EndDisabled();

    // Import dropdown
    ImGui::SameLine();
    if (!tab) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_FILE_IMPORT " Import")) {
        ImGui::OpenPopup("##import_popup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Import file as new layer");
    if (ImGui::BeginPopup("##import_popup")) {
        if (ImGui::MenuItem("Import .pxg as Layer...")) {
            import_pxg_as_layer();
        }
        if (ImGui::MenuItem("Import Image as Layer...")) {
            import_image_as_layer();
        }
        ImGui::EndPopup();
    }
    if (!tab) ImGui::EndDisabled();

    // Separator
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Grid size section
    if (tab) {
        ImGui::Text("Size: %dx%d", tab->doc->width(), tab->doc->height());
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EXPAND " Resize")) {
            m_resize_width = tab->doc->width();
            m_resize_height = tab->doc->height();
            m_show_resize_dialog = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Resize grid");
    } else {
        ImGui::TextDisabled("No document");
    }

    // Separator
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Undo/Redo section
    bool can_undo = tab && tab->undo.can_undo();
    bool can_redo = tab && tab->undo.can_redo();

    if (!can_undo) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_ROTATE_LEFT " Undo")) {
        if (tab) {
            tab->undo.undo(*tab->doc, &tab->active_layer);
            m_canvas_dirty = true;
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Undo (Ctrl+Z)");
    if (!can_undo) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!can_redo) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Redo")) {
        if (tab) {
            tab->undo.redo(*tab->doc, &tab->active_layer);
            m_canvas_dirty = true;
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Redo (Ctrl+Y)");
    if (!can_redo) ImGui::EndDisabled();

    // Show undo/redo count
    if (tab) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d/%d)", tab->undo.undo_count(), tab->undo.redo_count());
    }

    // Separator
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // View mode toggle
    ImGui::Text("Display:");
    ImGui::SameLine();
    const char* view_labels[] = { "Color", "Material", "Both" };
    int view_idx = static_cast<int>(m_view_mode);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("##view_mode", &view_idx, view_labels, 3)) {
        m_view_mode = static_cast<ViewMode>(view_idx);
        m_canvas_dirty = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("View mode");

    ImGui::Separator();
}

// ---------------------------------------------------------------------------
// Tab bar
// ---------------------------------------------------------------------------

void PixArtPanel::render_tab_bar() {
    if (ImGui::BeginTabBar("##pixart_tabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
            auto& tab = *m_tabs[i];
            std::string label;
            if (tab.path.empty()) {
                label = "Untitled";
            } else {
                label = std::filesystem::path(tab.path).filename().string();
            }
            if (tab.dirty) {
                label += "*";
            }
            label += "###tab" + std::to_string(i);

            bool open = true;
            // Only use SetSelected when we have a pending programmatic selection
            ImGuiTabItemFlags flags = (i == m_pending_select_tab) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                if (m_active_tab != i) {
                    m_active_tab = i;
                    m_canvas_dirty = true;  // Refresh canvas when switching tabs
                }
                ImGui::EndTabItem();
            }

            if (!open) {
                close_tab(i);
                --i;
            }
        }
        // Clear pending selection after processing
        m_pending_select_tab = -1;
        ImGui::EndTabBar();
    }
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void PixArtPanel::render_toolbar() {
    auto* tab = current_tab();
    if (!tab) return;

    // Tool buttons
    auto tool_button = [&](const char* label, pixart::Tool tool) {
        bool selected = m_tools.active_tool == tool;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(label, ImVec2(60, 0))) {
            // Commit floating selection when switching away from Select tool
            if (m_tools.active_tool == pixart::Tool::Select && tool != pixart::Tool::Select) {
                if (m_has_floating) {
                    commit_floating();
                }
            }
            m_tools.active_tool = tool;
            m_tools.cancel();
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
    };

    tool_button("Pencil", pixart::Tool::Pencil);
    ImGui::SameLine();
    tool_button("Bucket", pixart::Tool::Bucket);
    ImGui::SameLine();
    tool_button("Line", pixart::Tool::Line);
    ImGui::SameLine();
    tool_button("Eraser", pixart::Tool::Eraser);
    ImGui::SameLine();
    tool_button("Pipette", pixart::Tool::Pipette);
    ImGui::SameLine();
    tool_button("Select", pixart::Tool::Select);

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Brush size (not for pipette or select)
    if (m_tools.active_tool != pixart::Tool::Pipette &&
        m_tools.active_tool != pixart::Tool::Select) {
        ImGui::SetNextItemWidth(80);
        ImGui::SliderInt("Size", &m_tools.draw_state.brush_size, 1, 32);
    }

    // Eraser options (only show when eraser is selected)
    if (m_tools.active_tool == pixart::Tool::Eraser) {
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::Checkbox("Color", &m_tools.draw_state.erase_color);
        ImGui::SameLine();
        ImGui::Checkbox("Material", &m_tools.draw_state.erase_material);
    }

    // Pipette options (only show when pipette is selected)
    if (m_tools.active_tool == pixart::Tool::Pipette) {
        ImGui::Checkbox("Color", &m_tools.draw_state.pick_color);
        ImGui::SameLine();
        ImGui::Checkbox("Material", &m_tools.draw_state.pick_material);
    }

    // Selection options (only show when select is active)
    if (m_tools.active_tool == pixart::Tool::Select) {
        if (m_has_floating) {
            // Floating selection mode - show position and commit/cancel
            ImGui::Text("Moving: %dx%d at (%d,%d)", m_float_width, m_float_height, m_float_x, m_float_y);
            ImGui::SameLine();
            if (ImGui::Button("Commit")) commit_floating();
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                cancel_floating();
                deselect();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy")) copy_selection();
        } else if (m_selection.valid()) {
            ImGui::Text("Sel: %dx%d", m_selection.width, m_selection.height);
            ImGui::SameLine();
            ImGui::TextDisabled("(drag inside to move)");
            ImGui::SameLine();
            if (ImGui::Button("Copy")) copy_selection();
            ImGui::SameLine();
            if (ImGui::Button("Cut")) cut_selection();
            ImGui::SameLine();
            if (ImGui::Button("Delete")) delete_selection();
            ImGui::SameLine();
            if (ImGui::Button("Deselect")) deselect();
        }
        if (m_clipboard.valid()) {
            ImGui::SameLine();
            if (ImGui::Button("Paste")) paste_selection();
        }
    }

    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();

    // Origin button
    bool was_setting_origin = m_setting_origin;
    if (was_setting_origin) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    }
    if (ImGui::Button("Set Origin")) {
        m_setting_origin = !m_setting_origin;
    }
    if (was_setting_origin) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::Text("| %dx%d", tab->doc->width(), tab->doc->height());

    ImGui::SameLine();
    ImGui::Text("| Zoom: %.0fx", tab->view.zoom);
}

// ---------------------------------------------------------------------------
// Canvas rendering
// ---------------------------------------------------------------------------

void PixArtPanel::build_composite() {
    auto* tab = current_tab();
    if (!tab || !tab->doc->valid()) return;

    int w = tab->doc->width();
    int h = tab->doc->height();
    size_t pixel_count = static_cast<size_t>(w) * h;
    m_composite.assign(pixel_count * 4, 0);

    switch (m_view_mode) {
    case ViewMode::Color:
        // Composite art layers only
        tab->doc->composite_art_layers(m_composite);
        break;

    case ViewMode::Material: {
        // Show materials using their actual colors from MaterialLibrary
        // Color format is 0xRRGGBBAA
        auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
        for (size_t px = 0; px < pixel_count; ++px) {
            uint8_t mat_id = tab->doc->material_data()[px];
            uint32_t color = lib ? lib->get_color(mat_id) : 0x888888FF;
            m_composite[px * 4 + 0] = (color >> 24) & 0xFF; // R
            m_composite[px * 4 + 1] = (color >> 16) & 0xFF; // G
            m_composite[px * 4 + 2] = (color >> 8) & 0xFF;  // B
            m_composite[px * 4 + 3] = (color >> 0) & 0xFF;  // A
        }
        break;
    }

    case ViewMode::Both: {
        // Composite art layers first
        tab->doc->composite_art_layers(m_composite);
        // Then tint with material color where alpha > 0
        // Color format is 0xRRGGBBAA
        auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
        for (size_t px = 0; px < pixel_count; ++px) {
            if (m_composite[px * 4 + 3] > 0) {
                uint8_t mat_id = tab->doc->material_data()[px];
                uint32_t mat_color = lib ? lib->get_color(mat_id) : 0x888888FF;
                // Blend material color at 20% opacity
                float blend = 0.2f;
                m_composite[px * 4 + 0] = static_cast<uint8_t>(
                    m_composite[px * 4 + 0] * (1 - blend) + ((mat_color >> 24) & 0xFF) * blend);
                m_composite[px * 4 + 1] = static_cast<uint8_t>(
                    m_composite[px * 4 + 1] * (1 - blend) + ((mat_color >> 16) & 0xFF) * blend);
                m_composite[px * 4 + 2] = static_cast<uint8_t>(
                    m_composite[px * 4 + 2] * (1 - blend) + ((mat_color >> 8) & 0xFF) * blend);
            }
        }
        break;
    }
    }
}

void PixArtPanel::update_canvas_texture() {
    auto* tab = current_tab();
    if (!tab || !tab->doc->valid()) return;

    int w = tab->doc->width();
    int h = tab->doc->height();

    build_composite();

    if (!m_canvas_tex.valid() || m_canvas_tex.width() != w || m_canvas_tex.height() != h) {
        m_canvas_tex.destroy();
        m_canvas_tex.create_2d(w, h,
            engine::graphics::TextureFormat::RGBA8,
            engine::graphics::TextureFilter::Nearest,
            engine::graphics::TextureWrap::ClampToEdge,
            m_composite.data());
    } else {
        m_canvas_tex.upload_sub_2d(0, 0, w, h, m_composite.data());
    }

    m_canvas_dirty = false;
}

void PixArtPanel::render_canvas() {
    auto* tab = current_tab();
    if (!tab || !tab->doc->valid()) {
        ImGui::Text("No document loaded.");
        return;
    }

    if (m_canvas_dirty) {
        update_canvas_texture();
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f || avail.y < 1.0f) return;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    float canvas_w = avail.x;
    float canvas_h = avail.y;

    // Invisible button to capture input
    ImGui::InvisibleButton("##canvas", avail,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool canvas_hovered = ImGui::IsItemHovered();

    // Zoom
    if (canvas_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            float factor = (wheel > 0) ? constants::ZOOM_FACTOR_IN : constants::ZOOM_FACTOR_OUT;
            tab->view.zoom_towards(factor, mouse.x, mouse.y,
                                   canvas_pos.x + canvas_w * 0.5f,
                                   canvas_pos.y + canvas_h * 0.5f);
        }
    }

    // Pan (middle mouse)
    if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        m_pan_state.begin(mouse.x, mouse.y, tab->view.pan_x, tab->view.pan_y);
    }
    if (m_pan_state.active) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            m_pan_state.update(mouse.x, mouse.y, tab->view.pan_x, tab->view.pan_y);
        } else {
            m_pan_state.end();
        }
    }

    // Compute grid position
    float grid_x0, grid_y0, grid_w, grid_h;
    tab->view.get_grid_screen_bounds(canvas_pos.x, canvas_pos.y, canvas_w, canvas_h,
                                     tab->doc->width(), tab->doc->height(),
                                     grid_x0, grid_y0, grid_w, grid_h);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(canvas_pos,
                     ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h), true);

    // Checkerboard background
    {
        float check_size = std::max(tab->view.zoom * 0.5f, 4.0f);
        int cols = static_cast<int>(std::ceil(grid_w / check_size));
        int rows = static_cast<int>(std::ceil(grid_h / check_size));
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                ImU32 col = ((r + c) % 2 == 0)
                    ? IM_COL32(180, 180, 180, 255)
                    : IM_COL32(220, 220, 220, 255);
                float x0 = grid_x0 + c * check_size;
                float y0 = grid_y0 + r * check_size;
                float x1 = std::min(x0 + check_size, grid_x0 + grid_w);
                float y1 = std::min(y0 + check_size, grid_y0 + grid_h);
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col);
            }
        }
    }

    // Draw texture
    if (m_canvas_tex.valid()) {
        dl->AddImage((ImTextureID)(uintptr_t)m_canvas_tex.imgui_texture_id(),
                     ImVec2(grid_x0, grid_y0),
                     ImVec2(grid_x0 + grid_w, grid_y0 + grid_h));
    }

    // Grid overlay
    if (tab->view.zoom >= constants::MIN_GRID_ZOOM) {
        ImU32 grid_col = IM_COL32(100, 100, 100, 60);
        for (int x = 0; x <= tab->doc->width(); ++x) {
            float lx = grid_x0 + x * tab->view.zoom;
            dl->AddLine(ImVec2(lx, grid_y0), ImVec2(lx, grid_y0 + grid_h), grid_col);
        }
        for (int y = 0; y <= tab->doc->height(); ++y) {
            float ly = grid_y0 + y * tab->view.zoom;
            dl->AddLine(ImVec2(grid_x0, ly), ImVec2(grid_x0 + grid_w, ly), grid_col);
        }
    }

    // Origin crosshair
    {
        int ox = tab->doc->origin_x();
        int oy = tab->doc->origin_y();
        float ocx = grid_x0 + (ox + 0.5f) * tab->view.zoom;
        float ocy = grid_y0 + (oy + 0.5f) * tab->view.zoom;
        float arm = std::max(tab->view.zoom * 1.5f, 6.0f);
        ImU32 origin_col = IM_COL32(0, 220, 220, 220);
        dl->AddLine(ImVec2(ocx - arm, ocy), ImVec2(ocx + arm, ocy), origin_col, 2.0f);
        dl->AddLine(ImVec2(ocx, ocy - arm), ImVec2(ocx, ocy + arm), origin_col, 2.0f);
        float d = std::max(tab->view.zoom * 0.3f, 3.0f);
        dl->AddQuadFilled(ImVec2(ocx, ocy - d), ImVec2(ocx + d, ocy),
                          ImVec2(ocx, ocy + d), ImVec2(ocx - d, ocy), origin_col);
    }

    // Border
    dl->AddRect(ImVec2(grid_x0, grid_y0),
                ImVec2(grid_x0 + grid_w, grid_y0 + grid_h),
                IM_COL32(200, 200, 200, 255));

    // Hover pixel highlight / brush preview
    m_hover_px = -1;
    m_hover_py = -1;
    if (canvas_hovered && !m_pan_state.active) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int px, py;
        if (tab->view.screen_to_pixel(mouse.x, mouse.y,
                                      canvas_pos.x, canvas_pos.y, canvas_w, canvas_h,
                                      tab->doc->width(), tab->doc->height(), px, py)) {
            m_hover_px = px;
            m_hover_py = py;

            // Pipette shows single pixel, other tools show brush size
            int r = (m_tools.active_tool == pixart::Tool::Pipette) ? 0 : m_tools.draw_state.brush_size - 1;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy <= r * r) {
                        int bx = px + dx, by = py + dy;
                        if (bx >= 0 && bx < tab->doc->width() &&
                            by >= 0 && by < tab->doc->height()) {
                            float rx0 = grid_x0 + bx * tab->view.zoom;
                            float ry0 = grid_y0 + by * tab->view.zoom;
                            dl->AddRectFilled(
                                ImVec2(rx0, ry0),
                                ImVec2(rx0 + tab->view.zoom, ry0 + tab->view.zoom),
                                IM_COL32(255, 255, 0, 40));
                        }
                    }
                }
            }
        }
    }

    // Line tool preview
    if (m_tools.active_tool == pixart::Tool::Line && m_tools.line_started && canvas_hovered) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int px, py;
        if (tab->view.screen_to_pixel(mouse.x, mouse.y,
                                      canvas_pos.x, canvas_pos.y, canvas_w, canvas_h,
                                      tab->doc->width(), tab->doc->height(), px, py)) {
            float sx0 = grid_x0 + (m_tools.line_start_x + 0.5f) * tab->view.zoom;
            float sy0 = grid_y0 + (m_tools.line_start_y + 0.5f) * tab->view.zoom;
            float sx1 = grid_x0 + (px + 0.5f) * tab->view.zoom;
            float sy1 = grid_y0 + (py + 0.5f) * tab->view.zoom;
            dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy1),
                        IM_COL32(255, 255, 0, 180), 2.0f);
        }
    }

    // Floating selection content (draw pixels being moved)
    if (m_has_floating && m_selection.valid()) {
        float zoom = tab->view.zoom;
        for (int dy = 0; dy < m_float_height; ++dy) {
            for (int dx = 0; dx < m_float_width; ++dx) {
                size_t idx = static_cast<size_t>(dy) * m_float_width + dx;
                const uint8_t* rgba = m_float_color.data() + idx * 4;

                // Skip fully transparent pixels
                if (rgba[3] == 0) continue;

                float px0 = grid_x0 + (m_float_x + dx) * zoom;
                float py0 = grid_y0 + (m_float_y + dy) * zoom;
                float px1 = px0 + zoom;
                float py1 = py0 + zoom;

                ImU32 col = IM_COL32(rgba[0], rgba[1], rgba[2], rgba[3]);
                dl->AddRectFilled(ImVec2(px0, py0), ImVec2(px1, py1), col);
            }
        }
    }

    // Selection rectangle overlay
    if (m_selection.valid()) {
        float sel_x0 = grid_x0 + m_selection.x * tab->view.zoom;
        float sel_y0 = grid_y0 + m_selection.y * tab->view.zoom;
        float sel_x1 = sel_x0 + m_selection.width * tab->view.zoom;
        float sel_y1 = sel_y0 + m_selection.height * tab->view.zoom;

        // Semi-transparent fill (lighter if floating)
        ImU32 fill_col = m_has_floating ? IM_COL32(100, 200, 255, 30) : IM_COL32(100, 150, 255, 40);
        dl->AddRectFilled(ImVec2(sel_x0, sel_y0), ImVec2(sel_x1, sel_y1), fill_col);

        // Border (dashed effect for floating - using dotted line)
        ImU32 border_col = m_has_floating ? IM_COL32(50, 200, 255, 255) : IM_COL32(100, 150, 255, 255);
        dl->AddRect(ImVec2(sel_x0, sel_y0), ImVec2(sel_x1, sel_y1), border_col, 0.0f, 0, 2.0f);
    }

    dl->PopClipRect();

    handle_canvas_input(canvas_pos.x, canvas_pos.y, canvas_w, canvas_h);
}

// ---------------------------------------------------------------------------
// Canvas input handling
// ---------------------------------------------------------------------------

void PixArtPanel::handle_canvas_input(float cx0, float cy0, float cw, float ch) {
    auto* tab = current_tab();
    if (!tab || m_pan_state.active) return;

    // Only allow material editing on Main layer (index 0)
    m_tools.draw_state.allow_material_edit = (tab->active_layer == 0);

    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    int px, py;
    bool on_grid = tab->view.screen_to_pixel(mouse.x, mouse.y, cx0, cy0, cw, ch,
                                             tab->doc->width(), tab->doc->height(), px, py);

    // Origin placement mode
    if (m_setting_origin) {
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            tab->undo.push_origin_change(*tab->doc, tab->doc->origin_x(), tab->doc->origin_y());
            tab->doc->set_origin(px, py);
            m_setting_origin = false;
            mark_dirty();
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_setting_origin = false;
        }
        return;
    }

    switch (m_tools.active_tool) {
    case pixart::Tool::Pencil:
    case pixart::Tool::Eraser: {
        bool is_eraser = m_tools.active_tool == pixart::Tool::Eraser;
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            m_drawing = true;
            tab->undo.begin_operation(*tab->doc);
            // Record both art and material for undo
            tab->undo.record_art_brush(tab->active_layer, px, py, m_tools.draw_state.brush_size);
            tab->undo.record_material_brush(px, py, m_tools.draw_state.brush_size);
            if (is_eraser) {
                m_tools.apply_eraser(*tab->doc, tab->active_layer, px, py);
            } else {
                m_tools.apply_pencil(*tab->doc, tab->active_layer, px, py);
            }
            m_last_draw_x = px;
            m_last_draw_y = py;
            m_canvas_dirty = true;
            mark_dirty();
        }
        if (m_drawing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (on_grid && (px != m_last_draw_x || py != m_last_draw_y)) {
                pixart::bresenham_line(m_last_draw_x, m_last_draw_y, px, py, [&](int x, int y) {
                    tab->undo.record_art_brush(tab->active_layer, x, y, m_tools.draw_state.brush_size);
                    tab->undo.record_material_brush(x, y, m_tools.draw_state.brush_size);
                });
                if (is_eraser) {
                    pixart::bresenham_line(m_last_draw_x, m_last_draw_y, px, py, [&](int x, int y) {
                        m_tools.apply_eraser(*tab->doc, tab->active_layer, x, y);
                    });
                } else {
                    m_tools.apply_line(*tab->doc, tab->active_layer, m_last_draw_x, m_last_draw_y, px, py);
                }
                m_last_draw_x = px;
                m_last_draw_y = py;
                m_canvas_dirty = true;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (m_drawing) {
                tab->undo.end_operation();
            }
            m_drawing = false;
        }
        break;
    }

    case pixart::Tool::Bucket:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            // Determine matching mode based on view mode
            bool match_color = (m_view_mode == ViewMode::Color || m_view_mode == ViewMode::Both);
            bool match_material = (m_view_mode == ViewMode::Material || m_view_mode == ViewMode::Both);

            tab->undo.begin_operation(*tab->doc);
            m_tools.apply_bucket(*tab->doc, tab->active_layer, px, py, match_color, match_material,
                [&](int fx, int fy) {
                    tab->undo.record_art_pixel(tab->active_layer, fx, fy);
                    tab->undo.record_material_pixel(fx, fy);
                });
            tab->undo.end_operation();
            m_canvas_dirty = true;
            mark_dirty();
        }
        break;

    case pixart::Tool::Line:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            if (!m_tools.line_started) {
                m_tools.line_started = true;
                m_tools.line_start_x = px;
                m_tools.line_start_y = py;
            } else {
                tab->undo.begin_operation(*tab->doc);
                pixart::bresenham_line(m_tools.line_start_x, m_tools.line_start_y, px, py, [&](int x, int y) {
                    tab->undo.record_art_brush(tab->active_layer, x, y, m_tools.draw_state.brush_size);
                    tab->undo.record_material_brush(x, y, m_tools.draw_state.brush_size);
                });
                m_tools.apply_line(*tab->doc, tab->active_layer, m_tools.line_start_x, m_tools.line_start_y, px, py);
                tab->undo.end_operation();
                m_tools.line_started = false;
                m_canvas_dirty = true;
                mark_dirty();
            }
        }
        if (m_tools.line_started && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_tools.line_started = false;
        }
        break;

    case pixart::Tool::Pipette:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            m_tools.pick_from_pixel(*tab->doc, tab->active_layer, px, py);
        }
        break;

    case pixart::Tool::Select:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            // Check if clicking inside existing selection (to move it)
            bool inside_selection = m_selection.valid() &&
                px >= m_selection.x && px < m_selection.x + m_selection.width &&
                py >= m_selection.y && py < m_selection.y + m_selection.height;

            if (inside_selection) {
                // Lift selection if not already floating
                if (!m_has_floating) {
                    lift_selection();
                }
                // Start moving
                m_moving_float = true;
                m_move_offset_x = px - m_float_x;
                m_move_offset_y = py - m_float_y;
            } else {
                // Clicking outside - commit any floating selection and start new
                if (m_has_floating) {
                    commit_floating();
                }
                // Start new selection
                m_selecting = true;
                m_select_start_x = px;
                m_select_start_y = py;
                m_selection.x = px;
                m_selection.y = py;
                m_selection.width = 1;
                m_selection.height = 1;
            }
        }

        // Update while dragging
        if (m_moving_float && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Update floating selection position
            m_float_x = px - m_move_offset_x;
            m_float_y = py - m_move_offset_y;
            // Update selection rect to match
            m_selection.x = m_float_x;
            m_selection.y = m_float_y;
            m_canvas_dirty = true;
        } else if (m_selecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Update selection rectangle
            int x0 = std::min(m_select_start_x, px);
            int y0 = std::min(m_select_start_y, py);
            int x1 = std::max(m_select_start_x, px);
            int y1 = std::max(m_select_start_y, py);
            m_selection.x = x0;
            m_selection.y = y0;
            m_selection.width = x1 - x0 + 1;
            m_selection.height = y1 - y0 + 1;
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_selecting = false;
            m_moving_float = false;
        }

        // Right-click to deselect/cancel
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (m_has_floating) {
                cancel_floating();
            }
            deselect();
        }
        break;
    }

    // Eyedropper (right-click) - picks both color and material (except when using pipette or select tool)
    if (m_tools.active_tool != pixart::Tool::Pipette &&
        m_tools.active_tool != pixart::Tool::Select &&
        hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && on_grid) {
        m_tools.pick_from_pixel(*tab->doc, tab->active_layer, px, py);
    }
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts
// ---------------------------------------------------------------------------

void PixArtPanel::handle_shortcuts() {
    auto* tab = current_tab();
    if (!tab) return;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) return;

    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        m_show_new_dialog = true;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        auto path = open_pixel_grid();
        if (!path.empty()) {
            open_file(path);
        }
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S)) {
        save_document_as();
    } else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        save_current_document();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        // If we have a floating selection, cancel it first
        if (m_has_floating) {
            cancel_floating();
            deselect();
        } else if (tab->undo.undo(*tab->doc, &tab->active_layer)) {
            m_canvas_dirty = true;
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (tab->undo.redo(*tab->doc, &tab->active_layer)) {
            m_canvas_dirty = true;
        }
    }

    // Tool shortcuts
    if (!ctrl && !shift) {
        auto switch_tool = [&](pixart::Tool new_tool) {
            // Commit floating selection when switching away from Select
            if (m_tools.active_tool == pixart::Tool::Select && new_tool != pixart::Tool::Select) {
                if (m_has_floating) {
                    commit_floating();
                }
            }
            m_tools.active_tool = new_tool;
        };

        if (ImGui::IsKeyPressed(ImGuiKey_B)) switch_tool(pixart::Tool::Pencil);
        if (ImGui::IsKeyPressed(ImGuiKey_G)) switch_tool(pixart::Tool::Bucket);
        if (ImGui::IsKeyPressed(ImGuiKey_L)) switch_tool(pixart::Tool::Line);
        if (ImGui::IsKeyPressed(ImGuiKey_E)) switch_tool(pixart::Tool::Eraser);
        if (ImGui::IsKeyPressed(ImGuiKey_I)) switch_tool(pixart::Tool::Pipette);
        if (ImGui::IsKeyPressed(ImGuiKey_M)) switch_tool(pixart::Tool::Select);
    }

    // Selection shortcuts
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        copy_selection();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        cut_selection();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        paste_selection();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        delete_selection();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        deselect();
    }
}

// ---------------------------------------------------------------------------
// Layer panel
// ---------------------------------------------------------------------------

void PixArtPanel::render_layer_panel() {
    auto* tab = current_tab();
    if (!tab) return;

    ImGui::Text("Layers");

    // Add layer button
    if (ImGui::Button(ICON_FA_PLUS " Add")) {
        tab->active_layer = tab->doc->add_art_layer("Layer " + std::to_string(tab->doc->art_layer_count() + 1));
        mark_dirty();
    }

    ImGui::SameLine();

    // Remove layer button (cannot remove Main layer at index 0)
    bool can_remove = tab->doc->art_layer_count() > 1 && tab->active_layer != 0;
    if (!can_remove) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_TRASH " Remove")) {
        tab->active_layer = tab->doc->remove_art_layer(tab->active_layer, tab->active_layer);
        m_canvas_dirty = true;
        mark_dirty();
    }
    if (!can_remove) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && tab->active_layer == 0) {
        ImGui::SetTooltip("Cannot remove Main layer");
    }

    ImGui::SameLine();

    // Merge down button
    bool can_merge = tab->active_layer > 0;
    if (!can_merge) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_COMPRESS " Merge")) {
        // Merge current layer down to the layer below
        int below = tab->active_layer - 1;
        tab->doc->merge_art_layers(below, tab->active_layer);
        tab->active_layer = below;
        m_canvas_dirty = true;
        mark_dirty();
    }
    if (!can_merge) ImGui::EndDisabled();

    ImGui::Separator();

    // Layer list (top to bottom = top layer to bottom layer)
    for (int i = tab->doc->art_layer_count() - 1; i >= 0; --i) {
        auto& layer = tab->doc->art_layer(i);
        ImGui::PushID(i);

        // Row layout with fixed button area
        float button_width = 24.0f;
        float avail = ImGui::GetContentRegionAvail().x;
        float name_width = avail - button_width * 2 - 30; // checkbox + buttons

        // Visibility checkbox
        if (ImGui::Checkbox("##vis", &layer.visible)) {
            m_canvas_dirty = true;
        }
        ImGui::SameLine();

        // Selectable layer name (with material indicator for Main layer)
        bool selected = (i == tab->active_layer);
        bool is_main_layer = (i == 0);
        std::string display_name = is_main_layer ? (layer.name + " " ICON_FA_CUBE) : layer.name;
        ImGui::SetNextItemWidth(name_width);
        if (ImGui::Selectable(display_name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(name_width, 0))) {
            tab->active_layer = i;
            // Double-click to rename (cannot rename Main layer)
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !is_main_layer) {
                m_renaming_layer = i;
                m_rename_buffer = layer.name;
            }
        }
        if (is_main_layer && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Main layer (holds materials)");
        }

        // Move buttons (always visible to avoid layout shift)
        // Main layer (index 0) cannot be moved, and no layer can move into position 0
        ImGui::SameLine();

        // Move up (visually up = higher layer index)
        // Main layer cannot move up
        bool can_up = i < tab->doc->art_layer_count() - 1 && !is_main_layer;
        if (!can_up) ImGui::BeginDisabled();
        if (ImGui::SmallButton(ICON_FA_ARROW_UP)) {
            tab->undo.push_layer_swap(*tab->doc, i, i + 1);
            tab->doc->swap_art_layers(i, i + 1);
            if (tab->active_layer == i) tab->active_layer = i + 1;
            else if (tab->active_layer == i + 1) tab->active_layer = i;
            m_canvas_dirty = true;
            mark_dirty();
        }
        if (!can_up) ImGui::EndDisabled();

        ImGui::SameLine();

        // Move down (visually down = lower layer index)
        // Cannot move into position 0 (Main layer position), so minimum is index 1
        bool can_down = i > 1;
        if (!can_down) ImGui::BeginDisabled();
        if (ImGui::SmallButton(ICON_FA_ARROW_DOWN)) {
            tab->undo.push_layer_swap(*tab->doc, i, i - 1);
            tab->doc->swap_art_layers(i, i - 1);
            if (tab->active_layer == i) tab->active_layer = i - 1;
            else if (tab->active_layer == i - 1) tab->active_layer = i;
            m_canvas_dirty = true;
            mark_dirty();
        }
        if (!can_down) ImGui::EndDisabled();

        ImGui::PopID();
    }

    ImGui::Separator();

    // Opacity slider for current layer
    if (tab->active_layer >= 0 && tab->active_layer < tab->doc->art_layer_count()) {
        auto& layer = tab->doc->art_layer(tab->active_layer);
        if (ImGui::SliderFloat("Opacity", &layer.opacity, 0.0f, 1.0f)) {
            m_canvas_dirty = true;
        }
    }

    // Rename popup
    if (m_renaming_layer >= 0 && m_renaming_layer < tab->doc->art_layer_count()) {
        ImGui::OpenPopup("Rename Layer");
    }
    if (ImGui::BeginPopup("Rename Layer")) {
        ImGui::Text("Rename Layer");
        char buf[256];
        strncpy(buf, m_rename_buffer.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("##rename", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (m_renaming_layer >= 0 && m_renaming_layer < tab->doc->art_layer_count()) {
                tab->doc->art_layer(m_renaming_layer).name = buf;
                mark_dirty();
            }
            m_renaming_layer = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("OK")) {
            if (m_renaming_layer >= 0 && m_renaming_layer < tab->doc->art_layer_count()) {
                tab->doc->art_layer(m_renaming_layer).name = buf;
                mark_dirty();
            }
            m_renaming_layer = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            m_renaming_layer = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Color picker
// ---------------------------------------------------------------------------

void PixArtPanel::render_color_picker() {
    ImGui::Text("Color");

    // Color preview square
    ImVec4 col(m_tools.draw_state.color[0], m_tools.draw_state.color[1],
               m_tools.draw_state.color[2], m_tools.draw_state.color[3]);
    ImGui::ColorButton("##color_preview", col, ImGuiColorEditFlags_AlphaPreview, ImVec2(40, 40));
    ImGui::SameLine();
    ImGui::Text("RGBA: %d,%d,%d,%d",
                static_cast<int>(m_tools.draw_state.color[0] * 255),
                static_cast<int>(m_tools.draw_state.color[1] * 255),
                static_cast<int>(m_tools.draw_state.color[2] * 255),
                static_cast<int>(m_tools.draw_state.color[3] * 255));

    ImGui::ColorPicker4("##color", m_tools.draw_state.color,
                        ImGuiColorEditFlags_AlphaBar |
                        ImGuiColorEditFlags_NoSidePreview |
                        ImGuiColorEditFlags_NoSmallPreview);
}

// ---------------------------------------------------------------------------
// Material picker
// ---------------------------------------------------------------------------

void PixArtPanel::render_material_picker() {
    auto* tab = current_tab();
    bool is_main_layer = tab && tab->active_layer == 0;

    ImGui::Text("Material");

    // Materials can only be edited on Main layer (index 0)
    if (!is_main_layer) {
        ImGui::TextDisabled("(Main layer only)");
        ImGui::TextWrapped("Select the Main layer to edit materials. Other layers are for color/display only.");
        return;
    }

    // Helper to get display name for material
    auto get_display_name = [](uint8_t id, const std::string& name) -> std::string {
        if (name.empty()) {
            return "Material " + std::to_string(static_cast<int>(id));
        }
        return name;
    };

    // Current material display
    std::string current_name = "Unknown";
    for (const auto& [id, name] : m_material_cache) {
        if (id == m_tools.draw_state.material_id) {
            current_name = get_display_name(id, name);
            break;
        }
    }

    ImGui::PushID("material_picker");
    if (ImGui::BeginCombo("##mat_combo", current_name.c_str())) {
        for (const auto& [id, name] : m_material_cache) {
            bool selected = (id == m_tools.draw_state.material_id);
            // Use unique label with ##id suffix to ensure unique ImGui IDs
            std::string display = get_display_name(id, name);
            std::string label = display + "##mat" + std::to_string(static_cast<int>(id));
            if (ImGui::Selectable(label.c_str(), selected)) {
                m_tools.draw_state.material_id = id;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopID();

    // Show material color preview (color format is 0xRRGGBBAA)
    auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
    if (lib) {
        uint32_t color = lib->get_color(m_tools.draw_state.material_id);
        ImVec4 col(
            ((color >> 24) & 0xFF) / 255.0f, // R
            ((color >> 16) & 0xFF) / 255.0f, // G
            ((color >> 8) & 0xFF) / 255.0f,  // B
            ((color >> 0) & 0xFF) / 255.0f   // A
        );
        ImGui::ColorButton("##matcolor", col, 0, ImVec2(40, 40));
    }
}

// ---------------------------------------------------------------------------
// Hover info (shows data of currently hovered pixel)
// ---------------------------------------------------------------------------

void PixArtPanel::render_hover_info() {
    auto* tab = current_tab();
    if (!tab) return;

    ImGui::Text("Hover Info");

    if (m_hover_px < 0 || m_hover_py < 0 ||
        m_hover_px >= tab->doc->width() || m_hover_py >= tab->doc->height()) {
        ImGui::TextDisabled("(no pixel)");
        return;
    }

    // Position
    ImGui::Text("Pos: %d, %d", m_hover_px, m_hover_py);

    // Color from current art layer
    if (tab->active_layer >= 0 && tab->active_layer < tab->doc->art_layer_count()) {
        uint8_t rgba[4];
        tab->doc->get_art_layer_pixel(tab->active_layer, m_hover_px, m_hover_py, rgba);
        ImVec4 col(rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, rgba[3] / 255.0f);
        ImGui::ColorButton("##hover_color", col, ImGuiColorEditFlags_AlphaPreview, ImVec2(20, 20));
        ImGui::SameLine();
        ImGui::Text("RGBA: %d,%d,%d,%d", rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    // Material
    uint8_t mat_id = tab->doc->get_material(m_hover_px, m_hover_py);
    std::string mat_name = "Unknown";
    for (const auto& [id, name] : m_material_cache) {
        if (id == mat_id) {
            mat_name = name.empty() ? "Material " + std::to_string(static_cast<int>(id)) : name;
            break;
        }
    }

    // Material color preview (color format is 0xRRGGBBAA)
    auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
    if (lib) {
        uint32_t color = lib->get_color(mat_id);
        ImVec4 mat_col(
            ((color >> 24) & 0xFF) / 255.0f, // R
            ((color >> 16) & 0xFF) / 255.0f, // G
            ((color >> 8) & 0xFF) / 255.0f,  // B
            1.0f
        );
        ImGui::ColorButton("##hover_mat", mat_col, 0, ImVec2(20, 20));
        ImGui::SameLine();
    }
    ImGui::Text("Mat: %s (%d)", mat_name.c_str(), static_cast<int>(mat_id));
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void PixArtPanel::render_new_dialog() {
    if (!m_show_new_dialog) return;

    ImGui::OpenPopup("New Pixel Grid");
    if (ImGui::BeginPopupModal("New Pixel Grid", &m_show_new_dialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Width", &m_new_width);
        ImGui::InputInt("Height", &m_new_height);

        m_new_width = std::clamp(m_new_width, constants::GRID_SIZE_MIN, constants::GRID_SIZE_MAX);
        m_new_height = std::clamp(m_new_height, constants::GRID_SIZE_MIN, constants::GRID_SIZE_MAX);

        if (ImGui::Button("Create", ImVec2(100, 0))) {
            create_new_document(m_new_width, m_new_height);
            m_show_new_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_show_new_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PixArtPanel::render_resize_dialog() {
    if (!m_show_resize_dialog) return;

    ImGui::OpenPopup("Resize Pixel Grid");
    if (ImGui::BeginPopupModal("Resize Pixel Grid", &m_show_resize_dialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto* tab = current_tab();

        ImGui::InputInt("Width", &m_resize_width);
        ImGui::InputInt("Height", &m_resize_height);

        m_resize_width = std::clamp(m_resize_width, constants::GRID_SIZE_MIN, constants::GRID_SIZE_MAX);
        m_resize_height = std::clamp(m_resize_height, constants::GRID_SIZE_MIN, constants::GRID_SIZE_MAX);

        if (ImGui::Button("Resize", ImVec2(100, 0))) {
            if (tab && tab->doc->resize(m_resize_width, m_resize_height)) {
                m_canvas_dirty = true;
                mark_dirty();
            }
            m_show_resize_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_show_resize_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PixArtPanel::render_unsaved_dialog() {
    if (!m_show_unsaved_dialog) return;

    ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Do you want to save before closing?");

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            if (save_current_document()) {
                if (m_pending_action) m_pending_action();
            }
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            if (m_pending_action) m_pending_action();
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_pending_action = nullptr;
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void PixArtPanel::open_file(const std::string& path) {
    // Check if already open
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        if (m_tabs[i]->path == path) {
            m_active_tab = i;
            m_pending_select_tab = i;
            return;
        }
    }

    auto tab = std::make_unique<PixArtTab>();
    if (!tab->doc->load(path)) {
        engine::Logger::instance().error("PixArtPanel", "Failed to load file: %s", path.c_str());
        return;
    }
    tab->path = path;

    // Sync material names from library
    auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
    if (lib) {
        std::vector<std::string> names;
        for (const auto& mat : lib->get_all_materials()) {
            if (mat.id < 256) {
                while (names.size() <= mat.id) names.push_back("");
                names[mat.id] = mat.name;
            }
        }
        tab->doc->set_material_names(names);
    }

    m_tabs.push_back(std::move(tab));
    m_active_tab = static_cast<int>(m_tabs.size()) - 1;
    m_pending_select_tab = m_active_tab;
    m_canvas_dirty = true;
}

bool PixArtPanel::save_current_document() {
    auto* tab = current_tab();
    if (!tab) return false;

    if (tab->path.empty()) {
        return save_document_as();
    }

    // Flatten art layers to final color before saving
    std::vector<uint8_t> composited;
    tab->doc->composite_art_layers(composited);
    for (int y = 0; y < tab->doc->height(); ++y) {
        for (int x = 0; x < tab->doc->width(); ++x) {
            size_t idx = (static_cast<size_t>(y) * tab->doc->width() + x) * 4;
            tab->doc->set_final_color(x, y, composited.data() + idx);
        }
    }

    if (tab->doc->save(tab->path)) {
        tab->dirty = false;
        return true;
    }
    return false;
}

bool PixArtPanel::save_document_as() {
    auto* tab = current_tab();
    if (!tab) return false;

    auto initial_dir = get_assets_directory(m_context.scene_state().project_path());
    auto path = save_pixel_grid(initial_dir);
    if (path.empty()) return false;

    tab->path = path;
    return save_current_document();
}

void PixArtPanel::create_new_document(int width, int height) {
    auto tab = std::make_unique<PixArtTab>();
    if (!tab->doc->create(width, height)) {
        return;
    }

    // Set default material names from library
    auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
    if (lib) {
        std::vector<std::string> names;
        for (const auto& mat : lib->get_all_materials()) {
            if (mat.id < 256) {
                while (names.size() <= mat.id) names.push_back("");
                names[mat.id] = mat.name;
            }
        }
        tab->doc->set_material_names(names);
    }

    m_tabs.push_back(std::move(tab));
    m_active_tab = static_cast<int>(m_tabs.size()) - 1;
    m_pending_select_tab = m_active_tab;
    m_canvas_dirty = true;
}

void PixArtPanel::close_tab(int tab_index) {
    if (tab_index < 0 || tab_index >= static_cast<int>(m_tabs.size())) return;

    if (m_tabs[tab_index]->dirty) {
        m_pending_close_tab = tab_index;
        m_active_tab = tab_index;
        m_pending_select_tab = tab_index;
        m_pending_action = [this]() {
            if (m_pending_close_tab >= 0 && m_pending_close_tab < static_cast<int>(m_tabs.size())) {
                m_tabs.erase(m_tabs.begin() + m_pending_close_tab);
                if (m_active_tab >= static_cast<int>(m_tabs.size())) {
                    m_active_tab = static_cast<int>(m_tabs.size()) - 1;
                    m_pending_select_tab = m_active_tab;
                }
                m_canvas_dirty = true;
            }
            m_pending_close_tab = -1;
        };
        m_show_unsaved_dialog = true;
        return;
    }

    m_tabs.erase(m_tabs.begin() + tab_index);
    if (m_active_tab >= static_cast<int>(m_tabs.size())) {
        m_active_tab = static_cast<int>(m_tabs.size()) - 1;
        m_pending_select_tab = m_active_tab;
    }
    m_canvas_dirty = true;
}

bool PixArtPanel::has_any_unsaved_changes() const {
    for (const auto& tab : m_tabs) {
        if (tab->dirty) return true;
    }
    return false;
}

void PixArtPanel::refresh_materials() {
    m_material_cache.clear();

    auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library("default");
    if (lib) {
        for (const auto& mat : lib->get_all_materials()) {
            // Skip empty/unused material slots (same filtering as MaterialEditorPanel)
            if (mat.internal_name.empty()) continue;
            m_material_cache.emplace_back(mat.id, mat.name);
        }
    }

    // Fallback to default names if no library
    if (m_material_cache.empty()) {
        m_material_cache = {
            {uint8_t(0), "air"}, {uint8_t(1), "rock"}, {uint8_t(2), "dirt"}, {uint8_t(3), "sand"},
            {uint8_t(4), "water"}, {uint8_t(5), "lava"}, {uint8_t(6), "ice"}, {uint8_t(7), "steam"},
            {uint8_t(8), "fire"}, {uint8_t(9), "explosive"}
        };
    }

    m_materials_dirty = false;
}

PixArtTab* PixArtPanel::current_tab() {
    if (m_active_tab < 0 || m_active_tab >= static_cast<int>(m_tabs.size())) {
        return nullptr;
    }
    return m_tabs[m_active_tab].get();
}

const PixArtTab* PixArtPanel::current_tab() const {
    if (m_active_tab < 0 || m_active_tab >= static_cast<int>(m_tabs.size())) {
        return nullptr;
    }
    return m_tabs[m_active_tab].get();
}

void PixArtPanel::mark_dirty() {
    if (auto* tab = current_tab()) {
        tab->dirty = true;
    }
}

// ---------------------------------------------------------------------------
// Selection operations
// ---------------------------------------------------------------------------

void PixArtPanel::copy_selection() {
    auto* tab = current_tab();
    if (!tab || !m_selection.valid()) return;

    // If we have a floating selection, copy from that
    if (m_has_floating) {
        m_clipboard.width = m_float_width;
        m_clipboard.height = m_float_height;
        m_clipboard.color_data = m_float_color;
        m_clipboard.material_data = m_float_material;
        return;
    }

    // Otherwise copy from the canvas
    if (tab->active_layer < 0 || tab->active_layer >= tab->doc->art_layer_count()) return;

    int w = m_selection.width;
    int h = m_selection.height;
    m_clipboard.width = w;
    m_clipboard.height = h;
    m_clipboard.color_data.resize(static_cast<size_t>(w) * h * 4);
    m_clipboard.material_data.resize(static_cast<size_t>(w) * h);

    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            int sx = m_selection.x + dx;
            int sy = m_selection.y + dy;
            size_t dst_idx = static_cast<size_t>(dy) * w + dx;

            // Copy color from art layer
            uint8_t rgba[4] = {0, 0, 0, 0};
            if (sx >= 0 && sx < tab->doc->width() && sy >= 0 && sy < tab->doc->height()) {
                tab->doc->get_art_layer_pixel(tab->active_layer, sx, sy, rgba);
                m_clipboard.material_data[dst_idx] = tab->doc->get_material(sx, sy);
            } else {
                m_clipboard.material_data[dst_idx] = 0;
            }
            std::memcpy(m_clipboard.color_data.data() + dst_idx * 4, rgba, 4);
        }
    }
}

void PixArtPanel::cut_selection() {
    copy_selection();
    delete_selection();
    m_selection.clear();  // Remove selection after cutting
}

void PixArtPanel::paste_selection() {
    auto* tab = current_tab();
    if (!tab || !m_clipboard.valid()) return;

    // Create a new layer for the pasted content
    int new_layer = tab->doc->add_art_layer("Pasted Layer");
    tab->active_layer = new_layer;

    // Paste at the selection position, or at (0,0) if no selection
    int paste_x = m_selection.valid() ? m_selection.x : 0;
    int paste_y = m_selection.valid() ? m_selection.y : 0;

    tab->undo.begin_operation(*tab->doc);

    for (int dy = 0; dy < m_clipboard.height; ++dy) {
        for (int dx = 0; dx < m_clipboard.width; ++dx) {
            int tx = paste_x + dx;
            int ty = paste_y + dy;
            if (tx < 0 || tx >= tab->doc->width() || ty < 0 || ty >= tab->doc->height()) continue;

            size_t src_idx = static_cast<size_t>(dy) * m_clipboard.width + dx;

            tab->undo.record_art_pixel(new_layer, tx, ty);
            tab->undo.record_material_pixel(tx, ty);

            tab->doc->set_art_layer_pixel(new_layer, tx, ty,
                                          m_clipboard.color_data.data() + src_idx * 4);
            tab->doc->set_material(tx, ty, m_clipboard.material_data[src_idx]);
        }
    }

    tab->undo.end_operation();

    // Update selection to cover pasted area
    m_selection.x = paste_x;
    m_selection.y = paste_y;
    m_selection.width = m_clipboard.width;
    m_selection.height = m_clipboard.height;

    m_canvas_dirty = true;
    mark_dirty();
}

void PixArtPanel::delete_selection() {
    auto* tab = current_tab();
    if (!tab || !m_selection.valid()) return;
    if (tab->active_layer < 0 || tab->active_layer >= tab->doc->art_layer_count()) return;

    tab->undo.begin_operation(*tab->doc);

    uint8_t transparent[4] = {0, 0, 0, 0};
    for (int dy = 0; dy < m_selection.height; ++dy) {
        for (int dx = 0; dx < m_selection.width; ++dx) {
            int tx = m_selection.x + dx;
            int ty = m_selection.y + dy;
            if (tx < 0 || tx >= tab->doc->width() || ty < 0 || ty >= tab->doc->height()) continue;

            tab->undo.record_art_pixel(tab->active_layer, tx, ty);
            tab->undo.record_material_pixel(tx, ty);

            tab->doc->set_art_layer_pixel(tab->active_layer, tx, ty, transparent);
            tab->doc->set_material(tx, ty, 0);  // Air
        }
    }

    tab->undo.end_operation();
    m_canvas_dirty = true;
    mark_dirty();
}

void PixArtPanel::deselect() {
    // Commit any floating selection first
    if (m_has_floating) {
        commit_floating();
    }
    m_selection.clear();
    m_selecting = false;
}

void PixArtPanel::lift_selection() {
    auto* tab = current_tab();
    if (!tab || !m_selection.valid()) return;
    if (tab->active_layer < 0 || tab->active_layer >= tab->doc->art_layer_count()) return;

    int w = m_selection.width;
    int h = m_selection.height;

    // Store the floating selection data
    m_float_width = w;
    m_float_height = h;
    m_float_x = m_selection.x;
    m_float_y = m_selection.y;
    m_float_color.resize(static_cast<size_t>(w) * h * 4);
    m_float_material.resize(static_cast<size_t>(w) * h);

    // Begin undo operation for the cut
    tab->undo.begin_operation(*tab->doc);

    uint8_t transparent[4] = {0, 0, 0, 0};
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            int sx = m_selection.x + dx;
            int sy = m_selection.y + dy;
            size_t dst_idx = static_cast<size_t>(dy) * w + dx;

            if (sx >= 0 && sx < tab->doc->width() && sy >= 0 && sy < tab->doc->height()) {
                // Copy to floating buffer
                tab->doc->get_art_layer_pixel(tab->active_layer, sx, sy,
                                               m_float_color.data() + dst_idx * 4);
                m_float_material[dst_idx] = tab->doc->get_material(sx, sy);

                // Record for undo
                tab->undo.record_art_pixel(tab->active_layer, sx, sy);
                tab->undo.record_material_pixel(sx, sy);

                // Clear original pixels
                tab->doc->set_art_layer_pixel(tab->active_layer, sx, sy, transparent);
                tab->doc->set_material(sx, sy, 0);  // Air
            } else {
                // Outside canvas - fill with transparent/air
                std::memset(m_float_color.data() + dst_idx * 4, 0, 4);
                m_float_material[dst_idx] = 0;
            }
        }
    }

    tab->undo.end_operation();

    m_has_floating = true;
    m_canvas_dirty = true;
    mark_dirty();
}

void PixArtPanel::commit_floating() {
    auto* tab = current_tab();
    if (!tab || !m_has_floating) return;
    if (tab->active_layer < 0 || tab->active_layer >= tab->doc->art_layer_count()) return;

    tab->undo.begin_operation(*tab->doc);

    for (int dy = 0; dy < m_float_height; ++dy) {
        for (int dx = 0; dx < m_float_width; ++dx) {
            int tx = m_float_x + dx;
            int ty = m_float_y + dy;
            if (tx < 0 || tx >= tab->doc->width() || ty < 0 || ty >= tab->doc->height()) continue;

            size_t src_idx = static_cast<size_t>(dy) * m_float_width + dx;
            const uint8_t* rgba = m_float_color.data() + src_idx * 4;

            // Only stamp non-transparent pixels
            if (rgba[3] > 0) {
                tab->undo.record_art_pixel(tab->active_layer, tx, ty);
                tab->undo.record_material_pixel(tx, ty);

                tab->doc->set_art_layer_pixel(tab->active_layer, tx, ty, rgba);
                tab->doc->set_material(tx, ty, m_float_material[src_idx]);
            }
        }
    }

    tab->undo.end_operation();

    // Clear floating state
    m_has_floating = false;
    m_moving_float = false;
    m_float_color.clear();
    m_float_material.clear();
    m_float_width = 0;
    m_float_height = 0;

    // Update selection to new position
    m_selection.x = m_float_x;
    m_selection.y = m_float_y;

    m_canvas_dirty = true;
    mark_dirty();
}

void PixArtPanel::cancel_floating() {
    auto* tab = current_tab();
    if (!tab || !m_has_floating) return;

    // Just undo the lift operation - this restores original pixels
    if (tab->undo.can_undo()) {
        tab->undo.undo(*tab->doc, &tab->active_layer);
    }

    // Clear floating state
    m_has_floating = false;
    m_moving_float = false;
    m_float_color.clear();
    m_float_material.clear();
    m_float_width = 0;
    m_float_height = 0;

    m_canvas_dirty = true;
}

void PixArtPanel::import_pxg_as_layer() {
    auto* tab = current_tab();
    if (!tab) return;

    auto path = import_pixel_grid_layer();
    if (path.empty()) return;

    // Load the pxg file into a temporary document
    pixart::PixArtDocument temp_doc;
    if (!temp_doc.load(path)) return;

    // Create a new layer and copy pixels from temp doc
    int new_layer = tab->doc->add_art_layer("Imported: " +
        std::filesystem::path(path).filename().string());
    tab->active_layer = new_layer;

    tab->undo.begin_operation(*tab->doc);

    int copy_w = std::min(tab->doc->width(), temp_doc.width());
    int copy_h = std::min(tab->doc->height(), temp_doc.height());

    for (int y = 0; y < copy_h; ++y) {
        for (int x = 0; x < copy_w; ++x) {
            tab->undo.record_art_pixel(new_layer, x, y);
            tab->undo.record_material_pixel(x, y);

            // Copy color from temp doc's first art layer (if available)
            if (temp_doc.art_layer_count() > 0) {
                uint8_t rgba[4];
                temp_doc.get_art_layer_pixel(0, x, y, rgba);
                tab->doc->set_art_layer_pixel(new_layer, x, y, rgba);
            }
            // Copy material
            tab->doc->set_material(x, y, temp_doc.get_material(x, y));
        }
    }

    tab->undo.end_operation();
    m_canvas_dirty = true;
    mark_dirty();
}

void PixArtPanel::import_image_as_layer() {
    auto* tab = current_tab();
    if (!tab) return;

    auto path = import_image_layer();
    if (path.empty()) return;

    // Load image using stb_image
    int img_w, img_h, channels;
    unsigned char* data = stbi_load(path.c_str(), &img_w, &img_h, &channels, 4);  // Force RGBA
    if (!data) return;

    // Create a new layer
    int new_layer = tab->doc->add_art_layer("Imported: " +
        std::filesystem::path(path).filename().string());
    tab->active_layer = new_layer;

    tab->undo.begin_operation(*tab->doc);

    int copy_w = std::min(tab->doc->width(), img_w);
    int copy_h = std::min(tab->doc->height(), img_h);

    for (int y = 0; y < copy_h; ++y) {
        for (int x = 0; x < copy_w; ++x) {
            tab->undo.record_art_pixel(new_layer, x, y);

            size_t src_idx = (static_cast<size_t>(y) * img_w + x) * 4;
            uint8_t rgba[4] = {
                data[src_idx + 0],
                data[src_idx + 1],
                data[src_idx + 2],
                data[src_idx + 3]
            };
            tab->doc->set_art_layer_pixel(new_layer, x, y, rgba);
            // Material stays as Air (0) - don't modify existing materials
        }
    }

    stbi_image_free(data);

    tab->undo.end_operation();
    m_canvas_dirty = true;
    mark_dirty();
}

}
