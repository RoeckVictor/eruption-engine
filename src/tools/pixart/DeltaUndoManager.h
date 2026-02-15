#pragma once

#include "Document.h"
#include <deque>
#include <optional>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace pixart {

/// A delta-based undo snapshot that only stores changed pixels.
/// Much more memory-efficient than storing full layer copies.
struct DeltaSnapshot {
    /// Changed pixel data for each layer.
    /// Key: layer index, Value: map of (position -> old pixel data)
    struct LayerDelta {
        // Sparse map: pixel_index -> old pixel data (before change)
        std::unordered_map<int, std::vector<uint8_t>> changed_pixels;
    };
    std::unordered_map<int, LayerDelta> layer_deltas;

    /// Document dimensions at the time of capture.
    /// Used to correctly decode linear pixel indices during undo/redo,
    /// even if the document has been resized since.
    int doc_width = 0;
    int doc_height = 0;

    /// Optional structural changes (origin, layer swap) recorded alongside pixel deltas.
    struct OriginChange { int old_x, old_y; };
    std::optional<OriginChange> origin_change;

    struct LayerSwap { int index_a, index_b; };
    std::optional<LayerSwap> layer_swap;

    /// Check if this snapshot has any changes.
    bool empty() const {
        if (origin_change.has_value() || layer_swap.has_value()) return false;
        for (const auto& [layer_idx, delta] : layer_deltas) {
            if (!delta.changed_pixels.empty()) return false;
        }
        return true;
    }

    /// Get approximate memory usage in bytes.
    size_t memory_usage() const {
        size_t total = sizeof(*this);
        for (const auto& [layer_idx, delta] : layer_deltas) {
            total += sizeof(layer_idx) + sizeof(LayerDelta);
            for (const auto& [px_idx, data] : delta.changed_pixels) {
                total += sizeof(px_idx) + sizeof(std::vector<uint8_t>) + data.size();
            }
        }
        return total;
    }
};

/// Builder for creating delta snapshots.
/// Call begin_capture() before making changes, then capture_pixel() for each
/// pixel that will be modified, then end_capture() when done.
/// LIFETIME: The Document passed to begin_capture() must remain valid and
/// unmodified (no create/resize) until end_capture() is called.
class DeltaCapturer {
public:
    /// Start capturing changes for a document.
    void begin_capture(const Document& doc) {
        m_doc = &doc;
        m_expected_width = doc.width();
        m_expected_height = doc.height();
        m_snapshot = DeltaSnapshot{};
        m_snapshot.doc_width = doc.width();
        m_snapshot.doc_height = doc.height();
        m_capturing = true;
    }

    /// Record a pixel that is about to be changed.
    /// Must be called BEFORE the pixel is modified.
    void capture_pixel(int layer_idx, int x, int y) {
        if (!m_capturing || !m_doc || !m_doc->valid()) return;
        // Guard: if the document was replaced or resized since begin_capture,
        // the pointer may be stale — abort safely.
        if (m_doc->width() != m_expected_width || m_doc->height() != m_expected_height) {
            m_capturing = false;
            m_doc = nullptr;
            return;
        }
        if (layer_idx < 0 || layer_idx >= m_doc->layer_count()) return;
        if (x < 0 || x >= m_doc->width() || y < 0 || y >= m_doc->height()) return;

        const auto& layer = m_doc->layer(layer_idx);
        int pixel_idx = y * m_doc->width() + x;

        // Check if already captured
        auto& layer_delta = m_snapshot.layer_deltas[layer_idx];
        if (layer_delta.changed_pixels.count(pixel_idx)) return;

        // Store the current (pre-change) pixel data
        std::vector<uint8_t> old_data(layer.channels);
        m_doc->get_pixel(layer_idx, x, y, old_data.data());
        layer_delta.changed_pixels[pixel_idx] = std::move(old_data);
    }

    /// Capture a rectangular region that is about to be changed.
    void capture_region(int layer_idx, int x, int y, int w, int h) {
        for (int py = y; py < y + h; ++py) {
            for (int px = x; px < x + w; ++px) {
                capture_pixel(layer_idx, px, py);
            }
        }
    }

    /// Capture a brush stroke (circular region).
    void capture_brush(int layer_idx, int cx, int cy, int radius) {
        int r = radius - 1;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    capture_pixel(layer_idx, cx + dx, cy + dy);
                }
            }
        }
    }

    /// Finish capturing and return the snapshot.
    DeltaSnapshot end_capture() {
        m_capturing = false;
        m_doc = nullptr;
        return std::move(m_snapshot);
    }

    /// Check if currently capturing.
    bool is_capturing() const { return m_capturing; }

private:
    const Document* m_doc = nullptr;
    int m_expected_width = 0;
    int m_expected_height = 0;
    DeltaSnapshot m_snapshot;
    bool m_capturing = false;
};

/// Delta-based undo manager.
/// Much more memory-efficient than full-copy undo for typical editing operations.
class DeltaUndoManager {
public:
    static constexpr int DEFAULT_MAX_UNDO = 128;  // Can have more steps since deltas are smaller
    static constexpr size_t DEFAULT_MAX_MEMORY = 256 * 1024 * 1024;  // 256 MB limit

    explicit DeltaUndoManager(int max_steps = DEFAULT_MAX_UNDO,
                               size_t max_memory = DEFAULT_MAX_MEMORY)
        : m_max_steps(max_steps), m_max_memory(max_memory) {}

    /// Begin capturing changes for a new undo step.
    /// Call this BEFORE making any changes to the document.
    void begin_operation(const Document& doc) {
        m_capturer.begin_capture(doc);
    }

    /// Record that a pixel is about to be changed.
    void record_pixel(int layer_idx, int x, int y) {
        m_capturer.capture_pixel(layer_idx, x, y);
    }

    /// Record that a brush stroke is about to be applied.
    void record_brush(int layer_idx, int cx, int cy, int radius) {
        m_capturer.capture_brush(layer_idx, cx, cy, radius);
    }

    /// Finish the current operation and push it to the undo stack.
    /// Call this AFTER making changes to the document.
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

    /// Push a standalone structural undo step (origin change or layer swap).
    /// These don't require begin/end_operation since they don't involve pixel captures.
    void push_origin_change(const Document& doc, int old_x, int old_y) {
        DeltaSnapshot snapshot;
        snapshot.doc_width = doc.width();
        snapshot.doc_height = doc.height();
        snapshot.origin_change = DeltaSnapshot::OriginChange{old_x, old_y};
        push_snapshot(std::move(snapshot));
    }

    void push_layer_swap(const Document& doc, int index_a, int index_b) {
        DeltaSnapshot snapshot;
        snapshot.doc_width = doc.width();
        snapshot.doc_height = doc.height();
        snapshot.layer_swap = DeltaSnapshot::LayerSwap{index_a, index_b};
        push_snapshot(std::move(snapshot));
    }

    /// Cancel the current operation without pushing to undo stack.
    void cancel_operation() {
        if (m_capturer.is_capturing()) {
            m_capturer.end_capture();  // Discard
        }
    }

    /// Undo the last operation.
    /// @return True if undo was performed. Sets active_layer if a layer swap was undone.
    bool undo(Document& doc, int* active_layer = nullptr) {
        if (m_undo_stack.empty() || !doc.valid() || doc.width() == 0) return false;

        auto& snapshot = m_undo_stack.back();

        // Use the snapshot's stored dimensions to decode pixel indices
        int snap_w = snapshot.doc_width > 0 ? snapshot.doc_width : doc.width();

        // Capture current state for redo
        DeltaSnapshot redo_snapshot;
        redo_snapshot.doc_width = doc.width();
        redo_snapshot.doc_height = doc.height();

        // Handle structural changes
        if (snapshot.origin_change) {
            redo_snapshot.origin_change = DeltaSnapshot::OriginChange{doc.origin_x(), doc.origin_y()};
            doc.set_origin(snapshot.origin_change->old_x, snapshot.origin_change->old_y);
        }
        if (snapshot.layer_swap) {
            // Reverse the swap
            redo_snapshot.layer_swap = snapshot.layer_swap;
            int a = snapshot.layer_swap->index_a;
            int b = snapshot.layer_swap->index_b;
            doc.swap_layers(a, b);
            if (active_layer) {
                if (*active_layer == a) *active_layer = b;
                else if (*active_layer == b) *active_layer = a;
            }
        }

        // Handle pixel deltas
        for (const auto& [layer_idx, layer_delta] : snapshot.layer_deltas) {
            if (layer_idx >= doc.layer_count()) continue;
            const auto& layer = doc.layer(layer_idx);

            auto& redo_layer = redo_snapshot.layer_deltas[layer_idx];
            for (const auto& [pixel_idx, old_data] : layer_delta.changed_pixels) {
                int x = pixel_idx % snap_w;
                int y = pixel_idx / snap_w;

                if (x >= doc.width() || y >= doc.height()) continue;

                std::vector<uint8_t> current_data(layer.channels);
                doc.get_pixel(layer_idx, x, y, current_data.data());
                redo_layer.changed_pixels[pixel_idx] = std::move(current_data);

                doc.set_pixel(layer_idx, x, y, old_data.data());
            }
        }

        // Move to redo stack
        m_current_memory -= snapshot.memory_usage();
        m_undo_stack.pop_back();
        m_redo_stack.push_back(std::move(redo_snapshot));
        m_current_memory += m_redo_stack.back().memory_usage();

        return true;
    }

    /// Redo the last undone operation.
    /// @return True if redo was performed. Sets active_layer if a layer swap was redone.
    bool redo(Document& doc, int* active_layer = nullptr) {
        if (m_redo_stack.empty() || !doc.valid() || doc.width() == 0) return false;

        auto& snapshot = m_redo_stack.back();

        // Use the snapshot's stored dimensions to decode pixel indices
        int snap_w = snapshot.doc_width > 0 ? snapshot.doc_width : doc.width();

        // Capture current state for undo
        DeltaSnapshot undo_snapshot;
        undo_snapshot.doc_width = doc.width();
        undo_snapshot.doc_height = doc.height();

        // Handle structural changes
        if (snapshot.origin_change) {
            undo_snapshot.origin_change = DeltaSnapshot::OriginChange{doc.origin_x(), doc.origin_y()};
            doc.set_origin(snapshot.origin_change->old_x, snapshot.origin_change->old_y);
        }
        if (snapshot.layer_swap) {
            undo_snapshot.layer_swap = snapshot.layer_swap;
            int a = snapshot.layer_swap->index_a;
            int b = snapshot.layer_swap->index_b;
            doc.swap_layers(a, b);
            if (active_layer) {
                if (*active_layer == a) *active_layer = b;
                else if (*active_layer == b) *active_layer = a;
            }
        }

        // Handle pixel deltas
        for (const auto& [layer_idx, layer_delta] : snapshot.layer_deltas) {
            if (layer_idx >= doc.layer_count()) continue;
            const auto& layer = doc.layer(layer_idx);

            auto& undo_layer = undo_snapshot.layer_deltas[layer_idx];
            for (const auto& [pixel_idx, old_data] : layer_delta.changed_pixels) {
                int x = pixel_idx % snap_w;
                int y = pixel_idx / snap_w;

                if (x >= doc.width() || y >= doc.height()) continue;

                std::vector<uint8_t> current_data(layer.channels);
                doc.get_pixel(layer_idx, x, y, current_data.data());
                undo_layer.changed_pixels[pixel_idx] = std::move(current_data);

                doc.set_pixel(layer_idx, x, y, old_data.data());
            }
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

    /// Get current memory usage in bytes.
    size_t memory_usage() const { return m_current_memory; }

    /// Get number of undo steps available.
    int undo_count() const { return static_cast<int>(m_undo_stack.size()); }

    /// Get number of redo steps available.
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

} // namespace pixart
