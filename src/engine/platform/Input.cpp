#include "engine/platform/Input.h"
#include "engine/platform/Window.h"
#include "engine/profiler/Profiler.h"
#include <GLFW/glfw3.h>
#include <cstring>

namespace engine::platform {

void Input::update(Window& window) {
    PROFILE_SCOPE("Input::update");
    memcpy(m_keys_prev, m_keys, sizeof(m_keys));
    memcpy(m_mouse_prev, m_mouse, sizeof(m_mouse));

    for (int i = 0; i < KEY_COUNT; i++) {
        m_keys[i] = window.is_key_down(static_cast<KeyCode>(i));
    }

    for (int i = 0; i < MOUSE_COUNT; i++) {
        m_mouse[i] = window.is_mouse_button_down(static_cast<MouseButton>(i));
    }

    window.get_cursor_position(m_mouse_x, m_mouse_y);

    // Consume accumulated scroll from Window
    m_scroll_y = window.consume_scroll();

    // Poll gamepads
    for (int i = 0; i < MAX_GAMEPADS; ++i) {
        m_gamepads[i].update(GLFW_JOYSTICK_1 + i);
    }
}

bool Input::is_held(KeyCode key) const {
    int i = static_cast<int>(key);
    return i >= 0 && i < KEY_COUNT && m_keys[i];
}

bool Input::is_pressed(KeyCode key) const {
    int i = static_cast<int>(key);
    return i >= 0 && i < KEY_COUNT && m_keys[i] && !m_keys_prev[i];
}

bool Input::is_released(KeyCode key) const {
    int i = static_cast<int>(key);
    return i >= 0 && i < KEY_COUNT && !m_keys[i] && m_keys_prev[i];
}

bool Input::is_mouse_held(MouseButton button) const {
    int i = static_cast<int>(button);
    return i >= 0 && i < MOUSE_COUNT && m_mouse[i];
}

bool Input::is_mouse_pressed(MouseButton button) const {
    int i = static_cast<int>(button);
    return i >= 0 && i < MOUSE_COUNT && m_mouse[i] && !m_mouse_prev[i];
}

bool Input::is_mouse_released(MouseButton button) const {
    int i = static_cast<int>(button);
    return i >= 0 && i < MOUSE_COUNT && !m_mouse[i] && m_mouse_prev[i];
}

// --- Gamepad ---

bool Input::is_gamepad_connected(int index) const {
    return index >= 0 && index < MAX_GAMEPADS && m_gamepads[index].connected;
}

int Input::connected_gamepad_count() const {
    int count = 0;
    for (int i = 0; i < MAX_GAMEPADS; ++i) {
        if (m_gamepads[i].connected) ++count;
    }
    return count;
}

bool Input::is_gamepad_button_held(int index, GamepadButton btn) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    return m_gamepads[index].is_button_held(btn);
}

bool Input::is_gamepad_button_pressed(int index, GamepadButton btn) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    return m_gamepads[index].is_button_pressed(btn);
}

bool Input::is_gamepad_button_released(int index, GamepadButton btn) const {
    if (index < 0 || index >= MAX_GAMEPADS) return false;
    return m_gamepads[index].is_button_released(btn);
}

float Input::get_gamepad_axis(int index, GamepadAxis axis) const {
    if (index < 0 || index >= MAX_GAMEPADS) return 0.0f;
    return m_gamepads[index].get_axis(axis);
}

} // namespace engine::platform
