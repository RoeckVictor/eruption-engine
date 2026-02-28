#pragma once

#include "engine/simulation/MaterialDefs.h"
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace engine::simulation {

enum class InteractionType : uint8_t {
    TEMPERATURE = 0,
    CONTACT = 1,
    TIME_DECAY = 2,
    RANDOM = 3,
    FLAG_BASED = 4,
};

enum class EffectType : uint8_t {
    TRANSFORM = 0,
    TRANSFORM_NEIGHBOR = 1,
    SET_TEMP = 2,
    CHANGE_TEMP = 3,
    SET_NEIGHBOR_TEMP = 4,
    CHANGE_NEIGHBOR_TEMP = 5,
    SET_FLAG = 6,
    CLEAR_FLAG = 7,
    SET_NEIGHBOR_FLAG = 8,
    SPAWN_PARTICLE = 9,
    DESTROY = 10,
};

enum class ColorBehavior : uint8_t {
    NONE = 0,
    REPLACE = 1,
    INHERIT = 2,
    BLEND = 3,
    ADD = 4,
    SUBTRACT = 5,
};

enum class SpawnDirection : uint8_t {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3,
    RANDOM = 4,
};

struct InteractionConditions {
    uint8_t temp_above = 0;
    uint8_t temp_below = 255;

    std::vector<std::string> contact_with;
    std::string contact_with_category;

    uint8_t self_has_flag = 0;
    uint8_t self_not_has_flag = 0;
    uint8_t neighbor_has_flag = 0;
    uint8_t contact_with_flag = 0;
};

struct InteractionEffect {
    EffectType type = EffectType::TRANSFORM;
    std::string material_name;
    int16_t delta = 0;
    uint8_t value = 0;
    uint8_t flag = 0;
    SpawnDirection direction = SpawnDirection::UP;
    ColorBehavior color_behavior = ColorBehavior::NONE;
    uint32_t color_delta = 0;
};

struct Interaction {
    std::string id;
    InteractionType type = InteractionType::TEMPERATURE;
    uint8_t priority = 50;
    float probability = 1.0f;
    uint16_t sim_step_threshold = 0;
    InteractionConditions conditions;
    std::vector<InteractionEffect> effects;
};

/// Runtime material definition loaded from JSON.
/// Represents a single material with all its properties:
/// - Physical simulation (density, category)
/// - Thermal properties (melting, boiling)
/// - Rendering (color, display name)
/// - Gameplay flags
struct MaterialDefinition {
    uint8_t id = 0;
    std::string name;
    std::string internal_name;

    uint8_t density = 0;
    MaterialCategory category = CAT_EMPTY;

    uint8_t melt_point = 0;
    uint8_t boil_point = 0;
    uint8_t melt_into = 0;
    uint8_t boil_into = 0;
    uint8_t default_temp = 128;
    float conductivity = 0.5f;

    uint8_t flags = 0;

    uint32_t color = 0xFFFFFFFF;
    uint8_t color_variance_hue = 0;
    uint8_t color_variance_saturation = 0;
    uint8_t color_variance_lightness = 0;

    std::vector<Interaction> interactions;

    std::string source_path;

    // Pack this definition into a MaterialSlot for GPU upload
    // Note: interaction_offset and interaction_count are set by InteractionCompiler
    MaterialSlot to_material_slot() const {
        MaterialSlot slot{};
        slot.density = density;
        slot.category = static_cast<uint8_t>(category);
        slot.interaction_offset = 0;
        slot.interaction_count = 0;
        slot.flags = flags;
        slot.default_temp = default_temp;
        slot.conductivity_u8 = static_cast<uint8_t>(
            std::min(255.0f, std::max(0.0f, conductivity * 255.0f)));
        return slot;
    }

    // Unpack a MaterialSlot back to a definition (for debugging)
    static MaterialDefinition from_material_slot(const MaterialSlot& slot, uint8_t id) {
        MaterialDefinition def;
        def.id = id;
        def.density = slot.density;
        def.category = static_cast<MaterialCategory>(slot.category);
        def.flags = slot.flags;
        def.default_temp = slot.default_temp;
        def.conductivity = slot.conductivity_u8 / 255.0f;
        return def;
    }
};

constexpr uint8_t MAT_FLAG_HAZARD = 0x01;
constexpr uint8_t MAT_FLAG_FLAMMABLE = 0x02;
constexpr uint8_t MAT_FLAG_CONDUCTIVE = 0x04;

// Note: category_to_string and string_to_category have been removed.
// Use CategoryLibrary for category name resolution instead.

inline const char* interaction_type_to_string(InteractionType type) {
    switch (type) {
        case InteractionType::TEMPERATURE: return "temperature";
        case InteractionType::CONTACT: return "contact";
        case InteractionType::TIME_DECAY: return "time_decay";
        case InteractionType::RANDOM: return "random";
        case InteractionType::FLAG_BASED: return "flag_based";
        default: return "temperature";
    }
}

inline InteractionType string_to_interaction_type(const std::string& str) {
    if (str == "temperature") return InteractionType::TEMPERATURE;
    if (str == "contact") return InteractionType::CONTACT;
    if (str == "time_decay") return InteractionType::TIME_DECAY;
    if (str == "random") return InteractionType::RANDOM;
    if (str == "flag_based") return InteractionType::FLAG_BASED;
    return InteractionType::TEMPERATURE;
}

inline const char* effect_type_to_string(EffectType type) {
    switch (type) {
        case EffectType::TRANSFORM: return "transform";
        case EffectType::TRANSFORM_NEIGHBOR: return "transform_neighbor";
        case EffectType::SET_TEMP: return "set_temp";
        case EffectType::CHANGE_TEMP: return "change_temp";
        case EffectType::SET_NEIGHBOR_TEMP: return "set_neighbor_temp";
        case EffectType::CHANGE_NEIGHBOR_TEMP: return "change_neighbor_temp";
        case EffectType::SET_FLAG: return "set_flag";
        case EffectType::CLEAR_FLAG: return "clear_flag";
        case EffectType::SET_NEIGHBOR_FLAG: return "set_neighbor_flag";
        case EffectType::SPAWN_PARTICLE: return "spawn_particle";
        case EffectType::DESTROY: return "destroy";
        default: return "transform";
    }
}

inline EffectType string_to_effect_type(const std::string& str) {
    if (str == "transform") return EffectType::TRANSFORM;
    if (str == "transform_neighbor") return EffectType::TRANSFORM_NEIGHBOR;
    if (str == "set_temp") return EffectType::SET_TEMP;
    if (str == "change_temp") return EffectType::CHANGE_TEMP;
    if (str == "set_neighbor_temp") return EffectType::SET_NEIGHBOR_TEMP;
    if (str == "change_neighbor_temp") return EffectType::CHANGE_NEIGHBOR_TEMP;
    if (str == "set_flag") return EffectType::SET_FLAG;
    if (str == "clear_flag") return EffectType::CLEAR_FLAG;
    if (str == "set_neighbor_flag") return EffectType::SET_NEIGHBOR_FLAG;
    if (str == "spawn_particle") return EffectType::SPAWN_PARTICLE;
    if (str == "destroy") return EffectType::DESTROY;
    return EffectType::TRANSFORM;
}

inline const char* color_behavior_to_string(ColorBehavior behavior) {
    switch (behavior) {
        case ColorBehavior::NONE: return "none";
        case ColorBehavior::REPLACE: return "replace";
        case ColorBehavior::INHERIT: return "inherit";
        case ColorBehavior::BLEND: return "blend";
        case ColorBehavior::ADD: return "add";
        case ColorBehavior::SUBTRACT: return "subtract";
        default: return "none";
    }
}

inline ColorBehavior string_to_color_behavior(const std::string& str) {
    if (str == "none") return ColorBehavior::NONE;
    if (str == "replace") return ColorBehavior::REPLACE;
    if (str == "inherit") return ColorBehavior::INHERIT;
    if (str == "blend") return ColorBehavior::BLEND;
    if (str == "add") return ColorBehavior::ADD;
    if (str == "subtract") return ColorBehavior::SUBTRACT;
    return ColorBehavior::NONE;
}

inline uint8_t string_to_flag(const std::string& str) {
    if (str == "hazard") return MAT_FLAG_HAZARD;
    if (str == "flammable") return MAT_FLAG_FLAMMABLE;
    if (str == "conductive") return MAT_FLAG_CONDUCTIVE;
    return 0;
}

inline uint8_t strings_to_flags(const std::vector<std::string>& strs) {
    uint8_t flags = 0;
    for (const auto& s : strs) {
        flags |= string_to_flag(s);
    }
    return flags;
}

}
