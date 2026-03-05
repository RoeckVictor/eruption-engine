#pragma once

#include "PropertyValue.h"
#include <nlohmann/json.hpp>

namespace engine::animation {

// Parse a JSON value into PropertyValue based on the expected type
// This centralizes the JSON-to-variant conversion used by animation loaders
inline PropertyValue parse_property_value(const nlohmann::json& j, PropertyValueType type) {
    switch (type) {
        case PropertyValueType::Bool:
            return j.get<bool>();
        case PropertyValueType::Int:
            return j.get<int>();
        case PropertyValueType::Float:
            return j.get<float>();
        case PropertyValueType::Vec2:
            if (j.is_array() && j.size() >= 2) {
                return Vec2{j[0].get<float>(), j[1].get<float>()};
            }
            return Vec2{};
        case PropertyValueType::Vec3:
            if (j.is_array() && j.size() >= 3) {
                return Vec3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
            }
            return Vec3{};
        case PropertyValueType::Vec4:
        case PropertyValueType::Color:
            if (j.is_array() && j.size() >= 4) {
                return Vec4{j[0].get<float>(), j[1].get<float>(),
                            j[2].get<float>(), j[3].get<float>()};
            }
            return Vec4{};
        case PropertyValueType::String:
            return j.get<std::string>();
        default:
            return 0.0f;
    }
}

// Serialize a PropertyValue to JSON
// Handles all variant types: bool, int, float, Vec2, Vec3, Vec4, string
inline nlohmann::json property_value_to_json(const PropertyValue& value) {
    return std::visit([](const auto& val) -> nlohmann::json {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int> ||
                     std::is_same_v<T, float> || std::is_same_v<T, std::string>) {
            return val;
        } else if constexpr (std::is_same_v<T, Vec2>) {
            return {val.x, val.y};
        } else if constexpr (std::is_same_v<T, Vec3>) {
            return {val.x, val.y, val.z};
        } else if constexpr (std::is_same_v<T, Vec4>) {
            return {val.x, val.y, val.z, val.w};
        } else {
            return nullptr;
        }
    }, value);
}

}
