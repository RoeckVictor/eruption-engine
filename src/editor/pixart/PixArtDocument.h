#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace editor::pixart {

inline constexpr const char* MAIN_LAYER_NAME = "Main";

struct ArtLayer {
    std::string name;
    std::vector<uint8_t> rgba;
    float opacity = 1.0f;
    bool visible = true;
};

/// Document model for PixArt panel integration
class PixArtDocument {
public:
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool valid() const { return m_width > 0 && m_height > 0; }

    int origin_x() const { return m_origin_x; }
    int origin_y() const { return m_origin_y; }
    void set_origin(int x, int y) { m_origin_x = x; m_origin_y = y; }

    bool create(int w, int h);
    bool resize(int new_w, int new_h);
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    void get_final_color(int x, int y, uint8_t* out) const;
    void set_final_color(int x, int y, const uint8_t* rgba);

    uint8_t get_material(int x, int y) const;
    void set_material(int x, int y, uint8_t material_id);

    const std::vector<uint8_t>& final_color_data() const { return m_final_color; }
    const std::vector<uint8_t>& material_data() const { return m_materials; }

    int art_layer_count() const { return static_cast<int>(m_art_layers.size()); }
    ArtLayer& art_layer(int idx) { return m_art_layers[idx]; }
    const ArtLayer& art_layer(int idx) const { return m_art_layers[idx]; }

    static bool is_main_layer(int idx) { return idx == 0; }

    int add_art_layer(const std::string& name);
    int remove_art_layer(int idx, int current_active);
    void swap_art_layers(int a, int b);
    void merge_art_layers(int dst, int src);
    void set_art_layer_pixel(int layer_idx, int x, int y, const uint8_t* rgba);
    void get_art_layer_pixel(int layer_idx, int x, int y, uint8_t* out) const;
    void flatten_art_layers();
    void composite_art_layers(std::vector<uint8_t>& out) const;

    const std::vector<std::string>& material_names() const { return m_material_names; }
    void set_material_names(const std::vector<std::string>& names) { m_material_names = names; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_origin_x = 0;
    int m_origin_y = 0;

    std::vector<uint8_t> m_final_color;
    std::vector<uint8_t> m_materials;

    std::vector<ArtLayer> m_art_layers;

    std::vector<std::string> m_material_names;
};

}
