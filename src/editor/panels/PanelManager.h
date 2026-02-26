#pragma once

#include "Panel.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace editor {

// Manages all editor panels and the ImGui docking layout
class PanelManager {
public:
    PanelManager() = default;
    ~PanelManager() = default;

    PanelManager(const PanelManager&) = delete;
    PanelManager& operator=(const PanelManager&) = delete;

    void init();

    void shutdown();

    template<typename T, typename... Args>
    T* add_panel(Args&&... args) {
        auto panel = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = panel.get();
        m_panels.push_back(std::move(panel));
        return ptr;
    }

    template<typename T>
    T* get_panel() {
        for (auto& panel : m_panels) {
            if (auto* p = dynamic_cast<T*>(panel.get())) {
                return p;
            }
        }
        return nullptr;
    }

    void render();

    void render_menu_bar();

    void setup_default_layout();
    void reset_layout();
    void save_layout(const std::string& path);
    void load_layout(const std::string& path);
    bool needs_layout_reset() const { return m_needs_layout_reset; }

    void set_toolbar_height(float height) { m_toolbar_height = height; }
    float toolbar_height() const { return m_toolbar_height; }

    void show_about_dialog() { m_show_about_dialog = true; }

    void show_editor_panels();
    void hide_editor_panels();
    struct MenuCallbacks {
        std::function<void()> new_scene;
        std::function<void()> save_scene;
        std::function<void()> save_scene_as;
        std::function<void()> show_project_hub;
        std::function<void()> exit;
        std::function<void()> undo;
        std::function<void()> redo;
        std::function<void()> cut;
        std::function<void()> copy;
        std::function<void()> paste;
        std::function<void()> duplicate;
        std::function<void()> delete_selected;
        std::function<void()> reset_layout;
    };
    MenuCallbacks menu_callbacks;

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

}