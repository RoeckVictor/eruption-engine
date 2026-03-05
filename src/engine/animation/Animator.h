#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace engine::animation {

// The Animator component drives animation playback via a state machine
struct Animator {
    bool enabled = true;
    std::string controller_path;

    std::string current_state;
    float state_time = 0.0f;
    float previous_state_time = 0.0f;

    std::string blend_from_state;
    float blend_from_time = 0.0f;
    float blend_progress = 0.0f;
    float blend_duration = 0.0f;
    bool is_blending = false;

    std::unordered_map<std::string, bool> bool_params;
    std::unordered_map<std::string, int> int_params;
    std::unordered_map<std::string, float> float_params;
    std::unordered_map<std::string, bool> trigger_params;

    std::vector<std::string> pending_events;

    bool initialized = false;

    void set_bool(const std::string& name, bool value) {
        bool_params[name] = value;
    }
    void set_int(const std::string& name, int value) {
        int_params[name] = value;
    }
    void set_float(const std::string& name, float value) {
        float_params[name] = value;
    }
    void set_trigger(const std::string& name) {
        trigger_params[name] = true;
    }

    bool get_bool(const std::string& name) const {
        auto it = bool_params.find(name);
        return it != bool_params.end() ? it->second : false;
    }
    int get_int(const std::string& name) const {
        auto it = int_params.find(name);
        return it != int_params.end() ? it->second : 0;
    }
    float get_float(const std::string& name) const {
        auto it = float_params.find(name);
        return it != float_params.end() ? it->second : 0.0f;
    }
    bool get_trigger(const std::string& name) const {
        auto it = trigger_params.find(name);
        return it != trigger_params.end() ? it->second : false;
    }

    bool is_in_state(const std::string& state_name) const {
        return current_state == state_name;
    }

    float get_normalized_time(float clip_duration) const {
        if (clip_duration <= 0.0f) return 0.0f;
        return state_time / clip_duration;
    }

    bool has_event(const std::string& event_name) const {
        for (const auto& e : pending_events) {
            if (e == event_name) return true;
        }
        return false;
    }

    bool consume_event(const std::string& event_name) {
        for (auto it = pending_events.begin(); it != pending_events.end(); ++it) {
            if (*it == event_name) {
                pending_events.erase(it);
                return true;
            }
        }
        return false;
    }

    void clear_events() {
        pending_events.clear();
    }

    void reset_triggers() {
        for (auto& [name, value] : trigger_params) {
            value = false;
        }
    }

    void reset() {
        current_state.clear();
        state_time = 0.0f;
        previous_state_time = 0.0f;
        blend_from_state.clear();
        blend_from_time = 0.0f;
        blend_progress = 0.0f;
        blend_duration = 0.0f;
        is_blending = false;
        bool_params.clear();
        int_params.clear();
        float_params.clear();
        trigger_params.clear();
        pending_events.clear();
        initialized = false;
    }

    void start_blend(const std::string& to_state, float duration) {
        if (duration > 0.0f && !current_state.empty()) {
            blend_from_state = current_state;
            blend_from_time = state_time;
            blend_progress = 0.0f;
            blend_duration = duration;
            is_blending = true;
        }
        current_state = to_state;
        state_time = 0.0f;
        previous_state_time = 0.0f;
    }

    void set_state(const std::string& state_name) {
        current_state = state_name;
        state_time = 0.0f;
        previous_state_time = 0.0f;
        is_blending = false;
        blend_from_state.clear();
    }
};

}
