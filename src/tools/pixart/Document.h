#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pixart {

enum class LayerType {
    Color,  // RGBA, 4 channels per pixel
    UInt8,  // single uint8 value per pixel
    Enum    // single uint8 value per pixel + named enum values
};

struct Layer {
    std::string name;
    LayerType type = LayerType::UInt8;
    int channels = 1;                       // 4 for Color, 1 for data layers
    std::vector<uint8_t> data;              // size = width * height * channels
    std::vector<std::string> enum_names;    // only used for Enum type
    bool visible = true;
    float opacity = 1.0f;                   // 0.0 = fully transparent, 1.0 = fully opaque
    bool engine_required = false;           // if true, cannot be deleted by the user
};

/// Pixel grid document with a layer-based data model.
/// Layer 0 is always the mandatory Color layer (RGBA).
class Document {
public:
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool valid() const { return m_width > 0 && m_height > 0; }

    /// Origin/pivot point (pixel coordinates within the grid).
    int origin_x() const { return m_origin_x; }
    int origin_y() const { return m_origin_y; }
    void set_origin(int x, int y) { m_origin_x = x; m_origin_y = y; }

    /// Create a new blank document with only the Color layer.
    /// @return false if dimensions are invalid or would overflow.
    bool create(int w, int h);

    /// Resize the grid, preserving content at the top-left corner.
    /// @return false if dimensions are invalid or would overflow.
    bool resize(int new_w, int new_h);

    // --- Layer management ---

    int layer_count() const { return static_cast<int>(m_layers.size()); }
    Layer& layer(int idx) { return m_layers[idx]; }
    const Layer& layer(int idx) const { return m_layers[idx]; }

    /// Add a new data layer. Returns the index of the new layer.
    int add_layer(const std::string& name, LayerType type,
                  const std::vector<std::string>& enum_names = {});

    /// Remove a layer by index. Cannot remove layer 0 (Color).
    /// Returns the suggested new active layer index given the previous active layer.
    /// If removal is invalid, returns current_active unchanged.
    int remove_layer(int idx, int current_active);

    /// Swap two layers by index. Neither can be out of range.
    void swap_layers(int a, int b);

    // --- Pixel access ---

    /// Set pixel values for a given layer at (x, y).
    /// `values` must point to `layer.channels` bytes.
    void set_pixel(int layer_idx, int x, int y,
                   const uint8_t* values);

    /// Get pixel values for a given layer at (x, y).
    /// `out` must point to `layer.channels` bytes.
    void get_pixel(int layer_idx, int x, int y,
                   uint8_t* out) const;

    // --- File I/O ---

    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    int m_width = 0;
    int m_height = 0;
    int m_origin_x = 0;
    int m_origin_y = 0;
    std::vector<Layer> m_layers;
};

} // namespace pixart
