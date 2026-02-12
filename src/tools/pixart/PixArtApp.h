#pragma once

#include "Document.h"
#include "CanvasView.h"
#include "ToolManager.h"
#include "DeltaUndoManager.h"
#include <glad/gl.h>
#include <string>
#include <vector>

namespace pixart {

class PixArtApp {
public:
    void init();
    void init(const std::string& file_path);
    void update();
    void shutdown();

    /// Check if app should close (for main loop)
    bool should_close() const { return m_should_close; }

    /// Request app exit (shows unsaved dialog if needed)
    void try_exit();

    /// Check if there are unsaved changes
    bool has_unsaved_changes() const { return m_has_unsaved_changes; }

private:
    // --- UI panels ---
    void render_menu_bar();
    void render_toolbar();
    void render_canvas();
    void render_layer_panel();
    void render_value_picker();
    void render_new_dialog();
    void render_resize_dialog();
    void render_add_layer_dialog();
    void render_unsaved_changes_dialog();

    // --- Unsaved changes handling ---
    void try_open_file();
    void try_new_document();
    void do_open_file();
    void do_new_document();

    // --- Canvas helpers ---
    void handle_canvas_input(float canvas_x0, float canvas_y0,
                             float canvas_w, float canvas_h);
    void update_canvas_texture();
    void build_composite();

    // --- Keyboard shortcuts ---
    void handle_shortcuts();

    // --- Layout ---
    void setup_default_layout(unsigned int dockspace_id);

    // --- Core components ---
    Document m_doc;
    CanvasView m_view;
    PanState m_pan_state;
    ToolManager m_tools;
    DeltaUndoManager m_undo;

    // --- Rendering state ---
    GLuint m_canvas_tex = 0;
    bool m_canvas_dirty = true;
    std::vector<uint8_t> m_composite;

    // --- Editor state ---
    int m_active_layer = 0;

    // Drawing state (for continuous strokes)
    bool m_drawing = false;
    int m_last_draw_x = -1;
    int m_last_draw_y = -1;

    // Dialogs
    bool m_show_new_dialog = false;
    int m_new_width = 16;
    int m_new_height = 16;

    bool m_show_resize_dialog = false;
    int m_resize_width = 16;
    int m_resize_height = 16;

    bool m_show_add_layer_dialog = false;
    char m_new_layer_name[64] = {};
    int m_new_layer_type = 0;
    char m_enum_values_buf[256] = {};

    // Unsaved changes dialog
    bool m_show_unsaved_dialog = false;
    enum class PendingAction { None, Open, New, Exit };
    PendingAction m_pending_action = PendingAction::None;

    // Exit state
    bool m_should_close = false;

    // File state
    std::string m_current_path;
    bool m_has_unsaved_changes = false;

    // Hover pixel (for status bar)
    int m_hover_px = -1;
    int m_hover_py = -1;

    // Origin placement mode
    bool m_setting_origin = false;

    // Layout
    bool m_first_frame = true;
};

} // namespace pixart
