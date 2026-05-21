#include "engine/platform/InputAction.h"
#include "engine/platform/Input.h"
#include "engine/core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace engine::platform {

// ---- InputActionMap ----

void InputActionMap::add_action(InputAction action) {
    m_actions.push_back(std::move(action));
    rebuild_index();
}

void InputActionMap::remove_action(const std::string& name) {
    m_actions.erase(
        std::remove_if(m_actions.begin(), m_actions.end(),
                       [&](const InputAction& a) { return a.name == name; }),
        m_actions.end());
    rebuild_index();
}

InputAction* InputActionMap::find(const std::string& name) {
    auto it = m_index.find(name);
    return it != m_index.end() ? &m_actions[it->second] : nullptr;
}

const InputAction* InputActionMap::find(const std::string& name) const {
    auto it = m_index.find(name);
    return it != m_index.end() ? &m_actions[it->second] : nullptr;
}

void InputActionMap::evaluate(const Input& input) {
    for (auto& action : m_actions) {
        action._held_prev = action._held;

        if (action.type == ActionType::Button) {
            // Button: any binding active = held
            bool held = false;
            for (auto& b : action.bindings) {
                if (is_binding_active(b, input)) { held = true; break; }
            }
            action._held = held;
            action._pressed  = held && !action._held_prev;
            action._released = !held && action._held_prev;
            action._value = held ? 1.0f : 0.0f;
        } else {
            // Axis: sum contributions
            float value = 0.0f;

            // Negative bindings contribute -1
            for (auto& b : action.negative_bindings) {
                if (is_binding_active(b, input)) value -= 1.0f;
            }
            // Positive bindings contribute +1
            for (auto& b : action.positive_bindings) {
                if (is_binding_active(b, input)) value += 1.0f;
            }
            // Gamepad axis bindings contribute their raw value
            for (auto& b : action.bindings) {
                if (b.source_type == InputSourceType::GamepadAxis) {
                    value += get_binding_axis_value(b, input);
                }
            }

            // Clamp to [-1, 1]
            value = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);

            bool held = std::fabs(value) > 0.01f;
            action._held = held;
            action._pressed  = held && !action._held_prev;
            action._released = !held && action._held_prev;
            action._value = value;
        }
    }
}

bool InputActionMap::is_held(const std::string& name) const {
    auto* a = find(name);
    return a ? a->_held : false;
}

bool InputActionMap::is_pressed(const std::string& name) const {
    auto* a = find(name);
    return a ? a->_pressed : false;
}

bool InputActionMap::is_released(const std::string& name) const {
    auto* a = find(name);
    return a ? a->_released : false;
}

float InputActionMap::get_axis(const std::string& name) const {
    auto* a = find(name);
    return a ? a->_value : 0.0f;
}

bool InputActionMap::is_binding_active(const InputBinding& b, const Input& input) const {
    switch (b.source_type) {
    case InputSourceType::Key:
        return input.is_held(static_cast<KeyCode>(b.code));
    case InputSourceType::MouseButton:
        return input.is_mouse_held(static_cast<MouseButton>(b.code));
    case InputSourceType::GamepadButton:
        return input.is_gamepad_button_held(b.gamepad_index, static_cast<GamepadButton>(b.code));
    case InputSourceType::GamepadAxis: {
        float v = input.get_gamepad_axis(b.gamepad_index, static_cast<GamepadAxis>(b.code));
        return std::fabs(v) > 0.5f; // treat axis as button if past 50%
    }
    }
    return false;
}

float InputActionMap::get_binding_axis_value(const InputBinding& b, const Input& input) const {
    if (b.source_type == InputSourceType::GamepadAxis) {
        return input.get_gamepad_axis(b.gamepad_index, static_cast<GamepadAxis>(b.code));
    }
    return 0.0f;
}

void InputActionMap::rebuild_index() {
    m_index.clear();
    for (size_t i = 0; i < m_actions.size(); ++i) {
        m_index[m_actions[i].name] = i;
    }
}

// ---- JSON Serialization ----

static std::string source_type_to_string(InputSourceType t) {
    switch (t) {
    case InputSourceType::Key:           return "key";
    case InputSourceType::MouseButton:   return "mouse_button";
    case InputSourceType::GamepadButton: return "gamepad_button";
    case InputSourceType::GamepadAxis:   return "gamepad_axis";
    }
    return "key";
}

static InputSourceType string_to_source_type(const std::string& s) {
    if (s == "mouse_button")   return InputSourceType::MouseButton;
    if (s == "gamepad_button") return InputSourceType::GamepadButton;
    if (s == "gamepad_axis")   return InputSourceType::GamepadAxis;
    return InputSourceType::Key;
}

static nlohmann::json binding_to_json(const InputBinding& b) {
    nlohmann::json j;
    j["source"] = source_type_to_string(b.source_type);
    j["code"]   = b.code;
    if (b.source_type == InputSourceType::GamepadButton || b.source_type == InputSourceType::GamepadAxis) {
        j["gamepad"] = b.gamepad_index;
    }
    return j;
}

static InputBinding json_to_binding(const nlohmann::json& j) {
    InputBinding b;
    b.source_type  = string_to_source_type(j.value("source", "key"));
    b.code         = j.value("code", 0);
    b.gamepad_index = j.value("gamepad", 0);
    return b;
}

bool InputActionMap::load(const std::string& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) return false;

    try {
        nlohmann::json root;
        file >> root;

        m_actions.clear();
        for (auto& aj : root.value("actions", nlohmann::json::array())) {
            InputAction action;
            action.name = aj.value("name", "");
            action.type = (aj.value("type", "button") == "axis") ? ActionType::Axis : ActionType::Button;

            for (auto& bj : aj.value("bindings", nlohmann::json::array())) {
                action.bindings.push_back(json_to_binding(bj));
            }
            for (auto& bj : aj.value("negative", nlohmann::json::array())) {
                action.negative_bindings.push_back(json_to_binding(bj));
            }
            for (auto& bj : aj.value("positive", nlohmann::json::array())) {
                action.positive_bindings.push_back(json_to_binding(bj));
            }

            m_actions.push_back(std::move(action));
        }
        rebuild_index();
        ENGINE_LOG("InputActionMap: Loaded %zu actions from '%s'",
                   m_actions.size(), json_path.c_str());
        return true;
    } catch (const std::exception& e) {
        ENGINE_ERR("InputActionMap: Failed to parse '%s': %s", json_path.c_str(), e.what());
        return false;
    }
}

bool InputActionMap::save(const std::string& json_path) const {
    nlohmann::json root;
    nlohmann::json actions_array = nlohmann::json::array();

    for (auto& action : m_actions) {
        nlohmann::json aj;
        aj["name"] = action.name;
        aj["type"] = (action.type == ActionType::Axis) ? "axis" : "button";

        nlohmann::json bindings = nlohmann::json::array();
        for (auto& b : action.bindings) bindings.push_back(binding_to_json(b));
        aj["bindings"] = bindings;

        if (action.type == ActionType::Axis) {
            nlohmann::json neg = nlohmann::json::array();
            for (auto& b : action.negative_bindings) neg.push_back(binding_to_json(b));
            aj["negative"] = neg;

            nlohmann::json pos = nlohmann::json::array();
            for (auto& b : action.positive_bindings) pos.push_back(binding_to_json(b));
            aj["positive"] = pos;
        }

        actions_array.push_back(aj);
    }

    root["actions"] = actions_array;

    std::ofstream file(json_path);
    if (!file.is_open()) {
        ENGINE_ERR("InputActionMap: Cannot write '%s'", json_path.c_str());
        return false;
    }
    file << root.dump(4);
    ENGINE_LOG("InputActionMap: Saved %zu actions to '%s'", m_actions.size(), json_path.c_str());
    return true;
}

} // namespace engine::platform
