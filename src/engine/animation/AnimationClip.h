#pragma once

#include <string>
#include <vector>

namespace engine::animation {

/// A rectangular region within a sprite sheet texture (UV coordinates).
struct FrameRect {
    float u0 = 0.0f, v0 = 0.0f;
    float u1 = 1.0f, v1 = 1.0f;
};

/// A named sequence of frames with timing.
struct AnimationClip {
    std::string name;
    std::vector<FrameRect> frames;
    float frame_duration = 0.1f;
    bool looping = true;
};

} // namespace engine::animation
