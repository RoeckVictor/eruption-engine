#pragma once

#include "Panel.h"
#include <memory>
#include <vector>
#include <string>

namespace editor {

/// Manages all editor panels and the ImGui docking layout.
class PanelManager {
public:
    PanelManager() = default;
    ~PanelManager() = default;

    PanelManager(const PanelManager&) = delete;
    PanelManager& operator=(const PanelManager&) = delete;

    /// Initialize the panel manager.
    void init();

    /// Shutdown and clean up all panels.
    void shutdown();

    /// Register a panel. Takes ownership.
    template<typename T, typename... Args>
    T* add_panel(Args&&... args) {
        auto panel = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = panel.get();
        m_panels.push_back(std::move(panel));
        return ptr;
    }

    /// Get a panel by type.
    template<typename T>
    T* get_panel() {
        for (auto& panel : m_panels) {
            if (auto* p = dynamic_cast<T*>(panel.get())) {
                return p;
            }
        }
        return nullptr;
    }

    /// Render all visible panels.
    void render();

    /// Render the main menu bar.
    void render_menu_bar();

    /// Set up the default docking layout (called on first run or reset).
    void setup_default_layout();

    /// Reset layout to default.
    void reset_layout();

    /// Save the current layout to a file.
    void save_layout(const std::string& path);

    /// Load layout from a file.
    void load_layout(const std::string& path);

    /// Check if we need to set up the default layout on next frame.
    bool needs_layout_reset() const { return m_needs_layout_reset; }

    /// Set the toolbar height (dockspace will be offset by this amount).
    void set_toolbar_height(float height) { m_toolbar_height = height; }
    float toolbar_height() const { return m_toolbar_height; }

    /// Show the About dialog.
    void show_about_dialog() { m_show_about_dialog = true; }

private:
    void begin_dockspace();
    void end_dockspace();
    void render_about_dialog();

    std::vector<std::unique_ptr<Panel>> m_panels;
    bool m_needs_layout_reset = true;
    bool m_first_frame = true;
    bool m_show_about_dialog = false;
    std::string m_layout_path;
    float m_toolbar_height = 0.0f;
};

} // namespace editor
