#pragma once

#include "PixArtDocument.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <utility>
#include <vector>
#include <functional>

namespace editor::pixart {

enum class Tool {
    Pencil,
    Bucket,
    Line,
    Eraser,
    Pipette,
    Select
};

struct Selection {
    int x = 0, y = 0;
    int width = 0, height = 0;

    bool valid() const { return width > 0 && height > 0; }
    void clear() { x = y = width = height = 0; }

    void normalize() {
        if (width < 0) { x += width; width = -width; }
        if (height < 0) { y += height; height = -height; }
    }

    bool contains(int px, int py) const {
        return valid() && px >= x && px < x + width && py >= y && py < y + height;
    }
};

struct Clipboard {
    std::vector<uint8_t> color_data;
    std::vector<uint8_t> material_data;
    int width = 0;
    int height = 0;

    bool valid() const { return width > 0 && height > 0 && !color_data.empty(); }
    void clear() { color_data.clear(); material_data.clear(); width = height = 0; }
};

template<typename Func>
void bresenham_line(int x0, int y0, int x1, int y1, Func&& func) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        func(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

struct DrawState {
    float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    uint8_t material_id = 0;
    int brush_size = 1;

    bool allow_material_edit = true;

    bool erase_color = true;
    bool erase_material = true;

    bool pick_color = true;
    bool pick_material = true;
};

class ToolManager {
public:
    Tool active_tool = Tool::Pencil;
    DrawState draw_state;

    bool line_started = false;
    int line_start_x = 0;
    int line_start_y = 0;

    void cancel() {
        line_started = false;
    }

    void get_color_bytes(uint8_t* out) const {
        out[0] = static_cast<uint8_t>(draw_state.color[0] * 255.0f);
        out[1] = static_cast<uint8_t>(draw_state.color[1] * 255.0f);
        out[2] = static_cast<uint8_t>(draw_state.color[2] * 255.0f);
        out[3] = static_cast<uint8_t>(draw_state.color[3] * 255.0f);
    }

    void stamp_pixel(PixArtDocument& doc, int layer_idx, int px, int py) {
        if (px < 0 || px >= doc.width() || py < 0 || py >= doc.height()) return;
        if (layer_idx < 0 || layer_idx >= doc.art_layer_count()) return;

        uint8_t rgba[4];
        get_color_bytes(rgba);
        doc.set_art_layer_pixel(layer_idx, px, py, rgba);

        if (draw_state.allow_material_edit) {
            doc.set_material(px, py, draw_state.material_id);
        }
    }

    void stamp_eraser_pixel(PixArtDocument& doc, int layer_idx, int px, int py) {
        if (px < 0 || px >= doc.width() || py < 0 || py >= doc.height()) return;

        if (draw_state.erase_color && layer_idx >= 0 && layer_idx < doc.art_layer_count()) {
            uint8_t rgba[4] = {0, 0, 0, 0};
            doc.set_art_layer_pixel(layer_idx, px, py, rgba);
        }

        if (draw_state.erase_material && draw_state.allow_material_edit) {
            doc.set_material(px, py, 0);
        }
    }

    void apply_pencil(PixArtDocument& doc, int layer_idx, int px, int py) {
        int r = draw_state.brush_size - 1;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    stamp_pixel(doc, layer_idx, px + dx, py + dy);
                }
            }
        }
    }

    void apply_eraser(PixArtDocument& doc, int layer_idx, int px, int py) {
        int r = draw_state.brush_size - 1;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    stamp_eraser_pixel(doc, layer_idx, px + dx, py + dy);
                }
            }
        }
    }

    void apply_line(PixArtDocument& doc, int layer_idx, int x0, int y0, int x1, int y1) {
        bresenham_line(x0, y0, x1, y1, [&](int x, int y) {
            apply_pencil(doc, layer_idx, x, y);
        });
    }

    void apply_bucket(PixArtDocument& doc, int layer_idx, int px, int py) {
        apply_bucket(doc, layer_idx, px, py, true, true, [](int, int) {});
    }

    void apply_bucket(PixArtDocument& doc, int layer_idx, int px, int py,
                      bool match_color, bool match_material) {
        apply_bucket(doc, layer_idx, px, py, match_color, match_material, [](int, int) {});
    }

    template<typename Func>
    void apply_bucket(PixArtDocument& doc, int layer_idx, int px, int py,
                      bool match_color, bool match_material, Func&& pre_pixel) {
        if (!doc.valid()) return;
        if (layer_idx < 0 || layer_idx >= doc.art_layer_count()) return;
        if (px < 0 || px >= doc.width() || py < 0 || py >= doc.height()) return;

        // Need at least one matching criterion
        if (!match_color && !match_material) return;

        int w = doc.width();
        int h = doc.height();

        // Get target color and material at clicked pixel
        uint8_t target_color[4];
        doc.get_art_layer_pixel(layer_idx, px, py, target_color);
        uint8_t target_material = doc.get_material(px, py);

        // Get fill color and material
        uint8_t fill_color[4];
        get_color_bytes(fill_color);
        uint8_t fill_material = draw_state.material_id;

        // Check if there's anything to fill (at least one of the matched criteria must differ)
        bool color_same = (std::memcmp(target_color, fill_color, 4) == 0);
        bool material_same = (target_material == fill_material);

        if (match_color && match_material) {
            if (color_same && material_same) return;
        } else if (match_color) {
            if (color_same) return;
        } else if (match_material) {
            if (material_same) return;
        }

        // Lambda to check if a pixel matches the target (for flood fill propagation)
        auto matches_target = [&](int x, int y) -> bool {
            bool matches = true;
            if (match_color) {
                uint8_t c[4];
                doc.get_art_layer_pixel(layer_idx, x, y, c);
                matches = matches && (std::memcmp(c, target_color, 4) == 0);
            }
            if (match_material) {
                uint8_t m = doc.get_material(x, y);
                matches = matches && (m == target_material);
            }
            return matches;
        };

        // BFS flood fill (4-connectivity)
        std::vector<bool> visited(static_cast<size_t>(w) * h, false);
        std::queue<std::pair<int, int>> queue;
        queue.push({px, py});
        visited[py * w + px] = true;

        while (!queue.empty()) {
            auto [cx, cy] = queue.front();
            queue.pop();

            pre_pixel(cx, cy);
            // Set color always, set material only if allowed (Main layer)
            doc.set_art_layer_pixel(layer_idx, cx, cy, fill_color);
            if (draw_state.allow_material_edit) {
                doc.set_material(cx, cy, fill_material);
            }

            const int dx[] = {0, 0, -1, 1};
            const int dy[] = {-1, 1, 0, 0};
            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (visited[ny * w + nx]) continue;

                if (matches_target(nx, ny)) {
                    visited[ny * w + nx] = true;
                    queue.push({nx, ny});
                }
            }
        }
    }

    /// Pick color and/or material from pixel (respects pick_color and pick_material flags).
    void pick_from_pixel(const PixArtDocument& doc, int layer_idx, int px, int py) {
        if (!doc.valid()) return;
        if (px < 0 || px >= doc.width() || py < 0 || py >= doc.height()) return;

        // Pick color from art layer
        if (draw_state.pick_color && layer_idx >= 0 && layer_idx < doc.art_layer_count()) {
            uint8_t rgba[4];
            doc.get_art_layer_pixel(layer_idx, px, py, rgba);
            draw_state.color[0] = rgba[0] / 255.0f;
            draw_state.color[1] = rgba[1] / 255.0f;
            draw_state.color[2] = rgba[2] / 255.0f;
            draw_state.color[3] = rgba[3] / 255.0f;
        }

        // Pick material
        if (draw_state.pick_material) {
            draw_state.material_id = doc.get_material(px, py);
        }
    }
};

}
