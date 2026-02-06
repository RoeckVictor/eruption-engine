#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine::simulation { class PixelGrid; }
namespace engine::physics { class PixelBody; class PixelBodyManager; }

namespace game {

/// Extracted game-relevant data from a .pxg file.
/// Contains parallel arrays of material IDs, categories, and temperatures.
struct PxgSprite {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> materials;    // width*height material IDs (0 = empty)
    std::vector<uint8_t> categories;   // width*height physics categories (engine-defined)
    std::vector<uint8_t> temperatures; // width*height temperature values
};

/// Load a .pxg file and extract the material + temperature channels.
/// Parses the metadata JSON to find channels by name.
/// If the temperature channel is absent, defaults from MaterialData are used.
std::optional<PxgSprite> load_pxg_sprite(const std::string& path);

/// Paste a sprite onto the pixel grid at the given top-left world position.
/// Writes material (R) and temperature (B) to the grid. Skips AIR pixels.
void paste_sprite(engine::simulation::PixelGrid& grid,
                  const PxgSprite& sprite, int x, int y);

/// Create a rigidbody from the sprite's material data at a world position.
/// If is_dynamic is true, the body responds to physics; otherwise it's static.
engine::physics::PixelBody* spawn_sprite_body(
    engine::physics::PixelBodyManager& mgr,
    const PxgSprite& sprite, float wx, float wy, bool is_dynamic = true);

/// Definition loaded from a .prefab file. References a sprite and physics properties.
struct BodyPrefab {
    std::string sprite_path;    // Path to the .pxg sprite asset
    bool dynamic = true;        // Whether the body is dynamic (responds to forces)
    PxgSprite sprite;           // Loaded sprite data (populated by load_body_prefab)
};

/// Load a .prefab file, parse its JSON, and load the referenced sprite.
/// Returns nullopt if the prefab or its sprite cannot be loaded.
std::optional<BodyPrefab> load_body_prefab(const std::string& path);

} // namespace game
