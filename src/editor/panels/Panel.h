#pragma once

#include <string>

namespace editor {

// Defines when a panel should be visible by default
enum class PanelVisibilityMode {
    EditorDefault,
    OnDemand,
    Manual
};

// Base class for all editor panels
// Each panel is a dockable ImGui window with standard lifecycle hooks
class Panel {
public:
    explicit Panel(const char* name, PanelVisibilityMode mode = PanelVisibilityMode::EditorDefault)
        : m_name(name), m_visibility_mode(mode) {}
    virtual ~Panel() = default;

    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    virtual void on_open() {}
    virtual void on_close() {}

    virtual void on_gui() = 0;

    virtual bool on_close_requested() { return true; }

    const char* name() const { return m_name.c_str(); }

    PanelVisibilityMode visibility_mode() const { return m_visibility_mode; }

    bool is_visible() const { return m_visible; }
    void set_visible(bool visible) {
        if (visible && !m_visible) {
            on_open();
        } else if (!visible && m_visible) {
            on_close();
        }
        m_visible = visible;
    }

    bool is_closable() const { return m_closable; }
    void set_closable(bool closable) { m_closable = closable; }

protected:
    std::string m_name;
    PanelVisibilityMode m_visibility_mode;
    bool m_visible = true;
    bool m_closable = true;
};

}