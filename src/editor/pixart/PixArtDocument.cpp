#include "PixArtDocument.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/core/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>

namespace editor::pixart {

namespace {

static constexpr char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result += BASE64_CHARS[(triple >> 18) & 0x3F];
        result += BASE64_CHARS[(triple >> 12) & 0x3F];
        result += (i > data.size() + 1) ? '=' : BASE64_CHARS[(triple >> 6) & 0x3F];
        result += (i > data.size()) ? '=' : BASE64_CHARS[triple & 0x3F];
    }

    return result;
}

std::vector<uint8_t> base64_decode(const std::string& encoded) {
    static constexpr uint8_t DECODE_TABLE[256] = {
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
        52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
        64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
        64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
        64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
    };

    std::vector<uint8_t> result;
    if (encoded.empty()) return result;

    size_t in_len = encoded.size();
    // Remove padding from length calculation
    while (in_len > 0 && encoded[in_len - 1] == '=') --in_len;

    result.reserve(in_len * 3 / 4);

    uint32_t buffer = 0;
    int bits_collected = 0;

    for (size_t i = 0; i < in_len; ++i) {
        uint8_t c = static_cast<uint8_t>(encoded[i]);
        uint8_t d = DECODE_TABLE[c];
        if (d == 64) continue; // Skip invalid characters

        buffer = (buffer << 6) | d;
        bits_collected += 6;

        if (bits_collected >= 8) {
            bits_collected -= 8;
            result.push_back(static_cast<uint8_t>((buffer >> bits_collected) & 0xFF));
        }
    }

    return result;
}

}

bool PixArtDocument::create(int w, int h) {
    if (w <= 0 || h <= 0) return false;

    // Guard against integer overflow
    auto pixel_count = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (pixel_count > SIZE_MAX / 4) return false;

    m_width = w;
    m_height = h;
    m_origin_x = 0;
    m_origin_y = 0;

    // Initialize final color to transparent
    m_final_color.assign(pixel_count * 4, 0);

    // Initialize materials to 0 (air)
    m_materials.assign(pixel_count, 0);

    // Clear art layers and add the Main layer (index 0, special layer for materials)
    m_art_layers.clear();
    add_art_layer("Main");

    return true;
}

bool PixArtDocument::resize(int new_w, int new_h) {
    if (new_w <= 0 || new_h <= 0) return false;

    auto pixel_count = static_cast<size_t>(new_w) * static_cast<size_t>(new_h);
    if (pixel_count > SIZE_MAX / 4) return false;

    int copy_w = std::min(m_width, new_w);
    int copy_h = std::min(m_height, new_h);

    // Resize final color
    std::vector<uint8_t> new_color(pixel_count * 4, 0);
    for (int y = 0; y < copy_h; ++y) {
        size_t src_off = static_cast<size_t>(y) * m_width * 4;
        size_t dst_off = static_cast<size_t>(y) * new_w * 4;
        std::memcpy(new_color.data() + dst_off,
                    m_final_color.data() + src_off,
                    static_cast<size_t>(copy_w) * 4);
    }
    m_final_color = std::move(new_color);

    // Resize materials
    std::vector<uint8_t> new_materials(pixel_count, 0);
    for (int y = 0; y < copy_h; ++y) {
        size_t src_off = static_cast<size_t>(y) * m_width;
        size_t dst_off = static_cast<size_t>(y) * new_w;
        std::memcpy(new_materials.data() + dst_off,
                    m_materials.data() + src_off,
                    static_cast<size_t>(copy_w));
    }
    m_materials = std::move(new_materials);

    // Resize art layers
    for (auto& layer : m_art_layers) {
        std::vector<uint8_t> new_data(pixel_count * 4, 0);
        for (int y = 0; y < copy_h; ++y) {
            size_t src_off = static_cast<size_t>(y) * m_width * 4;
            size_t dst_off = static_cast<size_t>(y) * new_w * 4;
            std::memcpy(new_data.data() + dst_off,
                        layer.rgba.data() + src_off,
                        static_cast<size_t>(copy_w) * 4);
        }
        layer.rgba = std::move(new_data);
    }

    m_width = new_w;
    m_height = new_h;
    return true;
}

void PixArtDocument::get_final_color(int x, int y, uint8_t* out) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        std::memset(out, 0, 4);
        return;
    }
    size_t offset = (static_cast<size_t>(y) * m_width + x) * 4;
    std::memcpy(out, m_final_color.data() + offset, 4);
}

void PixArtDocument::set_final_color(int x, int y, const uint8_t* rgba) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    size_t offset = (static_cast<size_t>(y) * m_width + x) * 4;
    std::memcpy(m_final_color.data() + offset, rgba, 4);
}

uint8_t PixArtDocument::get_material(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return 0;
    return m_materials[static_cast<size_t>(y) * m_width + x];
}

void PixArtDocument::set_material(int x, int y, uint8_t material_id) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    m_materials[static_cast<size_t>(y) * m_width + x] = material_id;
}

int PixArtDocument::add_art_layer(const std::string& name) {
    ArtLayer layer;
    layer.name = name;
    layer.rgba.assign(static_cast<size_t>(m_width) * m_height * 4, 0);
    layer.opacity = 1.0f;
    layer.visible = true;
    m_art_layers.push_back(std::move(layer));
    return static_cast<int>(m_art_layers.size()) - 1;
}

int PixArtDocument::remove_art_layer(int idx, int current_active) {
    if (idx < 0 || idx >= static_cast<int>(m_art_layers.size())) {
        return current_active;
    }
    // Cannot remove the Main layer (index 0)
    if (is_main_layer(idx)) {
        return current_active;
    }
    // Must keep at least one layer (Main layer)
    if (m_art_layers.size() <= 1) {
        return current_active;
    }

    m_art_layers.erase(m_art_layers.begin() + idx);

    if (current_active < idx) {
        return current_active;
    } else if (current_active == idx) {
        int new_count = static_cast<int>(m_art_layers.size());
        return (idx < new_count) ? idx : new_count - 1;
    } else {
        return current_active - 1;
    }
}

void PixArtDocument::swap_art_layers(int a, int b) {
    if (a < 0 || a >= static_cast<int>(m_art_layers.size())) return;
    if (b < 0 || b >= static_cast<int>(m_art_layers.size())) return;
    if (a == b) return;
    // Cannot swap with Main layer (index 0)
    if (is_main_layer(a) || is_main_layer(b)) return;
    std::swap(m_art_layers[a], m_art_layers[b]);
}

void PixArtDocument::merge_art_layers(int dst, int src) {
    if (dst < 0 || dst >= static_cast<int>(m_art_layers.size())) return;
    if (src < 0 || src >= static_cast<int>(m_art_layers.size())) return;
    if (dst == src) return;
    if (m_art_layers.size() <= 1) return;  // Keep at least one layer
    // Cannot merge Main layer as source (it would be removed)
    if (is_main_layer(src)) return;

    auto& dst_layer = m_art_layers[dst];
    const auto& src_layer = m_art_layers[src];

    size_t pixel_count = static_cast<size_t>(m_width) * m_height;

    // Composite src onto dst using alpha blending
    float src_opacity = src_layer.opacity;
    for (size_t px = 0; px < pixel_count; ++px) {
        size_t offset = px * 4;

        // Source pixel
        uint8_t sr = src_layer.rgba[offset + 0];
        uint8_t sg = src_layer.rgba[offset + 1];
        uint8_t sb = src_layer.rgba[offset + 2];
        uint8_t sa = src_layer.rgba[offset + 3];

        float src_alpha = (sa / 255.0f) * src_opacity;
        if (src_alpha <= 0.0f) continue;

        // Destination pixel
        uint8_t dr = dst_layer.rgba[offset + 0];
        uint8_t dg = dst_layer.rgba[offset + 1];
        uint8_t db = dst_layer.rgba[offset + 2];
        uint8_t da = dst_layer.rgba[offset + 3];

        float dst_alpha = da / 255.0f;

        // Alpha compositing (source over destination)
        float out_alpha = src_alpha + dst_alpha * (1.0f - src_alpha);
        if (out_alpha > 0.0f) {
            float inv_out_alpha = 1.0f / out_alpha;
            dst_layer.rgba[offset + 0] = static_cast<uint8_t>(
                (sr * src_alpha + dr * dst_alpha * (1.0f - src_alpha)) * inv_out_alpha);
            dst_layer.rgba[offset + 1] = static_cast<uint8_t>(
                (sg * src_alpha + dg * dst_alpha * (1.0f - src_alpha)) * inv_out_alpha);
            dst_layer.rgba[offset + 2] = static_cast<uint8_t>(
                (sb * src_alpha + db * dst_alpha * (1.0f - src_alpha)) * inv_out_alpha);
            dst_layer.rgba[offset + 3] = static_cast<uint8_t>(out_alpha * 255.0f);
        }
    }

    // Remove the source layer
    m_art_layers.erase(m_art_layers.begin() + src);
}

void PixArtDocument::set_art_layer_pixel(int layer_idx, int x, int y, const uint8_t* rgba) {
    if (layer_idx < 0 || layer_idx >= static_cast<int>(m_art_layers.size())) return;
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    size_t offset = (static_cast<size_t>(y) * m_width + x) * 4;
    std::memcpy(m_art_layers[layer_idx].rgba.data() + offset, rgba, 4);
}

void PixArtDocument::get_art_layer_pixel(int layer_idx, int x, int y, uint8_t* out) const {
    if (layer_idx < 0 || layer_idx >= static_cast<int>(m_art_layers.size())) {
        std::memset(out, 0, 4);
        return;
    }
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        std::memset(out, 0, 4);
        return;
    }
    size_t offset = (static_cast<size_t>(y) * m_width + x) * 4;
    std::memcpy(out, m_art_layers[layer_idx].rgba.data() + offset, 4);
}

void PixArtDocument::composite_art_layers(std::vector<uint8_t>& out) const {
    size_t pixel_count = static_cast<size_t>(m_width) * m_height;
    out.assign(pixel_count * 4, 0);

    // Composite layers from bottom to top using alpha blending
    for (const auto& layer : m_art_layers) {
        if (!layer.visible) continue;

        float layer_opacity = layer.opacity;
        for (size_t px = 0; px < pixel_count; ++px) {
            size_t offset = px * 4;

            // Source pixel from this layer
            uint8_t sr = layer.rgba[offset + 0];
            uint8_t sg = layer.rgba[offset + 1];
            uint8_t sb = layer.rgba[offset + 2];
            uint8_t sa = layer.rgba[offset + 3];

            // Apply layer opacity
            float src_alpha = (sa / 255.0f) * layer_opacity;
            if (src_alpha <= 0.0f) continue;

            // Destination pixel (current composite)
            uint8_t dr = out[offset + 0];
            uint8_t dg = out[offset + 1];
            uint8_t db = out[offset + 2];
            uint8_t da = out[offset + 3];

            float dst_alpha = da / 255.0f;

            // Alpha compositing (source over destination)
            float out_alpha = src_alpha + dst_alpha * (1.0f - src_alpha);
            if (out_alpha > 0.0f) {
                float inv_out_alpha = 1.0f / out_alpha;
                out[offset + 0] = static_cast<uint8_t>(
                    (sr * src_alpha + dr * dst_alpha * (1.0f - src_alpha)) * inv_out_alpha);
                out[offset + 1] = static_cast<uint8_t>(
                    (sg * src_alpha + dg * dst_alpha * (1.0f - src_alpha)) * inv_out_alpha);
                out[offset + 2] = static_cast<uint8_t>(
                    (sb * src_alpha + db * dst_alpha * (1.0f - src_alpha)) * inv_out_alpha);
                out[offset + 3] = static_cast<uint8_t>(out_alpha * 255.0f);
            }
        }
    }
}

void PixArtDocument::flatten_art_layers() {
    composite_art_layers(m_final_color);

    // Clear all art layers and add the Main layer
    m_art_layers.clear();
    add_art_layer("Main");
}

// === File I/O ===

bool PixArtDocument::save(const std::string& path) const {
    if (!valid()) return false;

    // Total channels: 4 (color RGBA) + 1 (material)
    constexpr int total_channels = 5;

    // Build channel descriptors
    std::vector<engine::asset::PxgChannelDesc> descs(total_channels);
    std::snprintf(descs[0].name, sizeof(descs[0].name), "color_r");
    std::snprintf(descs[1].name, sizeof(descs[1].name), "color_g");
    std::snprintf(descs[2].name, sizeof(descs[2].name), "color_b");
    std::snprintf(descs[3].name, sizeof(descs[3].name), "color_a");
    std::snprintf(descs[4].name, sizeof(descs[4].name), "material");
    for (auto& desc : descs) desc.bits = 8;

    // Build interleaved pixel data
    size_t pixel_count = static_cast<size_t>(m_width) * m_height;
    std::vector<uint8_t> interleaved(pixel_count * total_channels, 0);

    for (size_t px = 0; px < pixel_count; ++px) {
        // Color RGBA
        interleaved[px * total_channels + 0] = m_final_color[px * 4 + 0];
        interleaved[px * total_channels + 1] = m_final_color[px * 4 + 1];
        interleaved[px * total_channels + 2] = m_final_color[px * 4 + 2];
        interleaved[px * total_channels + 3] = m_final_color[px * 4 + 3];
        // Material
        interleaved[px * total_channels + 4] = m_materials[px];
    }

    // Build metadata JSON
    nlohmann::json meta;
    // Flip Y coordinate: editor uses Y-down (top=0), engine uses Y-up (bottom=0)
    meta["origin"] = {{"x", m_origin_x}, {"y", m_height - m_origin_y}};
    meta["format_version"] = 2;  // New format with art layers

    // Save art layers with RGBA data
    meta["art_layers"] = nlohmann::json::array();
    for (const auto& layer : m_art_layers) {
        nlohmann::json lj;
        lj["name"] = layer.name;
        lj["opacity"] = layer.opacity;
        lj["visible"] = layer.visible;
        // Encode layer RGBA data as base64
        lj["data"] = base64_encode(layer.rgba);
        meta["art_layers"].push_back(lj);
    }

    // Save material names
    meta["material_names"] = m_material_names;

    // For backward compatibility, also save in old format
    meta["layers"] = nlohmann::json::array();
    {
        nlohmann::json color_layer;
        color_layer["name"] = "color";
        color_layer["type"] = "color";
        color_layer["opacity"] = 1.0f;
        color_layer["visible"] = true;
        color_layer["engine_required"] = true;
        meta["layers"].push_back(color_layer);

        nlohmann::json material_layer;
        material_layer["name"] = "material";
        material_layer["type"] = "enum";
        material_layer["opacity"] = 1.0f;
        material_layer["visible"] = true;
        material_layer["engine_required"] = true;
        material_layer["values"] = m_material_names;
        meta["layers"].push_back(material_layer);
    }

    std::string metadata_str = meta.dump();

    bool success = engine::asset::pxg_save(
        path,
        static_cast<uint32_t>(m_width),
        static_cast<uint32_t>(m_height),
        descs.data(),
        static_cast<uint32_t>(total_channels),
        interleaved.data(),
        metadata_str);

    if (!success) {
        engine::Logger::instance().error("PixArtDocument", "Failed to save file: %s", path.c_str());
    }

    return success;
}

bool PixArtDocument::load(const std::string& path) {
    auto result = engine::asset::pxg_load(path);
    if (!result) {
        engine::Logger::instance().error("PixArtDocument", "Failed to load file: %s", path.c_str());
        return false;
    }

    const auto& pxg = *result;
    int w = static_cast<int>(pxg.header.width);
    int h = static_cast<int>(pxg.header.height);
    int total_channels = static_cast<int>(pxg.header.channels_per_pixel);

    m_width = w;
    m_height = h;
    m_origin_x = 0;
    m_origin_y = 0;
    m_art_layers.clear();
    m_material_names.clear();

    size_t pixel_count = static_cast<size_t>(w) * h;
    m_final_color.assign(pixel_count * 4, 0);
    m_materials.assign(pixel_count, 0);

    // Try to parse metadata
    bool has_metadata = false;
    int format_version = 1;

    if (!pxg.metadata.empty()) {
        try {
            std::string meta_str(pxg.metadata.begin(), pxg.metadata.end());
            auto meta = nlohmann::json::parse(meta_str);

            // Parse origin (flip Y: file uses Y-up, editor uses Y-down)
            if (meta.contains("origin") && meta["origin"].is_object()) {
                m_origin_x = meta["origin"].value("x", 0);
                int file_origin_y = meta["origin"].value("y", 0);
                m_origin_y = h - file_origin_y;
            }

            format_version = meta.value("format_version", 1);

            // Parse material names from old or new format
            if (meta.contains("material_names")) {
                for (const auto& name : meta["material_names"]) {
                    m_material_names.push_back(name.get<std::string>());
                }
            } else if (meta.contains("layers")) {
                // Try to find material layer in old format
                for (const auto& lj : meta["layers"]) {
                    if (lj.value("name", "") == "material" && lj.contains("values")) {
                        for (const auto& v : lj["values"]) {
                            m_material_names.push_back(v.get<std::string>());
                        }
                        break;
                    }
                }
            }

            // Parse art layers from new format
            if (meta.contains("art_layers")) {
                int layer_idx = 0;
                for (const auto& lj : meta["art_layers"]) {
                    ArtLayer layer;
                    // First layer is always "Main" regardless of what's stored
                    layer.name = (layer_idx == 0) ? "Main" : lj.value("name", "Layer");
                    layer.opacity = lj.value("opacity", 1.0f);
                    layer.visible = lj.value("visible", true);
                    layer.rgba.assign(pixel_count * 4, 0);

                    // Decode base64 layer data if present
                    if (lj.contains("data")) {
                        std::string data_b64 = lj.value("data", "");
                        if (!data_b64.empty()) {
                            auto decoded = base64_decode(data_b64);
                            if (decoded.size() == pixel_count * 4) {
                                layer.rgba = std::move(decoded);
                            }
                        }
                    }

                    m_art_layers.push_back(std::move(layer));
                    ++layer_idx;
                }
            }

            has_metadata = true;
        } catch (const std::exception&) {
            // Metadata parse failed
        }
    }

    // De-interleave pixel data
    // Expect: color_r, color_g, color_b, color_a, material (or just first 5 channels)
    bool has_color = total_channels >= 4;
    bool has_material = total_channels >= 5;

    for (size_t px = 0; px < pixel_count; ++px) {
        if (has_color) {
            m_final_color[px * 4 + 0] = pxg.pixels[px * total_channels + 0];
            m_final_color[px * 4 + 1] = pxg.pixels[px * total_channels + 1];
            m_final_color[px * 4 + 2] = pxg.pixels[px * total_channels + 2];
            m_final_color[px * 4 + 3] = pxg.pixels[px * total_channels + 3];
        }
        if (has_material) {
            m_materials[px] = pxg.pixels[px * total_channels + 4];
        }
    }

    // If no art layers were loaded, create the Main layer
    if (m_art_layers.empty()) {
        add_art_layer("Main");
    }

    // Ensure first layer is always named "Main" (the material layer)
    if (!m_art_layers.empty()) {
        m_art_layers[0].name = "Main";
    }

    // Copy loaded final_color to the first art layer so colors are editable
    // This ensures backward compatibility with files that don't have art layer data
    if (!m_art_layers.empty() && has_color) {
        m_art_layers[0].rgba = m_final_color;
    }

    // Set default material names if none were loaded
    if (m_material_names.empty()) {
        m_material_names = {"air", "rock", "dirt", "sand", "water",
                           "lava", "ice", "steam", "fire", "explosive"};
    }

    return true;
}

}
