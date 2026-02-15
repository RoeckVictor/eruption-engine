#include "PixArtApp.h"

#include "engine/core/Logger.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace pixart {

// ---------------------------------------------------------------------------
// Keyboard shortcuts
// ---------------------------------------------------------------------------

void PixArtApp::handle_shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return; // Don't capture when typing in input fields

    bool ctrl = io.KeyCtrl;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (m_undo.undo(m_doc, &m_active_layer)) {
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (m_undo.redo(m_doc, &m_active_layer)) {
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        try_new_document();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        try_open_file();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        try_save_document();
    }
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

void PixArtApp::init() {
    // Start with a default 16x16 document
    m_doc.create(16, 16);
    m_canvas_dirty = true;
}

void PixArtApp::init(const std::string& file_path) {
    if (!file_path.empty() && m_doc.load(file_path)) {
        m_current_path = file_path;
        m_canvas_dirty = true;
        m_active_layer = 0;
        m_has_unsaved_changes = false;
    } else {
        // Fall back to default document if file couldn't be loaded
        if (!file_path.empty()) {
            engine::Logger::instance().warning("PixArt", "Failed to load '%s', creating new document", file_path.c_str());
        }
        m_doc.create(16, 16);
        m_canvas_dirty = true;
    }
}

void PixArtApp::update() {
    handle_shortcuts();

    // Dockspace over the entire viewport
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                  ImGuiDockNodeFlags_PassthruCentralNode);

    if (m_first_frame) {
        setup_default_layout(dockspace_id);
        m_first_frame = false;
    }

    render_menu_bar();
    render_toolbar();
    render_canvas();
    render_layer_panel();
    render_value_picker();

    // Popups
    render_new_dialog();
    render_resize_dialog();
    render_add_layer_dialog();
    render_unsaved_changes_dialog();
}

void PixArtApp::shutdown() {
    if (m_canvas_tex) {
        glDeleteTextures(1, &m_canvas_tex);
        m_canvas_tex = 0;
    }
}

void PixArtApp::setup_default_layout(unsigned int dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    // Split top strip for Toolbar (~15% height)
    ImGuiID dock_top, dock_main;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.15f, &dock_top, &dock_main);

    // Split right column from center (~22% width)
    ImGuiID dock_right, dock_center;
    ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.22f, &dock_right, &dock_center);

    // Split right column into Layers (top) and Picker (bottom, ~45% of right)
    ImGuiID dock_right_top, dock_right_bottom;
    ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.45f, &dock_right_bottom, &dock_right_top);

    // Dock the panels
    ImGui::DockBuilderDockWindow("Toolbar", dock_top);
    ImGui::DockBuilderDockWindow("Canvas", dock_center);
    ImGui::DockBuilderDockWindow("Layers", dock_right_top);
    ImGui::DockBuilderDockWindow("Picker", dock_right_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace pixart
