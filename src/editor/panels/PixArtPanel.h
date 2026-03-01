#pragma once

#include "Panel.h"
#include "editor/pixart/PixArtDocument.h"
#include "editor/pixart/PixArtTools.h"
#include "editor/pixart/CanvasView.h"
#include "editor/pixart/UndoManager.h"
#include "engine/graphics/Texture.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace editor {

class EditorContext;

struct PixArtTab {
    std::string path;
    std::unique_ptr<pixart::PixArtDocument> doc;
    pixart::UndoManager undo;
    pixart::CanvasView view;
    bool dirty = false;
    int active_layer = 0;

    PixArtTab() : doc(std::make_unique<pixart::PixArtDocument>()) {}
};

// Panel for editing .pxg pixel grid files integrated into the editor.
class PixArtPanel : public Panel {
public:
    explicit PixArtPanel(EditorContext& context);
    ~PixArtPanel() override;

    void on_open() override;
    void on_close() override;
    void on_gui() override;
    bool on_close_requested() override;

    void open_file(const std::string& path);
    void close_tab(int tab_index);

    bool has_any_unsaved_changes() const;

    void refresh_materials();

private:
    void render_top_bar();
    void render_tab_bar();
    void render_toolbar();
    void render_canvas();
    void render_layer_panel();
    void render_color_picker();
    void render_material_picker();
    void render_hover_info();

    void render_new_dialog();
    void render_resize_dialog();
    void render_unsaved_dialog();

    void handle_canvas_input(float cx0, float cy0, float cw, float ch);
    void handle_shortcuts();

    void update_canvas_texture();
    void build_composite();

    bool save_current_document();
    bool save_document_as();
    void create_new_document(int width, int height);

    PixArtTab* current_tab();
    const PixArtTab* current_tab() const;
    void mark_dirty();

    EditorContext& m_context;

    std::vector<std::unique_ptr<PixArtTab>> m_tabs;
    int m_active_tab = -1;
    int m_pending_select_tab = -1;

    engine::graphics::Texture m_canvas_tex;
    std::vector<uint8_t> m_composite;
    bool m_canvas_dirty = true;

    pixart::ToolManager m_tools;
    pixart::PanState m_pan_state;
    bool m_drawing = false;
    int m_last_draw_x = -1;
    int m_last_draw_y = -1;

    enum class ViewMode { Color, Material, Both };
    ViewMode m_view_mode = ViewMode::Color;

    int m_hover_px = -1;
    int m_hover_py = -1;

    bool m_show_new_dialog = false;
    int m_new_width = 16;
    int m_new_height = 16;

    bool m_show_resize_dialog = false;
    int m_resize_width = 16;
    int m_resize_height = 16;

    bool m_show_unsaved_dialog = false;
    std::function<void()> m_pending_action;
    int m_pending_close_tab = -1;

    std::vector<std::pair<uint8_t, std::string>> m_material_cache;
    bool m_materials_dirty = true;

    bool m_setting_origin = false;

    int m_renaming_layer = -1;
    std::string m_rename_buffer;

    pixart::Selection m_selection;
    pixart::Clipboard m_clipboard;
    bool m_selecting = false;
    int m_select_start_x = 0;
    int m_select_start_y = 0;

    bool m_has_floating = false;
    std::vector<uint8_t> m_float_color;
    std::vector<uint8_t> m_float_material;
    int m_float_width = 0;
    int m_float_height = 0;
    int m_float_x = 0;
    int m_float_y = 0;
    bool m_moving_float = false;
    int m_move_offset_x = 0;
    int m_move_offset_y = 0;

    void copy_selection();
    void cut_selection();
    void paste_selection();
    void delete_selection();
    void deselect();
    void lift_selection();
    void commit_floating();
    void cancel_floating();

    void import_pxg_as_layer();
    void import_image_as_layer();
};

}
