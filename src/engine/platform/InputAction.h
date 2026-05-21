#pragma once

#include "engine/platform/KeyCode.h"
#include "engine/platform/GamepadCodes.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace engine::platform {

class Input;

enum class InputSourceType { Key, MouseButton, GamepadButton, GamepadAxis };

struct InputBinding {
    InputSourceType source_type = InputSourceType::Key;
    int  code          = 0;
    int  gamepad_index = 0;
};

enum class ActionType { Button, Axis };

struct InputAction {
    std::string name;
    ActionType  type = ActionType::Button;

    std::vector<InputBinding> bindings;

    std::vector<InputBinding> negative_bindings;
    std::vector<InputBinding> positive_bindings;

    // Runtime state (not serialized)
    bool  _held     = false;
    bool  _held_prev = false;
    bool  _pressed  = false;
    bool  _released = false;
    float _value    = 0.0f;
};


// Project-level input action mapping
// Actions are named ("Jump", "MoveHorizontal") and can be bound to
// keyboard keys, mouse buttons, gamepad buttons, or gamepad axes
class InputActionMap {
public:
    void add_action(InputAction action);
    void remove_action(const std::string& name);
    InputAction* find(const std::string& name);
    const InputAction* find(const std::string& name) const;

    void evaluate(const Input& input);

    bool  is_held(const std::string& name) const;
    bool  is_pressed(const std::string& name) const;
    bool  is_released(const std::string& name) const;
    float get_axis(const std::string& name) const;

    bool load(const std::string& json_path);
    bool save(const std::string& json_path) const;

    std::vector<InputAction>& actions() { return m_actions; }
    const std::vector<InputAction>& actions() const { return m_actions; }

private:
    bool is_binding_active(const InputBinding& binding, const Input& input) const;
    float get_binding_axis_value(const InputBinding& binding, const Input& input) const;

    void rebuild_index();

    std::vector<InputAction> m_actions;
    std::unordered_map<std::string, size_t> m_index;
};

}