#pragma once

namespace engine::platform {

// Standard gamepad buttons, mapping 1:1 to GLFW_GAMEPAD_BUTTON_* constants
enum class GamepadButton {
    A = 0, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DPadUp, DPadRight, DPadDown, DPadLeft,
    COUNT
};

// Standard gamepad axes, mapping 1:1 to GLFW_GAMEPAD_AXIS_* constants
enum class GamepadAxis {
    LeftX = 0, LeftY,
    RightX, RightY,
    LeftTrigger, RightTrigger,
    COUNT
};

}