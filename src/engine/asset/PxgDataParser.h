#pragma once

#include "PixelGridFile.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <string>
#include <vector>

namespace engine::asset {

/// Parsed pixel grid with channels separated by purpose.
struct ParsedPixelGrid {
    int width = 0;
    int height = 0;

    /// RGBA color extracted from the "color" layer (width * height * 4 bytes).
    /// Empty if no color layer was found.
    std::vector<uint8_t> color_rgba;

    /// Material IDs extracted from the "material" layer (width * height bytes).
    /// Empty if no material layer was found.
    std::vector<uint8_t> material_ids;

    bool has_color_layer = false;
    bool has_material_layer = false;

    /// Origin/pivot point in pixel coordinates (from metadata).
    int origin_x = 0;
    int origin_y = 0;
};

/// Parse a loaded PxgFile into structured channel data.
///
/// Examines the metadata JSON (if present) and channel descriptors to identify
/// the color layer and material layer, then de-interleaves them from the
/// packed pixel data.
///
/// Fallback behavior for files without metadata:
/// - Checks channel descriptor names for "color_r/g/b/a" pattern
/// - If no color channels found, treats byte 0 as material ID (legacy)
inline ParsedPixelGrid parse_pxg(const PxgFile& pxg) {
    ParsedPixelGrid result;
    result.width = static_cast<int>(pxg.header.width);
    result.height = static_cast<int>(pxg.header.height);

    int total_channels = static_cast<int>(pxg.header.channels_per_pixel);
    size_t pixel_count = static_cast<size_t>(result.width) * result.height;

    if (pixel_count == 0 || total_channels == 0 || pxg.pixels.empty()) {
        return result;
    }

    // Channel offsets we want to find
    int color_r_offset = -1;  // offset within interleaved pixel for color R
    int material_offset = -1; // offset within interleaved pixel for material

    // Try to parse metadata JSON for layer info
    bool parsed_metadata = false;
    if (!pxg.metadata.empty()) {
        try {
            std::string meta_str(pxg.metadata.begin(), pxg.metadata.end());
            auto meta = nlohmann::json::parse(meta_str);

            // Parse origin point
            if (meta.contains("origin") && meta["origin"].is_object()) {
                result.origin_x = meta["origin"].value("x", 0);
                result.origin_y = meta["origin"].value("y", 0);
            }

            if (meta.contains("layers") && meta["layers"].is_array()) {
                int ch_offset = 0;
                for (const auto& lj : meta["layers"]) {
                    std::string type_str = lj.value("type", "uint8");
                    std::string name = lj.value("name", "");
                    int layer_channels = (type_str == "color") ? 4 : 1;

                    if (type_str == "color" && color_r_offset < 0) {
                        color_r_offset = ch_offset;
                        result.has_color_layer = true;
                    }

                    if (name == "material" && material_offset < 0) {
                        material_offset = ch_offset;
                        result.has_material_layer = true;
                    }

                    ch_offset += layer_channels;
                }
                parsed_metadata = true;
            }
        } catch (const std::exception&) {
            // Metadata parse failed, fall through to descriptor-based detection
        }
    }

    // Fallback: examine channel descriptors
    if (!parsed_metadata && static_cast<int>(pxg.channels.size()) >= total_channels) {
        // Check for color_r/g/b/a pattern in first 4 channels
        if (total_channels >= 4) {
            const char* expected[] = {"color_r", "color_g", "color_b", "color_a"};
            bool has_color = true;
            for (int i = 0; i < 4; ++i) {
                if (std::strcmp(pxg.channels[i].name, expected[i]) != 0) {
                    has_color = false;
                    break;
                }
            }
            if (has_color) {
                color_r_offset = 0;
                result.has_color_layer = true;
            }
        }

        // Check for a channel named "material"
        for (int i = 0; i < total_channels; ++i) {
            if (std::strcmp(pxg.channels[i].name, "material") == 0) {
                // Compute offset: if color was found at offset 0, color takes 4 channels
                // We need to figure out the actual byte offset for this channel
                // In the descriptor fallback, channels map 1:1 to byte positions
                material_offset = i;
                result.has_material_layer = true;
                break;
            }
        }
    }

    // If no color layer found and no metadata, treat as legacy (byte 0 = material)
    if (!result.has_color_layer && !result.has_material_layer && total_channels > 0) {
        material_offset = 0;
        result.has_material_layer = true;
    }

    // Validate pixel buffer size before extraction
    size_t expected_size = pixel_count * static_cast<size_t>(total_channels);
    if (pxg.pixels.size() < expected_size) {
        return result; // Truncated pixel data — return without extraction
    }

    // Extract color RGBA
    if (result.has_color_layer && color_r_offset >= 0 &&
        color_r_offset + 3 < total_channels) {
        result.color_rgba.resize(pixel_count * 4);
        for (size_t px = 0; px < pixel_count; ++px) {
            size_t src = px * total_channels + color_r_offset;
            size_t dst = px * 4;
            result.color_rgba[dst + 0] = pxg.pixels[src + 0]; // R
            result.color_rgba[dst + 1] = pxg.pixels[src + 1]; // G
            result.color_rgba[dst + 2] = pxg.pixels[src + 2]; // B
            result.color_rgba[dst + 3] = pxg.pixels[src + 3]; // A
        }
    }

    // Extract material IDs
    if (result.has_material_layer && material_offset >= 0 &&
        material_offset < total_channels) {
        result.material_ids.resize(pixel_count);
        for (size_t px = 0; px < pixel_count; ++px) {
            result.material_ids[px] = pxg.pixels[px * total_channels + material_offset];
        }
    }

    return result;
}

} // namespace engine::asset
