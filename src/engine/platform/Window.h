#pragma once

#include "engine/platform/KeyCode.h"
#include <vector>

struct GLFWwindow;

namespace engine::platform {

class Input;

enum class GraphicsAPI {
    OpenGL,
    Vulkan,
    None
};

class Window {
public:
    Window() = default;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    static bool init_platform();
    static void shutdown_platform();

    bool init(const char* title, int width, int height, GraphicsAPI api = GraphicsAPI::OpenGL);
    void shutdown();
    bool should_close() const;
    void set_should_close(bool close);
    void swap_buffers();
    void poll_events();
    void set_vsync(bool enabled);

    int width() const { return m_width; }
    int height() const { return m_height; }

    void get_position(int& x, int& y) const;

    GLFWwindow* glfw_handle() const;

    static void* get_gl_proc_address(const char* name);

    // Vulkan support: query required instance extensions and create a surface.
    // Uses void* to avoid pulling Vulkan headers into platform code.
    static std::vector<const char*> get_required_vulkan_extensions();
    void* create_vulkan_surface(void* vk_instance);

    GraphicsAPI graphics_api() const { return m_api; }

    void on_framebuffer_resize(int width, int height);
    void on_scroll(float y_offset);
    void on_focus(bool focused);

    bool focus_just_gained();

private:
    friend class Input;

    float consume_scroll();

    bool is_key_down(KeyCode key) const;
    bool is_mouse_button_down(MouseButton button) const;
    void get_cursor_position(double& x, double& y) const;

    void* m_handle = nullptr;
    GraphicsAPI m_api = GraphicsAPI::None;
    int m_width = 0;
    int m_height = 0;
    float m_scroll_accum = 0.0f;
    bool m_focus_gained = false;
};

}
