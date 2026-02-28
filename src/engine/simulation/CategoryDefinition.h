#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace engine::simulation {

/// Movement direction for category rules.
/// Encoded as 4 bits (0-7 for directions, 15 for none).
enum class Direction : uint8_t {
    DOWN = 0,
    DOWN_LEFT = 1,
    DOWN_RIGHT = 2,
    LEFT = 3,
    RIGHT = 4,
    UP = 5,
    UP_LEFT = 6,
    UP_RIGHT = 7,
    NONE = 15
};

/// Number of valid directions (excluding NONE).
constexpr int DIRECTION_COUNT = 8;

/// X offset for each direction.
constexpr int DIRECTION_DX[] = { 0, -1, +1, -1, +1, 0, -1, +1 };

/// Y offset for each direction (positive = down in screen space).
constexpr int DIRECTION_DY[] = { +1, +1, +1, 0, 0, -1, -1, -1 };

/// Get X offset for a direction.
inline int direction_dx(Direction d) {
    if (d == Direction::NONE) return 0;
    return DIRECTION_DX[static_cast<int>(d)];
}

/// Get Y offset for a direction.
inline int direction_dy(Direction d) {
    if (d == Direction::NONE) return 0;
    return DIRECTION_DY[static_cast<int>(d)];
}

/// Convert direction enum to string name.
inline const char* direction_name(Direction d) {
    switch (d) {
        case Direction::DOWN: return "down";
        case Direction::DOWN_LEFT: return "down_left";
        case Direction::DOWN_RIGHT: return "down_right";
        case Direction::LEFT: return "left";
        case Direction::RIGHT: return "right";
        case Direction::UP: return "up";
        case Direction::UP_LEFT: return "up_left";
        case Direction::UP_RIGHT: return "up_right";
        case Direction::NONE: return "none";
        default: return "unknown";
    }
}

/// Parse direction from string name.
inline Direction direction_from_name(const std::string& name) {
    if (name == "down") return Direction::DOWN;
    if (name == "down_left") return Direction::DOWN_LEFT;
    if (name == "down_right") return Direction::DOWN_RIGHT;
    if (name == "left") return Direction::LEFT;
    if (name == "right") return Direction::RIGHT;
    if (name == "up") return Direction::UP;
    if (name == "up_left") return Direction::UP_LEFT;
    if (name == "up_right") return Direction::UP_RIGHT;
    return Direction::NONE;
}

/// A single movement rule defining how a pixel can move in one direction.
struct MovementRule {
    Direction direction = Direction::NONE;
    uint8_t priority = 0;                     // Lower = checked first (0 is highest)
    std::vector<std::string> swap_with;       // Category internal_names this can swap with
    bool density_check = false;               // Only swap if target has lower density
};

/// Configuration for gradual dissipation (e.g., steam fading away).
struct DissipationConfig {
    bool enabled = false;
    float rate = 0.0f;                        // Probability (0.0-1.0) per tick
    std::string into_material;                // Material internal_name to transform into
};

/// Complete definition of a physical category.
/// Categories define how pixels move and what they can swap with.
struct CategoryDefinition {
    uint8_t id = 0;                           // Category ID (0-15, 0 = EMPTY)
    std::string name;                         // Display name ("Powder", "Liquid", etc.)
    std::string internal_name;                // Reference name for .material files
    bool mobile = false;                      // Whether pixels can move at all
    std::vector<MovementRule> movement_rules; // Movement rules (max 8)
    bool randomize_equal_priority = true;     // Shuffle equal-priority directions
    DissipationConfig dissipation;            // Optional decay behavior

    // Editor metadata
    std::string source_path;                  // File path (for editor save)
    bool is_engine_default = false;           // True for assets/categories/, read-only
};

/// Maximum number of categories supported.
constexpr int MAX_CATEGORIES = 16;

/// Maximum number of movement rules per category.
constexpr int MAX_MOVEMENT_RULES = 8;

/// Reserved category ID for empty/air.
constexpr int EMPTY_CATEGORY_ID = 0;

} // namespace engine::simulation
