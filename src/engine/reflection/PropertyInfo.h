#pragma once

#include <string>
#include <cstddef>
#include <vector>

namespace engine::reflection {

/// Property type categories for inspector rendering.
enum class PropertyType {
    Unknown,
    Bool,
    Int,
    Float,
    Double,
    String,
    Vec2,       // float[2]
    Vec3,       // float[3]
    Vec4,       // float[4] or color
    Color,      // RGBA color
    Enum,       // Enumeration
    EntityRef,  // Reference to another entity
    AssetRef,   // Reference to an asset
};

/// Flags for property behavior in the inspector.
enum class PropertyFlags : uint32_t {
    None        = 0,
    ReadOnly    = 1 << 0,   // Cannot be edited in inspector
    Hidden      = 1 << 1,   // Not shown in inspector
    Angle       = 1 << 2,   // Display as angle (degrees)
    Color       = 1 << 3,   // Display as color picker
    Slider      = 1 << 4,   // Display as slider
    Multiline   = 1 << 5,   // String uses multiline text box
    Header      = 1 << 6,   // Show collapsible header before this
    Space       = 1 << 7,   // Add spacing before this
    Normalized  = 1 << 8,   // Value is 0-1 range
    Percentage  = 1 << 9,   // Display as percentage
};

inline PropertyFlags operator|(PropertyFlags a, PropertyFlags b) {
    return static_cast<PropertyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline PropertyFlags operator&(PropertyFlags a, PropertyFlags b) {
    return static_cast<PropertyFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool has_flag(PropertyFlags flags, PropertyFlags flag) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

/// Metadata for a single reflected property.
struct PropertyInfo {
    std::string name;           // C++ member name
    std::string display_name;   // Human-readable name for inspector
    std::string tooltip;        // Tooltip text
    size_t offset = 0;          // Offset within the struct
    size_t size = 0;            // Size in bytes
    PropertyType type = PropertyType::Unknown;
    PropertyFlags flags = PropertyFlags::None;

    // For sliders/ranges
    float min_value = 0.0f;
    float max_value = 1.0f;
    float step = 0.1f;

    // For enums
    std::vector<std::string> enum_names;

    /// Get a pointer to the property value within an object.
    template<typename T>
    T* get_ptr(void* obj) const {
        return reinterpret_cast<T*>(static_cast<char*>(obj) + offset);
    }

    template<typename T>
    const T* get_ptr(const void* obj) const {
        return reinterpret_cast<const T*>(static_cast<const char*>(obj) + offset);
    }
};

/// Helper to determine PropertyType from C++ types.
template<typename T>
constexpr PropertyType get_property_type() {
    if constexpr (std::is_same_v<T, bool>) {
        return PropertyType::Bool;
    } else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, int32_t>) {
        return PropertyType::Int;
    } else if constexpr (std::is_same_v<T, float>) {
        return PropertyType::Float;
    } else if constexpr (std::is_same_v<T, double>) {
        return PropertyType::Double;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return PropertyType::String;
    } else if constexpr (std::is_enum_v<T>) {
        return PropertyType::Enum;
    } else {
        return PropertyType::Unknown;
    }
}

} // namespace engine::reflection
