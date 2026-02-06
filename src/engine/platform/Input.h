#pragma once

#include "engine/platform/KeyCode.h"

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

    double mouse_x() const { return m_mouse_x; }
    double mouse_y() const { return m_mouse_y; }
    float scroll_y() const { return m_scroll_y; }

private:
    static constexpr int KEY_COUNT = static_cast<int>(KeyCode::COUNT);
    static constexpr int MOUSE_COUNT = static_cast<int>(MouseButton::COUNT);

    bool m_keys[KEY_COUNT] = {};
    bool m_keys_prev[KEY_COUNT] = {};
    double m_mouse_x = 0.0;
    double m_mouse_y = 0.0;
    bool m_mouse[MOUSE_COUNT] = {};
    bool m_mouse_prev[MOUSE_COUNT] = {};
    float m_scroll_y = 0.0f;
};

} // namespace engine::platform
