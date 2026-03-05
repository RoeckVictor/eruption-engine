#pragma once

#include "AnimatorParameter.h"
#include "AnimatorState.h"
#include "StateTransition.h"
#include <deque>
#include <vector>
#include <string>

namespace engine::animation {

// The complete state machine definition for an Animator
struct AnimatorController {
    std::string name;
    std::string default_state;
    std::deque<AnimatorParameter> parameters;
    std::deque<AnimatorState> states;
    std::deque<StateTransition> transitions;

    AnimatorController() = default;
    AnimatorController(std::string controller_name)
        : name(std::move(controller_name)) {}

    AnimatorState* add_state(const std::string& state_name);
    AnimatorState* add_state(const std::string& state_name, const std::string& clip_path);
    const AnimatorState* get_state(const std::string& state_name) const;
    AnimatorState* get_state(const std::string& state_name);
    bool remove_state(const std::string& state_name);
    bool has_state(const std::string& state_name) const;

    AnimatorParameter* add_parameter(const AnimatorParameter& param);
    AnimatorParameter* add_bool(const std::string& name, bool default_value = false);
    AnimatorParameter* add_int(const std::string& name, int default_value = 0);
    AnimatorParameter* add_float(const std::string& name, float default_value = 0.0f);
    AnimatorParameter* add_trigger(const std::string& name);
    const AnimatorParameter* get_parameter(const std::string& name) const;
    AnimatorParameter* get_parameter(const std::string& name);
    bool remove_parameter(const std::string& name);
    bool has_parameter(const std::string& name) const;

    StateTransition* add_transition(const std::string& from_state, const std::string& to_state);
    StateTransition* add_any_state_transition(const std::string& to_state);
    bool remove_transition(size_t index);

    std::vector<const StateTransition*> get_transitions_from(const std::string& state_name) const;

    size_t transition_count() const { return transitions.size(); }
    const StateTransition& get_transition(size_t index) const { return transitions[index]; }
    StateTransition& get_transition(size_t index) { return transitions[index]; }

    bool is_valid() const;
};

}
