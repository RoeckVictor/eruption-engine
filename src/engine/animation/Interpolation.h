#pragma once

#include "PropertyValue.h"
#include "Keyframe.h"

namespace engine::animation {

float ease_step(float t);
float ease_linear(float t);
float ease_in(float t);
float ease_out(float t);
float ease_in_out(float t);

float apply_easing(float t, InterpolationType type);

PropertyValue interpolate(
    const PropertyValue& a,
    const PropertyValue& b,
    float t,
    InterpolationType type
);

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline Vec2 lerp(const Vec2& a, const Vec2& b, float t) {
    return {
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t)
    };
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return {
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t),
        lerp(a.z, b.z, t)
    };
}

inline Vec4 lerp(const Vec4& a, const Vec4& b, float t) {
    return {
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t),
        lerp(a.z, b.z, t),
        lerp(a.w, b.w, t)
    };
}

}
