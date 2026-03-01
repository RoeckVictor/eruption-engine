#pragma once

#include "PixArtDocument.h"
#include <deque>
#include <optional>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace editor::pixart {

enum class UndoDataType {
    ArtLayer,
    Material
};

// A delta-based undo snapshot that only stores changed pixels
// Much more memory-efficient than storing full layer copies
struct DeltaSnapshot {
    struct LayerDelta {
        std::unordered_map<int, std::vector<uint8_t>> changed_pixels;
    };

    std::unordered_map<int, LayerDelta> art_layer_deltas;

    LayerDelta material_delta;

    int doc_width = 0;
    int doc_height = 0;

    struct OriginChange { int old_x, old_y; };
    std::optional<OriginChange> origin_change;

    struct LayerSwap { int index_a, index_b; };
    std::optional<LayerSwap> layer_swap;

    bool empty() const {
        if (origin_change.has_value() || layer_swap.has_value()) return false;
        if (!material_delta.changed_pixels.empty()) return false;
        for (const auto& [layer_idx, delta] : art_layer_deltas) {
            if (!delta.changed_pixels.empty()) return false;
        }
        return true;
    }

    size_t memory_usage() const {
        size_t total = sizeof(*this);
        for (const auto& [layer_idx, delta] : art_layer_deltas) {
            total += sizeof(layer_idx) + sizeof(LayerDelta);
            for (const auto& [px_idx, data] : delta.changed_pixels) {
                total += sizeof(px_idx) + sizeof(std::vector<uint8_t>) + data.size();
            }
        }
        for (const auto& [px_idx, data] : material_delta.changed_pixels) {
            total += sizeof(px_idx) + sizeof(std::vector<uint8_t>) + data.size();
        }
        return total;
    }
};

class DeltaCapturer {
public:
    void begin_capture(const PixArtDocument& doc) {
        m_doc = &doc;
        m_expected_width = doc.width();
        m_expected_height = doc.height();
        m_snapshot = DeltaSnapshot{};
        m_snapshot.doc_width = doc.width();
        m_snapshot.doc_height = doc.height();
        m_capturing = true;
    }

    void capture_art_pixel(int layer_idx, int x, int y) {
        if (!m_capturing || !m_doc || !m_doc->valid()) return;
        if (m_doc->width() != m_expected_width || m_doc->height() != m_expected_height) {
            m_capturing = false;
            m_doc = nullptr;
            return;
        }
        if (layer_idx < 0 || layer_idx >= m_doc->art_layer_count()) return;
        if (x < 0 || x >= m_doc->width() || y < 0 || y >= m_doc->height()) return;

        int pixel_idx = y * m_doc->width() + x;
        auto& layer_delta = m_snapshot.art_layer_deltas[layer_idx];
        if (layer_delta.changed_pixels.count(pixel_idx)) return;

        std::vector<uint8_t> old_data(4);
        m_doc->get_art_layer_pixel(layer_idx, x, y, old_data.data());
        layer_delta.changed_pixels[pixel_idx] = std::move(old_data);
    }

    void capture_material_pixel(int x, int y) {
        if (!m_capturing || !m_doc || !m_doc->valid()) return;
        if (m_doc->width() != m_expected_width || m_doc->height() != m_expected_height) {
            m_capturing = false;
            m_doc = nullptr;
            return;
        }
        if (x < 0 || x >= m_doc->width() || y < 0 || y >= m_doc->height()) return;

        int pixel_idx = y * m_doc->width() + x;
        if (m_snapshot.material_delta.changed_pixels.count(pixel_idx)) return;

        std::vector<uint8_t> old_data(1);
        old_data[0] = m_doc->get_material(x, y);
        m_snapshot.material_delta.changed_pixels[pixel_idx] = std::move(old_data);
    }

    void capture_art_brush(int layer_idx, int cx, int cy, int radius) {
        int r = radius - 1;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    capture_art_pixel(layer_idx, cx + dx, cy + dy);
                }
            }
        }
    }

    void capture_material_brush(int cx, int cy, int radius) {
        int r = radius - 1;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    capture_material_pixel(cx + dx, cy + dy);
                }
            }
        }
    }

    DeltaSnapshot end_capture() {
        m_capturing = false;
        m_doc = nullptr;
        return std::move(m_snapshot);
    }

    bool is_capturing() const { return m_capturing; }

private:
    const PixArtDocument* m_doc = nullptr;
    int m_expected_width = 0;
    int m_expected_height = 0;
    DeltaSnapshot m_snapshot;
    bool m_capturing = false;
};

class UndoManager {
public:
    static constexpr int DEFAULT_MAX_UNDO = 128;
    static constexpr size_t DEFAULT_MAX_MEMORY = 256 * 1024 * 1024;

    explicit UndoManager(int max_steps = DEFAULT_MAX_UNDO,
                         size_t max_memory = DEFAULT_MAX_MEMORY)
        : m_max_steps(max_steps), m_max_memory(max_memory) {}

    void begin_operation(const PixArtDocument& doc) {
        m_capturer.begin_capture(doc);
    }

    void record_art_pixel(int layer_idx, int x, int y) {
        m_capturer.capture_art_pixel(layer_idx, x, y);
    }

    void record_art_brush(int layer_idx, int cx, int cy, int radius) {
        m_capturer.capture_art_brush(layer_idx, cx, cy, radius);
    }

    void record_material_pixel(int x, int y) {
        m_capturer.capture_material_pixel(x, y);
    }

    void record_material_brush(int cx, int cy, int radius) {
        m_capturer.capture_material_brush(cx, cy, radius);
    }

    void end_operation() {
        if (!m_capturer.is_capturing()) return;

        auto snapshot = m_capturer.end_capture();
        if (snapshot.empty()) return;

        m_undo_stack.push_back(std::move(snapshot));
        m_current_memory += m_undo_stack.back().memory_usage();

        // Enforce limits
        while (static_cast<int>(m_undo_stack.size()) > m_max_steps ||
               m_current_memory > m_max_memory) {
            if (m_undo_stack.empty()) break;
            m_current_memory -= m_undo_stack.front().memory_usage();
            m_undo_stack.pop_front();
        }

        // Clear redo stack on new action
        m_current_memory -= redo_memory();
        m_redo_stack.clear();
    }

    void push_origin_change(const PixArtDocument& doc, int old_x, int old_y) {
        DeltaSnapshot snapshot;
        snapshot.doc_width = doc.width();
        snapshot.doc_height = doc.height();
        snapshot.origin_change = DeltaSnapshot::OriginChange{old_x, old_y};
        push_snapshot(std::move(snapshot));
    }

    void push_layer_swap(const PixArtDocument& doc, int index_a, int index_b) {
        DeltaSnapshot snapshot;
        snapshot.doc_width = doc.width();
        snapshot.doc_height = doc.height();
        snapshot.layer_swap = DeltaSnapshot::LayerSwap{index_a, index_b};
        push_snapshot(std::move(snapshot));
    }

    void cancel_operation() {
        if (m_capturer.is_capturing()) {
            m_capturer.end_capture();
        }
    }

    bool undo(PixArtDocument& doc, int* active_layer = nullptr) {
        if (m_undo_stack.empty() || !doc.valid()) return false;

        auto& snapshot = m_undo_stack.back();
        int snap_w = snapshot.doc_width > 0 ? snapshot.doc_width : doc.width();

        // Capture current state for redo
        DeltaSnapshot redo_snapshot;
        redo_snapshot.doc_width = doc.width();
        redo_snapshot.doc_height = doc.height();

        // Handle structural changes
        if (snapshot.origin_change) {
            redo_snapshot.origin_change = DeltaSnapshot::OriginChange{
                doc.origin_x(), doc.origin_y()};
            doc.set_origin(snapshot.origin_change->old_x, snapshot.origin_change->old_y);
        }
        if (snapshot.layer_swap) {
            redo_snapshot.layer_swap = snapshot.layer_swap;
            int a = snapshot.layer_swap->index_a;
            int b = snapshot.layer_swap->index_b;
            doc.swap_art_layers(a, b);
            if (active_layer) {
                if (*active_layer == a) *active_layer = b;
                else if (*active_layer == b) *active_layer = a;
            }
        }

        // Handle art layer deltas
        for (const auto& [layer_idx, layer_delta] : snapshot.art_layer_deltas) {
            if (layer_idx >= doc.art_layer_count()) continue;

            auto& redo_layer = redo_snapshot.art_layer_deltas[layer_idx];
            for (const auto& [pixel_idx, old_data] : layer_delta.changed_pixels) {
                int x = pixel_idx % snap_w;
                int y = pixel_idx / snap_w;
                if (x >= doc.width() || y >= doc.height()) continue;

                std::vector<uint8_t> current_data(4);
                doc.get_art_layer_pixel(layer_idx, x, y, current_data.data());
                redo_layer.changed_pixels[pixel_idx] = std::move(current_data);

                doc.set_art_layer_pixel(layer_idx, x, y, old_data.data());
            }
        }

        // Handle material deltas
        for (const auto& [pixel_idx, old_data] : snapshot.material_delta.changed_pixels) {
            int x = pixel_idx % snap_w;
            int y = pixel_idx / snap_w;
            if (x >= doc.width() || y >= doc.height()) continue;

            std::vector<uint8_t> current_data(1);
            current_data[0] = doc.get_material(x, y);
            redo_snapshot.material_delta.changed_pixels[pixel_idx] = std::move(current_data);

            doc.set_material(x, y, old_data[0]);
        }

        // Move to redo stack
        m_current_memory -= snapshot.memory_usage();
        m_undo_stack.pop_back();
        m_redo_stack.push_back(std::move(redo_snapshot));
        m_current_memory += m_redo_stack.back().memory_usage();

        return true;
    }

    bool redo(PixArtDocument& doc, int* active_layer = nullptr) {
        if (m_redo_stack.empty() || !doc.valid()) return false;

        auto& snapshot = m_redo_stack.back();
        int snap_w = snapshot.doc_width > 0 ? snapshot.doc_width : doc.width();

        // Capture current state for undo
        DeltaSnapshot undo_snapshot;
        undo_snapshot.doc_width = doc.width();
        undo_snapshot.doc_height = doc.height();

        // Handle structural changes
        if (snapshot.origin_change) {
            undo_snapshot.origin_change = DeltaSnapshot::OriginChange{
                doc.origin_x(), doc.origin_y()};
            doc.set_origin(snapshot.origin_change->old_x, snapshot.origin_change->old_y);
        }
        if (snapshot.layer_swap) {
            undo_snapshot.layer_swap = snapshot.layer_swap;
            int a = snapshot.layer_swap->index_a;
            int b = snapshot.layer_swap->index_b;
            doc.swap_art_layers(a, b);
            if (active_layer) {
                if (*active_layer == a) *active_layer = b;
                else if (*active_layer == b) *active_layer = a;
            }
        }

        // Handle art layer deltas
        for (const auto& [layer_idx, layer_delta] : snapshot.art_layer_deltas) {
            if (layer_idx >= doc.art_layer_count()) continue;

            auto& undo_layer = undo_snapshot.art_layer_deltas[layer_idx];
            for (const auto& [pixel_idx, old_data] : layer_delta.changed_pixels) {
                int x = pixel_idx % snap_w;
                int y = pixel_idx / snap_w;
                if (x >= doc.width() || y >= doc.height()) continue;

                std::vector<uint8_t> current_data(4);
                doc.get_art_layer_pixel(layer_idx, x, y, current_data.data());
                undo_layer.changed_pixels[pixel_idx] = std::move(current_data);

                doc.set_art_layer_pixel(layer_idx, x, y, old_data.data());
            }
        }

        // Handle material deltas
        for (const auto& [pixel_idx, old_data] : snapshot.material_delta.changed_pixels) {
            int x = pixel_idx % snap_w;
            int y = pixel_idx / snap_w;
            if (x >= doc.width() || y >= doc.height()) continue;

            std::vector<uint8_t> current_data(1);
            current_data[0] = doc.get_material(x, y);
            undo_snapshot.material_delta.changed_pixels[pixel_idx] = std::move(current_data);

            doc.set_material(x, y, old_data[0]);
        }

        // Move to undo stack
        m_current_memory -= snapshot.memory_usage();
        m_redo_stack.pop_back();
        m_undo_stack.push_back(std::move(undo_snapshot));
        m_current_memory += m_undo_stack.back().memory_usage();

        return true;
    }

    bool can_undo() const { return !m_undo_stack.empty(); }
    bool can_redo() const { return !m_redo_stack.empty(); }

    void clear() {
        m_undo_stack.clear();
        m_redo_stack.clear();
        m_current_memory = 0;
        cancel_operation();
    }

    size_t memory_usage() const { return m_current_memory; }
    int undo_count() const { return static_cast<int>(m_undo_stack.size()); }
    int redo_count() const { return static_cast<int>(m_redo_stack.size()); }

private:
    void push_snapshot(DeltaSnapshot snapshot) {
        m_undo_stack.push_back(std::move(snapshot));
        m_current_memory += m_undo_stack.back().memory_usage();

        while (static_cast<int>(m_undo_stack.size()) > m_max_steps ||
               m_current_memory > m_max_memory) {
            if (m_undo_stack.empty()) break;
            m_current_memory -= m_undo_stack.front().memory_usage();
            m_undo_stack.pop_front();
        }

        m_current_memory -= redo_memory();
        m_redo_stack.clear();
    }

    size_t redo_memory() const {
        size_t total = 0;
        for (const auto& s : m_redo_stack) total += s.memory_usage();
        return total;
    }

    std::deque<DeltaSnapshot> m_undo_stack;
    std::deque<DeltaSnapshot> m_redo_stack;
    DeltaCapturer m_capturer;
    int m_max_steps;
    size_t m_max_memory;
    size_t m_current_memory = 0;
};

}
