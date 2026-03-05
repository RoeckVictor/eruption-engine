#include "Interpolation.h"
#include <cmath>

namespace engine::animation {

float ease_step(float t) {
    return t < 1.0f ? 0.0f : 1.0f;
}

float ease_linear(float t) {
    return t;
}

float ease_in(float t) {
    // Quadratic ease in
    return t * t;
}

float ease_out(float t) {
    // Quadratic ease out
    return t * (2.0f - t);
}

float ease_in_out(float t) {
    // Smoothstep
    return t * t * (3.0f - 2.0f * t);
}

float apply_easing(float t, InterpolationType type) {
    switch (type) {
        case InterpolationType::Step:      return ease_step(t);
        case InterpolationType::Linear:    return ease_linear(t);
        case InterpolationType::EaseIn:    return ease_in(t);
        case InterpolationType::EaseOut:   return ease_out(t);
        case InterpolationType::EaseInOut: return ease_in_out(t);
        default:                           return ease_linear(t);
    }
}

PropertyValue interpolate(
    const PropertyValue& a,
    const PropertyValue& b,
    float t,
    InterpolationType type
) {
    // Step interpolation: no blending, return a until t >= 1
    if (type == InterpolationType::Step) {
        return t < 1.0f ? a : b;
    }

    // Apply easing to t
    float eased_t = apply_easing(t, type);

    // Handle based on type (both values must be same type)
    return std::visit([&](const auto& val_a) -> PropertyValue {
        using T = std::decay_t<decltype(val_a)>;

        // Get value b as the same type
        if (!std::holds_alternative<T>(b)) {
            // Type mismatch: return a
            return a;
        }
        const T& val_b = std::get<T>(b);

        // Handle each type
        if constexpr (std::is_same_v<T, bool>) {
            // Bool: step interpolation only
            return eased_t < 0.5f ? val_a : val_b;
        } else if constexpr (std::is_same_v<T, int>) {
            // Int: round the interpolated value
            return static_cast<int>(std::round(
                static_cast<float>(val_a) + (static_cast<float>(val_b) - static_cast<float>(val_a)) * eased_t
            ));
        } else if constexpr (std::is_same_v<T, float>) {
            return lerp(val_a, val_b, eased_t);
        } else if constexpr (std::is_same_v<T, Vec2>) {
            return lerp(val_a, val_b, eased_t);
        } else if constexpr (std::is_same_v<T, Vec3>) {
            return lerp(val_a, val_b, eased_t);
        } else if constexpr (std::is_same_v<T, Vec4>) {
            return lerp(val_a, val_b, eased_t);
        } else if constexpr (std::is_same_v<T, std::string>) {
            // String: step interpolation only
            return eased_t < 0.5f ? val_a : val_b;
        } else {
            return a;
        }
    }, a);
}

} // namespace engine::animation
