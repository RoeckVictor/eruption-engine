#include "PixArtApp.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace pixart {

// ---------------------------------------------------------------------------
// Compositing
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

// ---------------------------------------------------------------------------
// Canvas rendering
// ---------------------------------------------------------------------------

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

    // --- Origin crosshair marker ---
    {
        int ox = m_doc.origin_x();
        int oy = m_doc.origin_y();
        float ocx = grid_x0 + (ox + 0.5f) * m_view.zoom;  // Center of origin pixel
        float ocy = grid_y0 + (oy + 0.5f) * m_view.zoom;
        float arm = std::max(m_view.zoom * 1.5f, 6.0f);    // Crosshair arm length
        ImU32 origin_col = IM_COL32(0, 220, 220, 220);      // Cyan
        dl->AddLine(ImVec2(ocx - arm, ocy), ImVec2(ocx + arm, ocy), origin_col, 2.0f);
        dl->AddLine(ImVec2(ocx, ocy - arm), ImVec2(ocx, ocy + arm), origin_col, 2.0f);
        // Small diamond at center
        float d = std::max(m_view.zoom * 0.3f, 3.0f);
        dl->AddQuadFilled(ImVec2(ocx, ocy - d), ImVec2(ocx + d, ocy),
                          ImVec2(ocx, ocy + d), ImVec2(ocx - d, ocy), origin_col);
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

// ---------------------------------------------------------------------------
// Canvas input handling
// ---------------------------------------------------------------------------

void PixArtApp::handle_canvas_input(float cx0, float cy0, float cw, float ch) {
    if (m_pan_state.active) return;

    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    int px, py;
    bool on_grid = m_view.screen_to_pixel(mouse.x, mouse.y, cx0, cy0, cw, ch,
                                          m_doc.width(), m_doc.height(), px, py);

    // Origin placement mode: click to set origin and exit mode
    if (m_setting_origin) {
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && on_grid) {
            m_undo.push_origin_change(m_doc, m_doc.origin_x(), m_doc.origin_y());
            m_doc.set_origin(px, py);
            m_setting_origin = false;
            m_has_unsaved_changes = true;
            m_canvas_dirty = true;
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_setting_origin = false;  // Cancel
        }
        return;  // Don't process drawing tools while setting origin
    }

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
                bresenham_line(m_last_draw_x, m_last_draw_y, px, py, [&](int x, int y) {
                    m_undo.record_brush(m_active_layer, x, y, m_tools.draw_state.brush_size);
                });
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
            // Begin delta capture - record only pixels that the flood fill
            // will actually modify via a pre-pixel callback
            m_undo.begin_operation(m_doc);
            m_tools.apply_bucket(m_doc, m_active_layer, px, py, [&](int fx, int fy) {
                m_undo.record_pixel(m_active_layer, fx, fy);
            });
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
                bresenham_line(m_tools.line_start_x, m_tools.line_start_y, px, py, [&](int x, int y) {
                    m_undo.record_brush(m_active_layer, x, y, m_tools.draw_state.brush_size);
                });
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

} // namespace pixart
