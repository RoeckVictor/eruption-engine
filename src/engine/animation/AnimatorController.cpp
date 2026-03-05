#include "AnimatorController.h"
#include <algorithm>

namespace engine::animation {

// State management

AnimatorState* AnimatorController::add_state(const std::string& state_name) {
    if (has_state(state_name)) {
        return get_state(state_name);
    }
    states.emplace_back(state_name);

    // Set as default if this is the first state
    if (default_state.empty()) {
        default_state = state_name;
    }

    return &states.back();
}

AnimatorState* AnimatorController::add_state(const std::string& state_name, const std::string& clip_path) {
    auto* state = add_state(state_name);
    if (state) {
        state->clip_path = clip_path;
    }
    return state;
}

const AnimatorState* AnimatorController::get_state(const std::string& state_name) const {
    for (const auto& state : states) {
        if (state.name == state_name) {
            return &state;
        }
    }
    return nullptr;
}

AnimatorState* AnimatorController::get_state(const std::string& state_name) {
    for (auto& state : states) {
        if (state.name == state_name) {
            return &state;
        }
    }
    return nullptr;
}

bool AnimatorController::remove_state(const std::string& state_name) {
    auto it = std::find_if(states.begin(), states.end(),
        [&](const AnimatorState& s) { return s.name == state_name; });

    if (it != states.end()) {
        states.erase(it);

        // Remove all transitions involving this state
        transitions.erase(
            std::remove_if(transitions.begin(), transitions.end(),
                [&](const StateTransition& t) {
                    return t.from_state == state_name || t.to_state == state_name;
                }),
            transitions.end()
        );

        // Update default state if necessary
        if (default_state == state_name) {
            default_state = states.empty() ? "" : states.front().name;
        }

        return true;
    }
    return false;
}

bool AnimatorController::has_state(const std::string& state_name) const {
    return get_state(state_name) != nullptr;
}

// Parameter management

AnimatorParameter* AnimatorController::add_parameter(const AnimatorParameter& param) {
    if (has_parameter(param.name)) {
        return get_parameter(param.name);
    }
    parameters.push_back(param);
    return &parameters.back();
}

AnimatorParameter* AnimatorController::add_bool(const std::string& param_name, bool default_value) {
    return add_parameter(AnimatorParameter::make_bool(param_name, default_value));
}

AnimatorParameter* AnimatorController::add_int(const std::string& param_name, int default_value) {
    return add_parameter(AnimatorParameter::make_int(param_name, default_value));
}

AnimatorParameter* AnimatorController::add_float(const std::string& param_name, float default_value) {
    return add_parameter(AnimatorParameter::make_float(param_name, default_value));
}

AnimatorParameter* AnimatorController::add_trigger(const std::string& param_name) {
    return add_parameter(AnimatorParameter::make_trigger(param_name));
}

const AnimatorParameter* AnimatorController::get_parameter(const std::string& param_name) const {
    for (const auto& param : parameters) {
        if (param.name == param_name) {
            return &param;
        }
    }
    return nullptr;
}

AnimatorParameter* AnimatorController::get_parameter(const std::string& param_name) {
    for (auto& param : parameters) {
        if (param.name == param_name) {
            return &param;
        }
    }
    return nullptr;
}

bool AnimatorController::remove_parameter(const std::string& param_name) {
    auto it = std::find_if(parameters.begin(), parameters.end(),
        [&](const AnimatorParameter& p) { return p.name == param_name; });

    if (it != parameters.end()) {
        parameters.erase(it);

        // Remove conditions using this parameter from all transitions
        for (auto& transition : transitions) {
            transition.conditions.erase(
                std::remove_if(transition.conditions.begin(), transition.conditions.end(),
                    [&](const TransitionCondition& c) { return c.parameter_name == param_name; }),
                transition.conditions.end()
            );
        }

        return true;
    }
    return false;
}

bool AnimatorController::has_parameter(const std::string& param_name) const {
    return get_parameter(param_name) != nullptr;
}

// Transition management

StateTransition* AnimatorController::add_transition(const std::string& from_state, const std::string& to_state) {
    transitions.emplace_back(from_state, to_state);
    return &transitions.back();
}

StateTransition* AnimatorController::add_any_state_transition(const std::string& to_state) {
    return add_transition(ANY_STATE, to_state);
}

bool AnimatorController::remove_transition(size_t index) {
    if (index < transitions.size()) {
        transitions.erase(transitions.begin() + static_cast<ptrdiff_t>(index));
        return true;
    }
    return false;
}

std::vector<const StateTransition*> AnimatorController::get_transitions_from(const std::string& state_name) const {
    std::vector<const StateTransition*> result;

    for (const auto& transition : transitions) {
        // Include direct transitions from this state
        if (transition.from_state == state_name) {
            result.push_back(&transition);
        }
        // Include "Any State" transitions (but not if going to current state)
        else if (transition.is_any_state_transition() && transition.to_state != state_name) {
            result.push_back(&transition);
        }
    }

    return result;
}

bool AnimatorController::is_valid() const {
    // Must have at least one state
    if (states.empty()) return false;

    // Default state must exist
    if (!has_state(default_state)) return false;

    // All transitions must reference valid states
    for (const auto& transition : transitions) {
        if (transition.from_state != ANY_STATE && !has_state(transition.from_state)) {
            return false;
        }
        if (!has_state(transition.to_state)) {
            return false;
        }

        // All conditions must reference valid parameters
        for (const auto& condition : transition.conditions) {
            if (!has_parameter(condition.parameter_name)) {
                return false;
            }
        }
    }

    return true;
}

} // namespace engine::animation
