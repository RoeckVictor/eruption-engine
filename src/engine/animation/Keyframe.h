#pragma once

#include "PropertyValue.h"

namespace engine::animation {

enum class InterpolationType {
    Step,
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

inline const char* interpolation_type_to_string(InterpolationType type) {
    switch (type) {
        case InterpolationType::Step:      return "step";
        case InterpolationType::Linear:    return "linear";
        case InterpolationType::EaseIn:    return "ease_in";
        case InterpolationType::EaseOut:   return "ease_out";
        case InterpolationType::EaseInOut: return "ease_in_out";
        default:                           return "linear";
    }
}

inline InterpolationType interpolation_type_from_string(const std::string& str) {
    if (str == "step")        return InterpolationType::Step;
    if (str == "linear")      return InterpolationType::Linear;
    if (str == "ease_in")     return InterpolationType::EaseIn;
    if (str == "ease_out")    return InterpolationType::EaseOut;
    if (str == "ease_in_out") return InterpolationType::EaseInOut;
    return InterpolationType::Linear;
}

struct Keyframe {
    float time = 0.0f;
    PropertyValue value;
    InterpolationType interpolation = InterpolationType::Linear;

    Keyframe() = default;
    Keyframe(float t, PropertyValue v, InterpolationType interp = InterpolationType::Linear)
        : time(t), value(std::move(v)), interpolation(interp) {}

    bool operator<(const Keyframe& other) const {
        return time < other.time;
    }
};

}
