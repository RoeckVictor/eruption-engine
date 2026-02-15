#pragma once

#include "Document.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <utility>
#include <vector>

namespace pixart {

/// Available drawing tools.
enum class Tool { Pencil, Bucket, Line };

/// Walk a Bresenham line and invoke callback(x, y) for each pixel.
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

/// Current drawing color/value state.
struct DrawState {
    float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};  // RGBA for color layers
    int data_value = 0;                           // Value for UInt8 layers
    int enum_value = 0;                           // Index for Enum layers
    int brush_size = 1;                           // Brush radius for pencil tool
};

/// Manages tool selection and operations.
class ToolManager {
public:
    Tool active_tool = Tool::Pencil;
    DrawState draw_state;

    // Line tool state
    bool line_started = false;
    int line_start_x = 0;
    int line_start_y = 0;

    /// Cancel any in-progress tool operation (like line drawing).
    void cancel() {
        line_started = false;
    }

    /// Stamp a single pixel using current draw state.
    void stamp_pixel(Document& doc, int layer_idx, int px, int py) {
        if (px < 0 || px >= doc.width() || py < 0 || py >= doc.height()) return;
        if (layer_idx < 0 || layer_idx >= doc.layer_count()) return;

        auto& layer = doc.layer(layer_idx);
        if (layer.type == LayerType::Color) {
            uint8_t rgba[4] = {
                static_cast<uint8_t>(draw_state.color[0] * 255.0f),
                static_cast<uint8_t>(draw_state.color[1] * 255.0f),
                static_cast<uint8_t>(draw_state.color[2] * 255.0f),
                static_cast<uint8_t>(draw_state.color[3] * 255.0f)
            };
            doc.set_pixel(layer_idx, px, py, rgba);
        } else {
            uint8_t val = (layer.type == LayerType::Enum)
                ? static_cast<uint8_t>(draw_state.enum_value)
                : static_cast<uint8_t>(draw_state.data_value);
            doc.set_pixel(layer_idx, px, py, &val);
        }
    }

    /// Apply pencil tool at a point (with brush size).
    void apply_pencil(Document& doc, int layer_idx, int px, int py) {
        int r = draw_state.brush_size - 1;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    stamp_pixel(doc, layer_idx, px + dx, py + dy);
                }
            }
        }
    }

    /// Apply line tool between two points.
    void apply_line(Document& doc, int layer_idx, int x0, int y0, int x1, int y1) {
        bresenham_line(x0, y0, x1, y1, [&](int x, int y) {
            apply_pencil(doc, layer_idx, x, y);
        });
    }

    /// Apply bucket fill at a point.
    void apply_bucket(Document& doc, int layer_idx, int px, int py) {
        apply_bucket(doc, layer_idx, px, py, [](int, int) {});
    }

    /// Apply bucket fill at a point, calling pre_pixel(x, y) before each pixel is modified.
    template<typename Func>
    void apply_bucket(Document& doc, int layer_idx, int px, int py, Func&& pre_pixel) {
        if (!doc.valid()) return;
        if (layer_idx < 0 || layer_idx >= doc.layer_count()) return;
        if (px < 0 || px >= doc.width() || py < 0 || py >= doc.height()) return;

        auto& layer = doc.layer(layer_idx);
        int ch = layer.channels;
        int w = doc.width();
        int h = doc.height();

        // Stack-allocated buffers (max 4 channels for Color type)
        uint8_t target[4] = {};
        uint8_t fill[4] = {};
        doc.get_pixel(layer_idx, px, py, target);

        if (layer.type == LayerType::Color) {
            fill[0] = static_cast<uint8_t>(draw_state.color[0] * 255.0f);
            fill[1] = static_cast<uint8_t>(draw_state.color[1] * 255.0f);
            fill[2] = static_cast<uint8_t>(draw_state.color[2] * 255.0f);
            fill[3] = static_cast<uint8_t>(draw_state.color[3] * 255.0f);
        } else if (layer.type == LayerType::Enum) {
            fill[0] = static_cast<uint8_t>(draw_state.enum_value);
        } else {
            fill[0] = static_cast<uint8_t>(draw_state.data_value);
        }

        // Don't fill if target == fill
        if (std::memcmp(target, fill, ch) == 0) return;

        // BFS flood fill (4-connectivity)
        std::vector<bool> visited(static_cast<size_t>(w) * h, false);
        std::queue<std::pair<int, int>> queue;
        queue.push({px, py});
        visited[py * w + px] = true;

        while (!queue.empty()) {
            auto [cx, cy] = queue.front();
            queue.pop();

            pre_pixel(cx, cy);
            doc.set_pixel(layer_idx, cx, cy, fill);

            const int dx[] = {0, 0, -1, 1};
            const int dy[] = {-1, 1, 0, 0};
            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                if (visited[ny * w + nx]) continue;

                uint8_t neighbor[4] = {};
                doc.get_pixel(layer_idx, nx, ny, neighbor);
                if (std::memcmp(neighbor, target, ch) == 0) {
                    visited[ny * w + nx] = true;
                    queue.push({nx, ny});
                }
            }
        }
    }

    /// Pick color/value from a pixel (eyedropper).
    void pick_from_pixel(const Document& doc, int layer_idx, int px, int py) {
        if (!doc.valid() || layer_idx < 0 || layer_idx >= doc.layer_count()) return;

        const auto& layer = doc.layer(layer_idx);
        if (layer.type == LayerType::Color) {
            uint8_t rgba[4];
            doc.get_pixel(layer_idx, px, py, rgba);
            draw_state.color[0] = rgba[0] / 255.0f;
            draw_state.color[1] = rgba[1] / 255.0f;
            draw_state.color[2] = rgba[2] / 255.0f;
            draw_state.color[3] = rgba[3] / 255.0f;
        } else if (layer.type == LayerType::Enum) {
            uint8_t val;
            doc.get_pixel(layer_idx, px, py, &val);
            draw_state.enum_value = val;
        } else {
            uint8_t val;
            doc.get_pixel(layer_idx, px, py, &val);
            draw_state.data_value = val;
        }
    }
};

} // namespace pixart
