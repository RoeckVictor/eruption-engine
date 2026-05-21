#pragma once

#include "engine/platform/KeyCode.h"
#include "engine/platform/Gamepad.h"

namespace engine::platform {

class Window;

class Input {
public:
    void update(Window& window);

    bool is_held(KeyCode key) const;
    bool is_pressed(KeyCode key) const;
    bool is_released(KeyCode key) const;

    bool is_mouse_held(MouseButton button) const;
    bool is_mouse_pressed(MouseButton button) const;
    bool is_mouse_released(MouseButton button) const;

    double mouse_x() const { return m_mouse_x; }
    double mouse_y() const { return m_mouse_y; }
    float scroll_y() const { return m_scroll_y; }

    bool is_gamepad_connected(int index = 0) const;
    int  connected_gamepad_count() const;
    bool is_gamepad_button_held(int index, GamepadButton btn) const;
    bool is_gamepad_button_pressed(int index, GamepadButton btn) const;
    bool is_gamepad_button_released(int index, GamepadButton btn) const;
    float get_gamepad_axis(int index, GamepadAxis axis) const;

    const Gamepad& gamepad(int index) const { return m_gamepads[index < MAX_GAMEPADS ? index : 0]; }

private:
    static constexpr int KEY_COUNT = static_cast<int>(KeyCode::COUNT);
    static constexpr int MOUSE_COUNT = static_cast<int>(MouseButton::COUNT);
    static constexpr int MAX_GAMEPADS = 4;

    bool m_keys[KEY_COUNT] = {};
    bool m_keys_prev[KEY_COUNT] = {};
    double m_mouse_x = 0.0;
    double m_mouse_y = 0.0;
    bool m_mouse[MOUSE_COUNT] = {};
    bool m_mouse_prev[MOUSE_COUNT] = {};
    float m_scroll_y = 0.0f;

    Gamepad m_gamepads[MAX_GAMEPADS];
};

}
