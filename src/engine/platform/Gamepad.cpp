#include "engine/platform/Gamepad.h"
#include <GLFW/glfw3.h>
#include <cstring>
#include <cmath>

namespace engine::platform {

void Gamepad::update(int glfw_joystick_id) {
    // Save previous button state
    std::memcpy(buttons_prev, buttons, sizeof(buttons));

    // Check if this slot has a recognized gamepad
    if (!glfwJoystickIsGamepad(glfw_joystick_id)) {
        if (connected) {
            // Was connected, now disconnected
            connected = false;
            name = nullptr;
            std::memset(buttons, 0, sizeof(buttons));
            std::memset(axes, 0, sizeof(axes));
        }
        return;
    }

    connected = true;
    name = glfwGetGamepadName(glfw_joystick_id);

    GLFWgamepadstate state;
    if (glfwGetGamepadState(glfw_joystick_id, &state)) {
        // Copy button states
        for (int i = 0; i < BUTTON_COUNT && i <= GLFW_GAMEPAD_BUTTON_LAST; ++i) {
            buttons[i] = (state.buttons[i] == GLFW_PRESS);
        }

        // Copy axis values with deadzone
        for (int i = 0; i < AXIS_COUNT && i <= GLFW_GAMEPAD_AXIS_LAST; ++i) {
            float value = state.axes[i];
            // Apply deadzone to stick axes (not triggers)
            if (i < 4) { // LeftX, LeftY, RightX, RightY
                if (std::fabs(value) < DEADZONE) {
                    value = 0.0f;
                }
            }
            axes[i] = value;
        }
    }
}

} // namespace engine::platform
