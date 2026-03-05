#include "AnimationTrack.h"
#include "Interpolation.h"
#include <algorithm>
#include <cmath>

namespace engine::animation {

void AnimationTrack::sort_keyframes() {
    std::sort(keyframes.begin(), keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
}

void AnimationTrack::add_keyframe(const Keyframe& kf) {
    // Find insertion point to maintain sorted order
    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), kf,
        [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
    keyframes.insert(it, kf);
}

void AnimationTrack::set_keyframe(float time, const PropertyValue& value,
                                  InterpolationType interp, float tolerance) {
    int idx = find_keyframe_at_time(time, tolerance);
    if (idx >= 0) {
        // Update existing keyframe
        keyframes[idx].value = value;
        keyframes[idx].interpolation = interp;
    } else {
        // Add new keyframe
        add_keyframe(Keyframe(time, value, interp));
    }
}

void AnimationTrack::remove_keyframe(size_t index) {
    if (index < keyframes.size()) {
        keyframes.erase(keyframes.begin() + static_cast<ptrdiff_t>(index));
    }
}

bool AnimationTrack::remove_keyframe_at_time(float time, float tolerance) {
    int idx = find_keyframe_at_time(time, tolerance);
    if (idx >= 0) {
        remove_keyframe(static_cast<size_t>(idx));
        return true;
    }
    return false;
}

int AnimationTrack::find_keyframe_at_time(float time, float tolerance) const {
    for (size_t i = 0; i < keyframes.size(); ++i) {
        if (std::abs(keyframes[i].time - time) <= tolerance) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

PropertyValue AnimationTrack::sample(float time) const {
    if (keyframes.empty()) {
        // Return default value based on type
        switch (value_type) {
            case PropertyValueType::Bool:   return false;
            case PropertyValueType::Int:    return 0;
            case PropertyValueType::Float:  return 0.0f;
            case PropertyValueType::Vec2:   return Vec2{};
            case PropertyValueType::Vec3:   return Vec3{};
            case PropertyValueType::Vec4:
            case PropertyValueType::Color:  return Vec4{};
            case PropertyValueType::String: return std::string{};
            default:                        return 0.0f;
        }
    }

    // Before first keyframe: return first value
    if (time <= keyframes.front().time) {
        return keyframes.front().value;
    }

    // After last keyframe: return last value
    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    // Find the two keyframes to interpolate between
    // Binary search for the first keyframe with time > input time
    auto it = std::upper_bound(keyframes.begin(), keyframes.end(), time,
        [](float t, const Keyframe& kf) { return t < kf.time; });

    // it points to the keyframe after our time
    // it-1 points to the keyframe before (or at) our time
    const Keyframe& kf_after = *it;
    const Keyframe& kf_before = *(it - 1);

    // Calculate normalized time between keyframes
    float duration = kf_after.time - kf_before.time;
    if (duration <= 0.0f) {
        return kf_before.value;
    }

    float t = (time - kf_before.time) / duration;

    // Interpolate using the "before" keyframe's interpolation type
    return interpolate(kf_before.value, kf_after.value, t, kf_before.interpolation);
}

float AnimationTrack::get_start_time() const {
    return keyframes.empty() ? 0.0f : keyframes.front().time;
}

float AnimationTrack::get_end_time() const {
    return keyframes.empty() ? 0.0f : keyframes.back().time;
}

} // namespace engine::animation
