#pragma once

#include "engine/platform/GamepadCodes.h"
#include <cstring>

namespace engine::platform {

// Per-gamepad state, polled each frame from GLFW
struct Gamepad {
    static constexpr int BUTTON_COUNT = static_cast<int>(GamepadButton::COUNT);
    static constexpr int AXIS_COUNT   = static_cast<int>(GamepadAxis::COUNT);
    static constexpr float DEADZONE   = 0.15f;

    bool connected = false;
    const char* name = nullptr;

    bool  buttons[BUTTON_COUNT]      = {};
    bool  buttons_prev[BUTTON_COUNT] = {};
    float axes[AXIS_COUNT]           = {};

    void update(int glfw_joystick_id);

    bool is_button_held(GamepadButton btn) const {
        int i = static_cast<int>(btn);
        return i >= 0 && i < BUTTON_COUNT && buttons[i];
    }
    bool is_button_pressed(GamepadButton btn) const {
        int i = static_cast<int>(btn);
        return i >= 0 && i < BUTTON_COUNT && buttons[i] && !buttons_prev[i];
    }
    bool is_button_released(GamepadButton btn) const {
        int i = static_cast<int>(btn);
        return i >= 0 && i < BUTTON_COUNT && !buttons[i] && buttons_prev[i];
    }
    float get_axis(GamepadAxis axis) const {
        int i = static_cast<int>(axis);
        return (i >= 0 && i < AXIS_COUNT) ? axes[i] : 0.0f;
    }
};

}
