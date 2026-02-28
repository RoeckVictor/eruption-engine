#pragma once

#include "engine/simulation/MaterialDefs.h"
#include <cstdint>

namespace engine::simulation {
/// Per-pixel data structure for SSBO-based grid storage.
struct PixelData {
    // Bytes 0-3: Simulation data
    uint8_t material;
    uint8_t category;
    uint8_t temperature;
    uint8_t flags;

    // Bytes 4-7: Per-pixel color (decoupled from material)
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint8_t color_a;

    PixelData()
        : material(0), category(CAT_EMPTY), temperature(128), flags(0)
        , color_r(0), color_g(0), color_b(0), color_a(0) {}

    PixelData(uint8_t mat, uint8_t cat, uint8_t temp, uint8_t flg = 0)
        : material(mat), category(cat), temperature(temp), flags(flg)
        , color_r(0), color_g(0), color_b(0), color_a(0) {}

    PixelData(uint8_t mat, uint8_t cat, uint8_t temp, uint8_t flg,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        : material(mat), category(cat), temperature(temp), flags(flg)
        , color_r(r), color_g(g), color_b(b), color_a(a) {}

    PixelData(uint8_t mat, uint8_t cat, uint8_t temp, uint8_t flg, uint32_t rgba)
        : material(mat), category(cat), temperature(temp), flags(flg)
        , color_r((rgba >> 24) & 0xFF)
        , color_g((rgba >> 16) & 0xFF)
        , color_b((rgba >> 8) & 0xFF)
        , color_a(rgba & 0xFF) {}

    void set_color(uint32_t rgba) {
        color_r = (rgba >> 24) & 0xFF;
        color_g = (rgba >> 16) & 0xFF;
        color_b = (rgba >> 8) & 0xFF;
        color_a = rgba & 0xFF;
    }

    uint32_t get_color() const {
        return (static_cast<uint32_t>(color_r) << 24) |
               (static_cast<uint32_t>(color_g) << 16) |
               (static_cast<uint32_t>(color_b) << 8) |
               static_cast<uint32_t>(color_a);
    }

    bool is_empty() const { return category == CAT_EMPTY; }
    bool is_solid() const { return category == CAT_STATIC || category == CAT_POWDER; }
    bool is_mobile() const {
        return category == CAT_POWDER || category == CAT_LIQUID || category == CAT_GAS;
    }
};

static_assert(sizeof(PixelData) == 8, "PixelData must be exactly 8 bytes");

}
