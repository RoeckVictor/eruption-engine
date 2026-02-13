#include "Document.h"
#include "engine/asset/PixelGridFile.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace pixart {

void Document::create(int w, int h) {
    m_width = w;
    m_height = h;
    m_origin_x = 0;
    m_origin_y = 0;
    m_layers.clear();

    // Layer 0: mandatory Color layer (RGBA, default = fully transparent)
    Layer color;
    color.name = "color";
    color.type = LayerType::Color;
    color.channels = 4;
    color.data.assign(static_cast<size_t>(w) * h * 4, 0);
    color.engine_required = true;
    m_layers.push_back(std::move(color));

    // Layer 1: mandatory Material layer (Enum, default = 0 / air)
    Layer material;
    material.name = "material";
    material.type = LayerType::Enum;
    material.channels = 1;
    material.data.assign(static_cast<size_t>(w) * h, 0);
    material.enum_names = {"air", "rock", "dirt", "sand", "water",
                           "lava", "ice", "steam", "fire", "explosive"};
    material.engine_required = true;
    m_layers.push_back(std::move(material));
}

void Document::resize(int new_w, int new_h) {
    if (new_w <= 0 || new_h <= 0) return;

    int copy_w = std::min(m_width, new_w);
    int copy_h = std::min(m_height, new_h);

    for (auto& layer : m_layers) {
        // New area defaults to 0 (transparent for Color, zero for data layers)
        std::vector<uint8_t> new_data(static_cast<size_t>(new_w) * new_h * layer.channels, 0);
        // Copy existing content
        for (int y = 0; y < copy_h; ++y) {
            size_t src_off = static_cast<size_t>(y) * m_width * layer.channels;
            size_t dst_off = static_cast<size_t>(y) * new_w * layer.channels;
            std::memcpy(new_data.data() + dst_off,
                        layer.data.data() + src_off,
                        static_cast<size_t>(copy_w) * layer.channels);
        }
        layer.data = std::move(new_data);
    }

    m_width = new_w;
    m_height = new_h;
}

int Document::add_layer(const std::string& name, LayerType type,
                         const std::vector<std::string>& enum_names) {
    Layer layer;
    layer.name = name;
    layer.type = type;
    layer.channels = (type == LayerType::Color) ? 4 : 1;
    layer.data.assign(static_cast<size_t>(m_width) * m_height * layer.channels, 0);
    if (type == LayerType::Enum) {
        layer.enum_names = enum_names;
    }
    m_layers.push_back(std::move(layer));
    return static_cast<int>(m_layers.size()) - 1;
}

int Document::remove_layer(int idx, int current_active) {
    if (idx < 0 || idx >= static_cast<int>(m_layers.size())) {
        return current_active; // Invalid removal, no change
    }
    if (m_layers[idx].engine_required) {
        return current_active; // Cannot remove engine-required layers
    }

    m_layers.erase(m_layers.begin() + idx);

    // Calculate new active layer:
    // - If active was below removed layer, no change needed
    // - If active was the removed layer, select the one below (or above if none below)
    // - If active was above removed layer, decrement index
    if (current_active < idx) {
        return current_active;
    } else if (current_active == idx) {
        // Removed the active layer: select the one that's now at this index,
        // or the last one if we removed the top layer
        int new_count = static_cast<int>(m_layers.size());
        return (idx < new_count) ? idx : new_count - 1;
    } else {
        // Active was above removed layer, decrement
        return current_active - 1;
    }
}

void Document::swap_layers(int a, int b) {
    if (a < 0 || a >= static_cast<int>(m_layers.size())) return;
    if (b < 0 || b >= static_cast<int>(m_layers.size())) return;
    if (a == b) return;
    std::swap(m_layers[a], m_layers[b]);
}

void Document::set_pixel(int layer_idx, int x, int y, const uint8_t* values) {
    if (layer_idx < 0 || layer_idx >= static_cast<int>(m_layers.size())) return;
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    auto& layer = m_layers[layer_idx];
    size_t offset = (static_cast<size_t>(y) * m_width + x) * layer.channels;
    std::memcpy(layer.data.data() + offset, values, layer.channels);
}

void Document::get_pixel(int layer_idx, int x, int y, uint8_t* out) const {
    if (layer_idx < 0 || layer_idx >= static_cast<int>(m_layers.size())) {
        // Invalid layer - can't determine channel count, just return
        return;
    }
    const auto& layer = m_layers[layer_idx];
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        std::memset(out, 0, layer.channels);
        return;
    }
    size_t offset = (static_cast<size_t>(y) * m_width + x) * layer.channels;
    std::memcpy(out, layer.data.data() + offset, layer.channels);
}

// --- .pxg save/load ---

bool Document::save(const std::string& path) const {
    if (!valid()) return false;

    // Count total channels across all layers
    int total_channels = 0;
    for (const auto& layer : m_layers)
        total_channels += layer.channels;

    // Build channel descriptors
    std::vector<engine::asset::PxgChannelDesc> descs(total_channels);
    int ch_idx = 0;
    for (const auto& layer : m_layers) {
        if (layer.type == LayerType::Color) {
            const char* suffixes[] = {"color_r", "color_g", "color_b", "color_a"};
            for (int c = 0; c < 4; ++c) {
                std::snprintf(descs[ch_idx].name, sizeof(descs[ch_idx].name), "%s", suffixes[c]);
                descs[ch_idx].bits = 8;
                ch_idx++;
            }
        } else {
            std::snprintf(descs[ch_idx].name, sizeof(descs[ch_idx].name), "%s", layer.name.c_str());
            descs[ch_idx].bits = 8;
            ch_idx++;
        }
    }

    // Build interleaved pixel data
    // .pxg format: for each pixel, write all channels in order
    // Data layer values are zeroed where the color layer is fully transparent,
    // since those pixels don't physically exist.
    size_t pixel_count = static_cast<size_t>(m_width) * m_height;
    std::vector<uint8_t> interleaved(pixel_count * total_channels, 0);

    // Find the color layer to determine pixel existence
    const Layer* color_layer = nullptr;
    for (const auto& layer : m_layers) {
        if (layer.type == LayerType::Color) {
            color_layer = &layer;
            break;
        }
    }

    for (size_t px = 0; px < pixel_count; ++px) {
        // Check if this pixel physically exists (color alpha > 0)
        bool pixel_exists = true;
        if (color_layer) {
            uint8_t alpha = color_layer->data[px * 4 + 3];
            pixel_exists = (alpha > 0);
        }

        int out_ch = 0;
        for (const auto& layer : m_layers) {
            if (layer.type == LayerType::Color || pixel_exists) {
                for (int c = 0; c < layer.channels; ++c) {
                    interleaved[px * total_channels + out_ch] =
                        layer.data[px * layer.channels + c];
                    out_ch++;
                }
            } else {
                // Pixel doesn't exist: write zeros for data layers
                out_ch += layer.channels;
            }
        }
    }

    // Build metadata JSON
    nlohmann::json meta;
    meta["origin"] = {{"x", m_origin_x}, {"y", m_origin_y}};
    meta["layers"] = nlohmann::json::array();
    for (const auto& layer : m_layers) {
        nlohmann::json lj;
        lj["name"] = layer.name;
        lj["opacity"] = layer.opacity;
        lj["visible"] = layer.visible;
        switch (layer.type) {
            case LayerType::Color: lj["type"] = "color"; break;
            case LayerType::UInt8: lj["type"] = "uint8"; break;
            case LayerType::Enum:
                lj["type"] = "enum";
                lj["values"] = layer.enum_names;
                break;
        }
        meta["layers"].push_back(lj);
    }
    std::string metadata_str = meta.dump();

    return engine::asset::pxg_save(
        path,
        static_cast<uint32_t>(m_width),
        static_cast<uint32_t>(m_height),
        descs.data(),
        static_cast<uint32_t>(total_channels),
        interleaved.data(),
        metadata_str);
}

bool Document::load(const std::string& path) {
    auto result = engine::asset::pxg_load(path);
    if (!result) return false;

    const auto& pxg = *result;
    int w = static_cast<int>(pxg.header.width);
    int h = static_cast<int>(pxg.header.height);
    int total_channels = static_cast<int>(pxg.header.channels_per_pixel);

    // Try to parse metadata for layer info
    m_layers.clear();
    m_origin_x = 0;
    m_origin_y = 0;
    bool has_metadata = false;

    if (!pxg.metadata.empty()) {
        try {
            std::string meta_str(pxg.metadata.begin(), pxg.metadata.end());
            auto meta = nlohmann::json::parse(meta_str);

            // Parse origin point
            if (meta.contains("origin")) {
                m_origin_x = meta["origin"].value("x", 0);
                m_origin_y = meta["origin"].value("y", 0);
            }

            if (meta.contains("layers")) {
                for (const auto& lj : meta["layers"]) {
                    Layer layer;
                    layer.name = lj.value("name", "unnamed");
                    std::string type_str = lj.value("type", "uint8");
                    if (type_str == "color") {
                        layer.type = LayerType::Color;
                        layer.channels = 4;
                    } else if (type_str == "enum") {
                        layer.type = LayerType::Enum;
                        layer.channels = 1;
                        if (lj.contains("values")) {
                            for (const auto& v : lj["values"])
                                layer.enum_names.push_back(v.get<std::string>());
                        }
                    } else {
                        layer.type = LayerType::UInt8;
                        layer.channels = 1;
                    }
                    layer.opacity = lj.value("opacity", 1.0f);
                    layer.visible = lj.value("visible", true);
                    layer.data.resize(static_cast<size_t>(w) * h * layer.channels, 0);
                    m_layers.push_back(std::move(layer));
                }
                has_metadata = true;
            }
        } catch (...) {
            // Metadata parse failed, fall through to fallback
        }
    }

    // Fallback: if no metadata, check for color_r/g/b/a channels
    if (!has_metadata) {
        // Check if first 4 channels are color_r/g/b/a
        bool has_color = total_channels >= 4;
        if (has_color) {
            const char* expected[] = {"color_r", "color_g", "color_b", "color_a"};
            for (int i = 0; i < 4; ++i) {
                if (std::strncmp(pxg.channels[i].name, expected[i], 11) != 0) {
                    has_color = false;
                    break;
                }
            }
        }

        if (has_color) {
            Layer color;
            color.name = "color";
            color.type = LayerType::Color;
            color.channels = 4;
            color.data.resize(static_cast<size_t>(w) * h * 4, 255);
            m_layers.push_back(std::move(color));

            // Remaining channels become individual UInt8 layers
            for (int i = 4; i < total_channels; ++i) {
                Layer dl;
                dl.name = pxg.channels[i].name;
                dl.type = LayerType::UInt8;
                dl.channels = 1;
                dl.data.resize(static_cast<size_t>(w) * h, 0);
                m_layers.push_back(std::move(dl));
            }
        } else {
            // No recognized structure: create a Color layer from first 4 channels
            // (or pad if fewer) and treat rest as data
            Layer color;
            color.name = "color";
            color.type = LayerType::Color;
            color.channels = 4;
            color.data.resize(static_cast<size_t>(w) * h * 4, 255);
            m_layers.push_back(std::move(color));

            for (int i = 4; i < total_channels; ++i) {
                Layer dl;
                dl.name = pxg.channels[i].name;
                dl.type = LayerType::UInt8;
                dl.channels = 1;
                dl.data.resize(static_cast<size_t>(w) * h, 0);
                m_layers.push_back(std::move(dl));
            }
        }
    }

    // De-interleave pixel data into layer buffers
    size_t pixel_count = static_cast<size_t>(w) * h;
    for (size_t px = 0; px < pixel_count; ++px) {
        int src_ch = 0;
        for (auto& layer : m_layers) {
            for (int c = 0; c < layer.channels; ++c) {
                if (src_ch < total_channels) {
                    layer.data[px * layer.channels + c] =
                        pxg.pixels[px * total_channels + src_ch];
                }
                src_ch++;
            }
        }
    }

    m_width = w;
    m_height = h;
    return true;
}

} // namespace pixart
