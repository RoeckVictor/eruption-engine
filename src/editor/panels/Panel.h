#pragma once

#include <string>

namespace editor {

/// Base class for all editor panels.
/// Each panel is a dockable ImGui window with standard lifecycle hooks.
class Panel {
public:
    explicit Panel(const char* name) : m_name(name) {}
    virtual ~Panel() = default;

    Panel(const Panel&) = delete;
    Panel& operator=(const Panel&) = delete;

    /// Called once when the panel is first opened.
    virtual void on_open() {}

    /// Called once when the panel is closed.
    virtual void on_close() {}

    /// Called every frame to render the panel's ImGui content.
    /// Implementation should NOT call ImGui::Begin/End - that's handled by PanelManager.
    virtual void on_gui() = 0;

    /// Called when the user attempts to close the panel (e.g., clicking X).
    /// Return true to allow closing, false to prevent it (e.g., to show an unsaved changes dialog).
    virtual bool on_close_requested() { return true; }

    /// Get the panel's display name (used as window title).
    const char* name() const { return m_name.c_str(); }

    /// Visibility state.
    bool is_visible() const { return m_visible; }
    void set_visible(bool visible) {
        if (visible && !m_visible) {
            on_open();
        } else if (!visible && m_visible) {
            on_close();
        }
        m_visible = visible;
    }

    /// Whether this panel can be closed by the user.
    bool is_closable() const { return m_closable; }
    void set_closable(bool closable) { m_closable = closable; }

protected:
    std::string m_name;
    bool m_visible = true;
    bool m_closable = true;
};

} // namespace editor
