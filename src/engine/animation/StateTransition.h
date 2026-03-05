#pragma once

#include "TransitionCondition.h"
#include <vector>
#include <string>

namespace engine::animation {

inline constexpr const char* ANY_STATE = "__any__";

// A transition between two states in the animator state machine
struct StateTransition {
    std::string from_state;
    std::string to_state;
    std::vector<TransitionCondition> conditions;
    float blend_duration = 0.1f;
    bool has_exit_time = false;
    float exit_time = 1.0f;

    StateTransition() = default;
    StateTransition(std::string from, std::string to)
        : from_state(std::move(from)), to_state(std::move(to)) {}

    // Check if this is an "Any State" transition
    bool is_any_state_transition() const {
        return from_state == ANY_STATE;
    }

    void add_condition(const TransitionCondition& condition) {
        conditions.push_back(condition);
    }

    void set_exit_time(float normalized_time) {
        has_exit_time = true;
        exit_time = normalized_time;
    }

    void clear_exit_time() {
        has_exit_time = false;
        exit_time = 1.0f;
    }
};

}
