#pragma once

#include "engine/animation/AnimationClip.h"
#include <string>
#include <unordered_map>

namespace engine::animation {

/// Component that tracks animation playback state.
/// Attach to any entity that has a sprite sheet texture.
struct Animator {
    std::unordered_map<std::string, AnimationClip> clips;

    std::string current_clip;
    int current_frame = 0;
    float elapsed = 0.0f;
    bool playing = true;
    bool finished = false;

    /// Get the current frame rect, or a default full-texture rect if no clip is active.
    FrameRect current_frame_rect() const {
        auto it = clips.find(current_clip);
        if (it == clips.end() || it->second.frames.empty()) {
            return FrameRect{0, 0, 1, 1};
        }
        int idx = current_frame % static_cast<int>(it->second.frames.size());
        return it->second.frames[idx];
    }

    /// Switch to a named clip. Resets frame and timer if different from current.
    void play(const std::string& clip_name) {
        if (current_clip != clip_name) {
            current_clip = clip_name;
            current_frame = 0;
            elapsed = 0.0f;
            playing = true;
            finished = false;
        }
    }

    void stop() {
        playing = false;
    }

    void reset() {
        current_frame = 0;
        elapsed = 0.0f;
        finished = false;
        playing = true;
    }
};

} // namespace engine::animation
