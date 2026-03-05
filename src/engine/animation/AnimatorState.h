#pragma once

#include "PropertyValue.h"
#include <string>

namespace engine::animation {

// A state in the animator state machine
struct AnimatorState {
    std::string name;
    std::string clip_path;
    float speed = 1.0f;
    Vec2 editor_position;

    AnimatorState() = default;
    AnimatorState(std::string state_name)
        : name(std::move(state_name)) {}
    AnimatorState(std::string state_name, std::string animation_clip_path)
        : name(std::move(state_name)), clip_path(std::move(animation_clip_path)) {}

    bool has_clip() const {
        return !clip_path.empty();
    }
};

}
