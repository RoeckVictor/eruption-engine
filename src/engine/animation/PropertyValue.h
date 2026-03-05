#pragma once

#include <variant>
#include <string>

namespace engine::animation {

// Vector types for animation (stored as float arrays to match reflection system)
struct Vec2 {
    float x = 0.0f, y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    float& operator[](size_t i) { return (&x)[i]; }
    float operator[](size_t i) const { return (&x)[i]; }

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }

    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float& operator[](size_t i) { return (&x)[i]; }
    float operator[](size_t i) const { return (&x)[i]; }

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3& o) const { return !(*this == o); }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    float& operator[](size_t i) { return (&x)[i]; }
    float operator[](size_t i) const { return (&x)[i]; }

    Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }

    bool operator==(const Vec4& o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
    bool operator!=(const Vec4& o) const { return !(*this == o); }
};

enum class PropertyValueType {
    Bool,
    Int,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Color,
    String
};

using PropertyValue = std::variant<
    bool,
    int,
    float,
    Vec2,
    Vec3,
    Vec4,
    std::string
>;

inline PropertyValueType get_value_type(const PropertyValue& value) {
    return static_cast<PropertyValueType>(value.index());
}

inline bool is_numeric_type(PropertyValueType type) {
    switch (type) {
        case PropertyValueType::Float:
        case PropertyValueType::Int:
        case PropertyValueType::Vec2:
        case PropertyValueType::Vec3:
        case PropertyValueType::Vec4:
        case PropertyValueType::Color:
            return true;
        default:
            return false;
    }
}

inline const char* property_value_type_to_string(PropertyValueType type) {
    switch (type) {
        case PropertyValueType::Bool:   return "bool";
        case PropertyValueType::Int:    return "int";
        case PropertyValueType::Float:  return "float";
        case PropertyValueType::Vec2:   return "vec2";
        case PropertyValueType::Vec3:   return "vec3";
        case PropertyValueType::Vec4:   return "vec4";
        case PropertyValueType::Color:  return "color";
        case PropertyValueType::String: return "string";
        default:                        return "unknown";
    }
}

inline PropertyValueType property_value_type_from_string(const std::string& str) {
    if (str == "bool")   return PropertyValueType::Bool;
    if (str == "int")    return PropertyValueType::Int;
    if (str == "float")  return PropertyValueType::Float;
    if (str == "vec2")   return PropertyValueType::Vec2;
    if (str == "vec3")   return PropertyValueType::Vec3;
    if (str == "vec4")   return PropertyValueType::Vec4;
    if (str == "color")  return PropertyValueType::Color;
    if (str == "string") return PropertyValueType::String;
    return PropertyValueType::Float;
}

}
