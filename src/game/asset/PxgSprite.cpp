#include "game/asset/PxgSprite.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MaterialDefs.h"
#include "engine/physics/PixelBodyManager.h"
#include "game/world/MaterialData.h"
#include "game/GameLog.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace game {

std::optional<PxgSprite> load_pxg_sprite(const std::string& path) {
    auto pxg = engine::asset::pxg_load(path);
    if (!pxg) {
        GAME_ERR("load_pxg_sprite: Failed to load '%s'", path.c_str());
        return std::nullopt;
    }

    int w = static_cast<int>(pxg->header.width);
    int h = static_cast<int>(pxg->header.height);
    int total_ch = static_cast<int>(pxg->header.channels_per_pixel);
    int pixel_count = w * h;

    if (total_ch < 1 || pixel_count == 0) {
        GAME_ERR("load_pxg_sprite: Invalid dimensions in '%s'", path.c_str());
        return std::nullopt;
    }

    // Find material and temperature channel offsets from metadata JSON
    int material_offset = -1;
    int temperature_offset = -1;

    if (!pxg->metadata.empty()) {
        try {
            std::string meta_str(pxg->metadata.begin(), pxg->metadata.end());
            auto meta = nlohmann::json::parse(meta_str);

            if (meta.contains("layers")) {
                int ch_offset = 0;
                for (const auto& lj : meta["layers"]) {
                    std::string name = lj.value("name", "");
                    std::string type = lj.value("type", "uint8");
                    int channels = (type == "color") ? 4 : 1;

                    if (name == "material") {
                        material_offset = ch_offset;
                    } else if (name == "temperature") {
                        temperature_offset = ch_offset;
                    }

                    ch_offset += channels;
                }
            }
        } catch (const nlohmann::json::exception& e) {
            GAME_ERR("load_pxg_sprite: JSON parse error in '%s': %s", path.c_str(), e.what());
        }
    }

    // Fallback: if no metadata, try to find channels by name from channel descriptors
    if (material_offset < 0) {
        int ch = 0;
        for (const auto& desc : pxg->channels) {
            if (std::string(desc.name) == "material") {
                material_offset = ch;
            } else if (std::string(desc.name) == "temperature") {
                temperature_offset = ch;
            }
            ch++;
        }
    }

    if (material_offset < 0) {
        GAME_ERR("load_pxg_sprite: No 'material' channel found in '%s'", path.c_str());
        return std::nullopt;
    }

    // Extract material, category, and temperature data from interleaved pixel buffer
    PxgSprite sprite;
    sprite.width = w;
    sprite.height = h;
    sprite.materials.resize(pixel_count);
    sprite.categories.resize(pixel_count);
    sprite.temperatures.resize(pixel_count);

    const uint8_t* pixels = pxg->pixels.data();

    for (int i = 0; i < pixel_count; i++) {
        uint8_t mat = pixels[i * total_ch + material_offset];
        sprite.materials[i] = mat;

        // Look up category from material table (engine physics category)
        if (mat < MAT_COUNT) {
            sprite.categories[i] = MATERIAL_TABLE[mat].category;
        } else {
            sprite.categories[i] = engine::simulation::CAT_EMPTY;
        }

        if (temperature_offset >= 0) {
            sprite.temperatures[i] = pixels[i * total_ch + temperature_offset];
        } else {
            // Default temperature from material table
            if (mat < MAT_COUNT) {
                sprite.temperatures[i] = MATERIAL_TABLE[mat].default_temp;
            } else {
                sprite.temperatures[i] = 128;
            }
        }
    }

    GAME_LOG("load_pxg_sprite: Loaded '%s' (%dx%d, %d non-empty pixels)",
             path.c_str(), w, h,
             (int)std::count_if(sprite.materials.begin(), sprite.materials.end(),
                                [](uint8_t m) { return m != MAT_AIR; }));
    return sprite;
}

void paste_sprite(engine::simulation::PixelGrid& grid,
                  const PxgSprite& sprite, int x, int y) {
    // Clamp to grid bounds
    int x0 = x;
    int y0 = y;
    int x1 = x + sprite.width;
    int y1 = y + sprite.height;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > grid.width()) x1 = grid.width();
    if (y1 > grid.height()) y1 = grid.height();

    int paste_w = x1 - x0;
    int paste_h = y1 - y0;
    if (paste_w <= 0 || paste_h <= 0) return;

    // Build RGBA8UI buffer for the paste region
    // First read existing pixels so we only overwrite non-AIR sprite pixels
    std::vector<uint8_t> buf(paste_w * paste_h * 4);
    grid.readback_region(x0, y0, paste_w, paste_h, buf.data(),
                         static_cast<int>(buf.size()));

    for (int sy = 0; sy < paste_h; sy++) {
        for (int sx = 0; sx < paste_w; sx++) {
            int sprite_x = (x0 - x) + sx;
            int sprite_y = (y0 - y) + sy;
            int si = sprite_y * sprite.width + sprite_x;
            uint8_t mat = sprite.materials[si];

            if (mat == MAT_AIR) continue;

            int bi = (sy * paste_w + sx) * 4;
            buf[bi + 0] = mat;                       // R = material ID
            buf[bi + 1] = sprite.categories[si];    // G = category (engine physics)
            buf[bi + 2] = sprite.temperatures[si];  // B = temperature
            buf[bi + 3] = 0;                        // A = reserved
        }
    }

    grid.upload_both(x0, y0, paste_w, paste_h, buf.data());
}

engine::physics::PixelBody* spawn_sprite_body(
    engine::physics::PixelBodyManager& mgr,
    const PxgSprite& sprite, float wx, float wy, bool is_dynamic) {
    if (sprite.width <= 0 || sprite.height <= 0) return nullptr;

    // PixelBody takes both materials (game-defined) and categories (engine physics)
    auto* body = mgr.create_body(sprite.materials.data(), sprite.categories.data(),
                                  sprite.width, sprite.height,
                                  wx, wy, is_dynamic);
    if (!body) {
        GAME_ERR("spawn_sprite_body: Failed to create body (%dx%d)",
                 sprite.width, sprite.height);
    }
    return body;
}

std::optional<BodyPrefab> load_body_prefab(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        GAME_ERR("load_body_prefab: Cannot open '%s'", path.c_str());
        return std::nullopt;
    }

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(file);
    } catch (const nlohmann::json::exception& e) {
        GAME_ERR("load_body_prefab: JSON parse error in '%s': %s", path.c_str(), e.what());
        return std::nullopt;
    }

    if (!json.contains("sprite") || !json["sprite"].is_string()) {
        GAME_ERR("load_body_prefab: Missing 'sprite' field in '%s'", path.c_str());
        return std::nullopt;
    }

    BodyPrefab prefab;
    prefab.sprite_path = json["sprite"].get<std::string>();
    prefab.dynamic = json.value("dynamic", true);

    // Load the referenced sprite
    auto sprite = load_pxg_sprite(prefab.sprite_path);
    if (!sprite) {
        GAME_ERR("load_body_prefab: Failed to load sprite '%s' referenced by '%s'",
                 prefab.sprite_path.c_str(), path.c_str());
        return std::nullopt;
    }
    prefab.sprite = std::move(*sprite);

    GAME_LOG("load_body_prefab: Loaded '%s' (sprite='%s', dynamic=%s)",
             path.c_str(), prefab.sprite_path.c_str(),
             prefab.dynamic ? "true" : "false");
    return prefab;
}

} // namespace game
