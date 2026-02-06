#include "PixArtApp.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

// Native file dialogs via Win32 API
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#endif

namespace pixart {

// ---------------------------------------------------------------------------
// Native file dialogs
// ---------------------------------------------------------------------------

std::string PixArtApp::open_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Pixel Grid Files (*.pxg)\0*.pxg\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open Pixel Grid";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return {};
}

std::string PixArtApp::save_file_dialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Pixel Grid Files (*.pxg)\0*.pxg\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save Pixel Grid";
    ofn.lpstrDefExt = "pxg";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return {};
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts
// ---------------------------------------------------------------------------

void PixArtApp::handle_shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return; // Don't capture when typing in input fields

    bool ctrl = io.KeyCtrl;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (m_undo.undo(m_doc)) {
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (m_undo.redo(m_doc)) {
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
        if (m_current_path.empty()) {
            std::string path = save_file_dialog();
            if (!path.empty() && m_doc.save(path)) {
                m_current_path = path;
                m_has_unsaved_changes = false;
            }
        } else {
            if (m_doc.save(m_current_path)) {
                m_has_unsaved_changes = false;
            }
        }
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

void PixArtApp::update() {
    handle_shortcuts();

    // Dockspace over the entire viewport
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                  ImGuiDockNodeFlags_PassthruCentralNode);

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
                if (m_current_path.empty()) {
                    std::string path = save_file_dialog();
                    if (!path.empty() && m_doc.save(path)) {
                        m_current_path = path;
                        m_has_unsaved_changes = false;
                    }
                } else {
                    if (m_doc.save(m_current_path)) {
                        m_has_unsaved_changes = false;
                    }
                }
            }
            if (ImGui::MenuItem("Save As...")) {
                std::string path = save_file_dialog();
                if (!path.empty() && m_doc.save(path)) {
                    m_current_path = path;
                    m_has_unsaved_changes = false;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                try_exit();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_undo.can_undo())) {
                if (m_undo.undo(m_doc)) {
                    m_canvas_dirty = true;
                    m_has_unsaved_changes = true;
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_undo.can_redo())) {
                if (m_undo.redo(m_doc)) {
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

    // Status info
    ImGui::Separator();
    if (m_doc.valid()) {
        ImGui::Text("Grid: %dx%d", m_doc.width(), m_doc.height());
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
// Canvas
// ---------------------------------------------------------------------------

void PixArtApp::build_composite() {
    if (!m_doc.valid()) return;

    int w = m_doc.width();
    int h = m_doc.height();
    size_t pixel_count = static_cast<size_t>(w) * h;
    m_composite.assign(pixel_count * 4, 0); // start fully transparent

    // Find the color layer to use as the "existence mask" for data layers.
    // A pixel only physically exists if the color layer has alpha > 0 there.
    // We search for the first Color layer (normally index 0).
    int color_layer_idx = -1;
    for (int i = 0; i < m_doc.layer_count(); ++i) {
        if (m_doc.layer(i).type == LayerType::Color) {
            color_layer_idx = i;
            break;
        }
    }

    const std::vector<uint8_t>* color_data = nullptr;
    if (color_layer_idx >= 0)
        color_data = &m_doc.layer(color_layer_idx).data;

    // Composite layers from bottom (index 0) to top
    for (int li = 0; li < m_doc.layer_count(); ++li) {
        const auto& layer = m_doc.layer(li);
        if (!layer.visible || layer.opacity <= 0.0f) continue;

        float layer_opacity = std::clamp(layer.opacity, 0.0f, 1.0f);

        for (size_t px = 0; px < pixel_count; ++px) {
            uint8_t sr, sg, sb, sa;

            if (layer.type == LayerType::Color) {
                sr = layer.data[px * 4 + 0];
                sg = layer.data[px * 4 + 1];
                sb = layer.data[px * 4 + 2];
                sa = layer.data[px * 4 + 3];
            } else {
                // Data layers use the color layer's alpha as existence mask:
                // if the pixel is transparent in the color layer, this pixel
                // doesn't physically exist, so skip it.
                if (color_data) {
                    uint8_t color_alpha = (*color_data)[px * 4 + 3];
                    if (color_alpha == 0) continue;
                }

                if (layer.type == LayerType::UInt8) {
                    // Grayscale: 0 = black, 255 = white
                    uint8_t val = layer.data[px];
                    sr = sg = sb = val;
                } else {
                    // Enum: scale grayscale by number of enum values
                    uint8_t val = layer.data[px];
                    int num_values = std::max(1, static_cast<int>(layer.enum_names.size()));
                    uint8_t gray = (num_values > 1)
                        ? static_cast<uint8_t>(val * 255 / (num_values - 1))
                        : 0;
                    sr = sg = sb = gray;
                }
                sa = 255; // data layers are fully opaque where the pixel exists
            }

            // Apply layer opacity to source alpha
            float src_a = (sa / 255.0f) * layer_opacity;
            if (src_a <= 0.0f) continue;

            // Alpha compositing (src over dst)
            size_t off = px * 4;
            float dst_a = m_composite[off + 3] / 255.0f;
            float out_a = src_a + dst_a * (1.0f - src_a);

            if (out_a > 0.0f) {
                m_composite[off + 0] = static_cast<uint8_t>(
                    (sr * src_a + m_composite[off + 0] * dst_a * (1.0f - src_a)) / out_a);
                m_composite[off + 1] = static_cast<uint8_t>(
                    (sg * src_a + m_composite[off + 1] * dst_a * (1.0f - src_a)) / out_a);
                m_composite[off + 2] = static_cast<uint8_t>(
                    (sb * src_a + m_composite[off + 2] * dst_a * (1.0f - src_a)) / out_a);
                m_composite[off + 3] = static_cast<uint8_t>(out_a * 255.0f);
            }
        }
    }
}

void PixArtApp::update_canvas_texture() {
    if (!m_doc.valid()) return;

    int w = m_doc.width();
    int h = m_doc.height();

    if (!m_canvas_tex) {
        glGenTextures(1, &m_canvas_tex);
        glBindTexture(GL_TEXTURE_2D, m_canvas_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    // Build composited image from all visible layers
    build_composite();

    // Upload composite to GPU
    glBindTexture(GL_TEXTURE_2D, m_canvas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, m_composite.data());

    m_canvas_dirty = false;
}

void PixArtApp::render_canvas() {
    if (m_canvas_dirty) {
        update_canvas_texture();
    }

    ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (!m_doc.valid() || !m_canvas_tex) {
        ImGui::Text("No document. Use File > New to create one.");
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f || avail.y < 1.0f) {
        ImGui::End();
        return;
    }
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    float canvas_w = avail.x;
    float canvas_h = avail.y;

    // Invisible button to capture input over the canvas area
    ImGui::InvisibleButton("##canvas", avail,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool canvas_hovered = ImGui::IsItemHovered();

    // --- Zoom ---
    if (canvas_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            float factor = (wheel > 0) ? 1.25f : 0.8f;
            m_view.zoom_towards(factor, mouse.x, mouse.y,
                               canvas_pos.x + canvas_w * 0.5f,
                               canvas_pos.y + canvas_h * 0.5f);
        }
    }

    // --- Pan (middle mouse) ---
    if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        m_pan_state.begin(mouse.x, mouse.y, m_view.pan_x, m_view.pan_y);
    }
    if (m_pan_state.active) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
            ImVec2 mouse = ImGui::GetIO().MousePos;
            m_pan_state.update(mouse.x, mouse.y, m_view.pan_x, m_view.pan_y);
        } else {
            m_pan_state.end();
        }
    }

    // --- Compute grid position using CanvasView ---
    float grid_x0, grid_y0, grid_w, grid_h;
    m_view.get_grid_screen_bounds(canvas_pos.x, canvas_pos.y, canvas_w, canvas_h,
                                   m_doc.width(), m_doc.height(),
                                   grid_x0, grid_y0, grid_w, grid_h);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Clip to canvas area
    dl->PushClipRect(canvas_pos,
                     ImVec2(canvas_pos.x + canvas_w, canvas_pos.y + canvas_h), true);

    // --- Draw checkerboard background (transparency indicator) ---
    {
        float check_size = std::max(m_view.zoom * 0.5f, 4.0f);
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

    // --- Draw the pixel grid texture ---
    dl->AddImage(static_cast<ImTextureID>(static_cast<uintptr_t>(m_canvas_tex)),
                 ImVec2(grid_x0, grid_y0),
                 ImVec2(grid_x0 + grid_w, grid_y0 + grid_h));

    // --- Grid overlay (when zoomed in enough) ---
    if (m_view.zoom >= 4.0f) {
        ImU32 grid_col = IM_COL32(100, 100, 100, 60);
        for (int x = 0; x <= m_doc.width(); ++x) {
            float lx = grid_x0 + x * m_view.zoom;
            dl->AddLine(ImVec2(lx, grid_y0), ImVec2(lx, grid_y0 + grid_h), grid_col);
        }
        for (int y = 0; y <= m_doc.height(); ++y) {
            float ly = grid_y0 + y * m_view.zoom;
            dl->AddLine(ImVec2(grid_x0, ly), ImVec2(grid_x0 + grid_w, ly), grid_col);
        }
    }

    // --- Grid border ---
    dl->AddRect(ImVec2(grid_x0, grid_y0),
                ImVec2(grid_x0 + grid_w, grid_y0 + grid_h),
                IM_COL32(200, 200, 200, 255));

    // --- Hover pixel highlight ---
    m_hover_px = -1;
    m_hover_py = -1;
    if (canvas_hovered && !m_pan_state.active) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int px, py;
        if (m_view.screen_to_pixel(mouse.x, mouse.y,
                                   canvas_pos.x, canvas_pos.y, canvas_w, canvas_h,
                                   m_doc.width(), m_doc.height(), px, py)) {
            m_hover_px = px;
            m_hover_py = py;

            // Draw brush preview
            int r = m_tools.draw_state.brush_size - 1;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy <= r * r) {
                        int bx = px + dx, by = py + dy;
                        if (bx >= 0 && bx < m_doc.width() &&
                            by >= 0 && by < m_doc.height()) {
                            float rx0 = grid_x0 + bx * m_view.zoom;
                            float ry0 = grid_y0 + by * m_view.zoom;
                            dl->AddRectFilled(
                                ImVec2(rx0, ry0),
                                ImVec2(rx0 + m_view.zoom, ry0 + m_view.zoom),
                                IM_COL32(255, 255, 0, 40));
                        }
                    }
                }
            }
        }
    }

    // --- Line tool preview ---
    if (m_tools.active_tool == Tool::Line && m_tools.line_started && canvas_hovered) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int px, py;
        if (m_view.screen_to_pixel(mouse.x, mouse.y,
                                   canvas_pos.x, canvas_pos.y, canvas_w, canvas_h,
                                   m_doc.width(), m_doc.height(), px, py)) {
            float sx0 = grid_x0 + (m_tools.line_start_x + 0.5f) * m_view.zoom;
            float sy0 = grid_y0 + (m_tools.line_start_y + 0.5f) * m_view.zoom;
            float sx1 = grid_x0 + (px + 0.5f) * m_view.zoom;
            float sy1 = grid_y0 + (py + 0.5f) * m_view.zoom;
            dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy1),
                        IM_COL32(255, 255, 0, 180), 2.0f);
        }
    }

    dl->PopClipRect();

    // --- Handle drawing input ---
    handle_canvas_input(canvas_pos.x, canvas_pos.y, canvas_w, canvas_h);

    ImGui::End();
}

void PixArtApp::handle_canvas_input(float cx0, float cy0, float cw, float ch) {
    if (m_pan_state.active) return;

    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    int px, py;
    bool on_grid = m_view.screen_to_pixel(mouse.x, mouse.y, cx0, cy0, cw, ch,
                                          m_doc.width(), m_doc.height(), px, py);

    switch (m_tools.active_tool) {
    case Tool::Pencil:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            m_drawing = true;
            // Begin delta capture for undo
            m_undo.begin_operation(m_doc);
            m_undo.record_brush(m_active_layer, px, py, m_tools.draw_state.brush_size);
            m_tools.apply_pencil(m_doc, m_active_layer, px, py);
            m_last_draw_x = px;
            m_last_draw_y = py;
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
        if (m_drawing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (on_grid && (px != m_last_draw_x || py != m_last_draw_y)) {
                // Record pixels along the line for undo
                int x0 = m_last_draw_x, y0 = m_last_draw_y;
                int x1 = px, y1 = py;
                int dx = std::abs(x1 - x0);
                int dy = std::abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;
                while (true) {
                    m_undo.record_brush(m_active_layer, x0, y0, m_tools.draw_state.brush_size);
                    if (x0 == x1 && y0 == y1) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x0 += sx; }
                    if (e2 < dx)  { err += dx; y0 += sy; }
                }
                // Apply the line
                m_tools.apply_line(m_doc, m_active_layer, m_last_draw_x, m_last_draw_y, px, py);
                m_last_draw_x = px;
                m_last_draw_y = py;
                m_canvas_dirty = true;
                m_has_unsaved_changes = true;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (m_drawing) {
                m_undo.end_operation();
            }
            m_drawing = false;
        }
        break;

    case Tool::Bucket:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            // Begin delta capture - for bucket fill, capture entire layer since
            // we don't know which pixels will change until flood fill runs
            m_undo.begin_operation(m_doc);
            for (int row = 0; row < m_doc.height(); ++row) {
                for (int col = 0; col < m_doc.width(); ++col) {
                    m_undo.record_pixel(m_active_layer, col, row);
                }
            }
            m_tools.apply_bucket(m_doc, m_active_layer, px, py);
            m_undo.end_operation();
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
        }
        break;

    case Tool::Line:
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            if (!m_tools.line_started) {
                m_tools.line_started = true;
                m_tools.line_start_x = px;
                m_tools.line_start_y = py;
            } else {
                // Record pixels along the line for undo
                m_undo.begin_operation(m_doc);
                int x0 = m_tools.line_start_x, y0 = m_tools.line_start_y;
                int x1 = px, y1 = py;
                int dx = std::abs(x1 - x0);
                int dy = std::abs(y1 - y0);
                int sx = (x0 < x1) ? 1 : -1;
                int sy = (y0 < y1) ? 1 : -1;
                int err = dx - dy;
                while (true) {
                    m_undo.record_brush(m_active_layer, x0, y0, m_tools.draw_state.brush_size);
                    if (x0 == x1 && y0 == y1) break;
                    int e2 = 2 * err;
                    if (e2 > -dy) { err -= dy; x0 += sx; }
                    if (e2 < dx)  { err += dx; y0 += sy; }
                }
                m_tools.apply_line(m_doc, m_active_layer, m_tools.line_start_x, m_tools.line_start_y, px, py);
                m_undo.end_operation();
                m_tools.line_started = false;
                m_canvas_dirty = true;
                m_has_unsaved_changes = true;
            }
        }
        // Right-click cancels
        if (m_tools.line_started && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_tools.line_started = false;
        }
        break;
    }
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
            m_doc.swap_layers(i, i + 1);
            if (m_active_layer == i) m_active_layer = i + 1;
            else if (m_active_layer == i + 1) m_active_layer = i;
            m_canvas_dirty = true;
        }
        if (!can_move_up) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move up");

        ImGui::SameLine();
        if (!can_move_down) ImGui::BeginDisabled();
        if (ImGui::SmallButton("v")) {
            m_doc.swap_layers(i, i - 1);
            if (m_active_layer == i) m_active_layer = i - 1;
            else if (m_active_layer == i - 1) m_active_layer = i;
            m_canvas_dirty = true;
        }
        if (!can_move_down) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move down");

        // Delete button (not for the last remaining layer)
        if (m_doc.layer_count() > 1) {
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

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void PixArtApp::render_new_dialog() {
    if (!m_show_new_dialog) return;

    ImGui::OpenPopup("New Document");
    if (ImGui::BeginPopupModal("New Document", &m_show_new_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Width", &m_new_width);
        ImGui::InputInt("Height", &m_new_height);
        m_new_width = std::clamp(m_new_width, 1, 4096);
        m_new_height = std::clamp(m_new_height, 1, 4096);

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Delete old texture
            if (m_canvas_tex) {
                glDeleteTextures(1, &m_canvas_tex);
                m_canvas_tex = 0;
            }
            m_doc.create(m_new_width, m_new_height);
            m_canvas_dirty = true;
            m_active_layer = 0;
            m_current_path.clear();
            m_has_unsaved_changes = false;
            m_undo.clear();
            m_view.reset();
            m_show_new_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_new_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PixArtApp::render_resize_dialog() {
    if (!m_show_resize_dialog) return;

    ImGui::OpenPopup("Resize Grid");
    if (ImGui::BeginPopupModal("Resize Grid", &m_show_resize_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputInt("Width", &m_resize_width);
        ImGui::InputInt("Height", &m_resize_height);
        m_resize_width = std::clamp(m_resize_width, 1, 4096);
        m_resize_height = std::clamp(m_resize_height, 1, 4096);

        if (ImGui::Button("Resize", ImVec2(120, 0))) {
            if (m_canvas_tex) {
                glDeleteTextures(1, &m_canvas_tex);
                m_canvas_tex = 0;
            }
            // Capture all pixels before resize for undo
            m_undo.begin_operation(m_doc);
            for (int ly = 0; ly < m_doc.height(); ++ly) {
                for (int lx = 0; lx < m_doc.width(); ++lx) {
                    for (int li = 0; li < m_doc.layer_count(); ++li) {
                        m_undo.record_pixel(li, lx, ly);
                    }
                }
            }
            m_doc.resize(m_resize_width, m_resize_height);
            m_undo.end_operation();
            m_canvas_dirty = true;
            m_has_unsaved_changes = true;
            m_show_resize_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_resize_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void PixArtApp::render_add_layer_dialog() {
    if (!m_show_add_layer_dialog) return;

    ImGui::OpenPopup("Add Layer");
    if (ImGui::BeginPopupModal("Add Layer", &m_show_add_layer_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", m_new_layer_name, sizeof(m_new_layer_name));

        const char* types[] = {"UInt8", "Enum"};
        ImGui::Combo("Type", &m_new_layer_type, types, 2);

        if (m_new_layer_type == 1) {
            ImGui::Text("Enum values (comma-separated):");
            ImGui::InputText("##enumvals", m_enum_values_buf, sizeof(m_enum_values_buf));
        }

        bool valid_name = (m_new_layer_name[0] != '\0');

        if (!valid_name) ImGui::BeginDisabled();
        if (ImGui::Button("Add", ImVec2(120, 0))) {
            LayerType type = (m_new_layer_type == 1) ? LayerType::Enum : LayerType::UInt8;
            std::vector<std::string> enum_names;

            if (type == LayerType::Enum) {
                // Parse comma-separated enum values
                std::string buf(m_enum_values_buf);
                size_t pos = 0;
                while (pos < buf.size()) {
                    size_t comma = buf.find(',', pos);
                    if (comma == std::string::npos) comma = buf.size();
                    std::string val = buf.substr(pos, comma - pos);
                    // Trim whitespace
                    size_t start = val.find_first_not_of(" \t");
                    size_t end = val.find_last_not_of(" \t");
                    if (start != std::string::npos) {
                        enum_names.push_back(val.substr(start, end - start + 1));
                    }
                    pos = comma + 1;
                }
            }

            m_doc.add_layer(m_new_layer_name, type, enum_names);
            m_canvas_dirty = true;
            m_show_add_layer_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        if (!valid_name) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_show_add_layer_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Unsaved changes handling
// ---------------------------------------------------------------------------

void PixArtApp::try_new_document() {
    if (m_has_unsaved_changes) {
        m_pending_action = PendingAction::New;
        m_show_unsaved_dialog = true;
    } else {
        do_new_document();
    }
}

void PixArtApp::try_open_file() {
    if (m_has_unsaved_changes) {
        m_pending_action = PendingAction::Open;
        m_show_unsaved_dialog = true;
    } else {
        do_open_file();
    }
}

void PixArtApp::try_exit() {
    if (m_has_unsaved_changes) {
        m_pending_action = PendingAction::Exit;
        m_show_unsaved_dialog = true;
    } else {
        m_should_close = true;
    }
}

void PixArtApp::do_new_document() {
    m_show_new_dialog = true;
    m_new_width = 16;
    m_new_height = 16;
}

void PixArtApp::do_open_file() {
    std::string path = open_file_dialog();
    if (!path.empty() && m_doc.load(path)) {
        if (m_canvas_tex) {
            glDeleteTextures(1, &m_canvas_tex);
            m_canvas_tex = 0;
        }
        m_current_path = path;
        m_canvas_dirty = true;
        m_active_layer = 0;
        m_has_unsaved_changes = false;
        m_undo.clear();
    }
}

void PixArtApp::render_unsaved_changes_dialog() {
    if (!m_show_unsaved_dialog) return;

    ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", &m_show_unsaved_dialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Do you want to save before continuing?");
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(100, 0))) {
            // Try to save
            bool saved = false;
            if (m_current_path.empty()) {
                std::string path = save_file_dialog();
                if (!path.empty() && m_doc.save(path)) {
                    m_current_path = path;
                    m_has_unsaved_changes = false;
                    saved = true;
                }
            } else {
                if (m_doc.save(m_current_path)) {
                    m_has_unsaved_changes = false;
                    saved = true;
                }
            }

            if (saved) {
                // Proceed with pending action
                PendingAction action = m_pending_action;
                m_pending_action = PendingAction::None;
                m_show_unsaved_dialog = false;
                ImGui::CloseCurrentPopup();

                if (action == PendingAction::New) do_new_document();
                else if (action == PendingAction::Open) do_open_file();
                else if (action == PendingAction::Exit) m_should_close = true;
            }
            // If save failed, stay in dialog
        }

        ImGui::SameLine();
        if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
            // Proceed without saving
            PendingAction action = m_pending_action;
            m_pending_action = PendingAction::None;
            m_has_unsaved_changes = false;  // Discard changes
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();

            if (action == PendingAction::New) do_new_document();
            else if (action == PendingAction::Open) do_open_file();
            else if (action == PendingAction::Exit) m_should_close = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_pending_action = PendingAction::None;
            m_show_unsaved_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace pixart
