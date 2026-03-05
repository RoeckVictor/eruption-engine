#pragma once

#include <string>

namespace engine::animation {

// An event that fires at a specific time during animation playback
struct AnimationEvent {
    float time = 0.0f;
    std::string name;
    std::string parameter;

    AnimationEvent() = default;
    AnimationEvent(float t, std::string n, std::string param = "")
        : time(t), name(std::move(n)), parameter(std::move(param)) {}

    bool operator<(const AnimationEvent& other) const {
        return time < other.time;
    }
};

}
