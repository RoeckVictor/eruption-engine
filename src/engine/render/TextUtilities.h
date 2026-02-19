#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace engine::render {

// Forward declaration
struct DynamicFont;

/// Decode a UTF-8 string into a vector of Unicode codepoints.
/// Handles 1-4 byte sequences, skipping invalid sequences.
std::vector<uint32_t> decode_utf8(const std::string& text);

/// Measure the width of a line of text in pixels.
/// @param codepoints Pre-decoded codepoints
/// @param start Start index in codepoints
/// @param end End index (exclusive)
/// @param font The font to use for metrics
/// @param font_size Quantized font size for atlas lookup
/// @param render_scale Scale factor for final pixel width
float measure_line_width(
    const std::vector<uint32_t>& codepoints,
    size_t start, size_t end,
    DynamicFont& font,
    int font_size,
    float render_scale
);

/// Build a font variant path from a base path.
/// @param base_path Base font path (e.g., "assets/fonts/Roboto.ttf")
/// @param bold Whether to use bold variant
/// @param italic Whether to use italic variant
/// @return Modified path (e.g., "assets/fonts/Roboto-Bold.ttf")
inline std::string build_font_path(const std::string& base_path, bool bold, bool italic) {
    auto dot = base_path.rfind('.');
    std::string base = (dot != std::string::npos) ? base_path.substr(0, dot) : base_path;
    std::string ext = (dot != std::string::npos) ? base_path.substr(dot) : ".ttf";

    if (bold && italic) {
        return base + "-BoldItalic" + ext;
    } else if (bold) {
        return base + "-Bold" + ext;
    } else if (italic) {
        return base + "-Italic" + ext;
    }
    return base_path;
}

/// Normalize a font path to canonical form for asset loading.
/// Handles ./prefix, fonts/ prefix, and assets/fonts/ prefix.
inline std::string normalize_font_path(const std::string& path) {
    std::string normalized = path;

    // Strip leading ./
    while (normalized.size() >= 2 && normalized[0] == '.' &&
           (normalized[1] == '/' || normalized[1] == '\\')) {
        normalized = normalized.substr(2);
    }

    // If already starts with assets/fonts/, it's good
    if (normalized.find("assets/fonts/") == 0 || normalized.find("assets\\fonts\\") == 0) {
        return normalized;
    }

    // If starts with fonts/, prepend assets/
    if (normalized.find("fonts/") == 0 || normalized.find("fonts\\") == 0) {
        return "assets/" + normalized;
    }

    // Otherwise, it's just a filename - prepend full path
    return "assets/fonts/" + normalized;
}

} // namespace engine::render
