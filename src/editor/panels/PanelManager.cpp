#include "PanelManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>

namespace editor {

void PanelManager::init() {
    // Panels will be added by EditorApplication
}

void PanelManager::shutdown() {
    for (auto& panel : m_panels) {
        if (panel->is_visible()) {
            panel->on_close();
        }
    }
    m_panels.clear();
}

void PanelManager::render() {
    begin_dockspace();

    // Set up default layout on first frame if needed
    if (m_needs_layout_reset && m_first_frame) {
        setup_default_layout();
        m_needs_layout_reset = false;
    }
    m_first_frame = false;

    // Render each visible panel
    for (auto& panel : m_panels) {
        if (!panel->is_visible()) {
            continue;
        }

        bool open = true;
        ImGuiWindowFlags flags = ImGuiWindowFlags_None;

        if (ImGui::Begin(panel->name(), panel->is_closable() ? &open : nullptr, flags)) {
            panel->on_gui();
        }
        ImGui::End();

        if (!open) {
            panel->set_visible(false);
        }
    }

    end_dockspace();
}

void PanelManager::render_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project...", "Ctrl+Shift+N")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Open Project...", "Ctrl+Shift+O")) {
                // TODO: Implement
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                // TODO: Implement
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                // TODO: Signal exit
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                // TODO: Implement
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                // TODO: Implement
            }
            if (ImGui::MenuItem("Delete", "Delete")) {
                // TODO: Implement
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            for (auto& panel : m_panels) {
                bool visible = panel->is_visible();
                if (ImGui::MenuItem(panel->name(), nullptr, &visible)) {
                    panel->set_visible(visible);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                reset_layout();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Project")) {
            if (ImGui::MenuItem("Project Settings...")) {
                // Show Project Settings panel
                for (auto& panel : m_panels) {
                    if (std::string(panel->name()) == "Project Settings") {
                        panel->set_visible(true);
                        break;
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Scripts", "Ctrl+B")) {
                // Handled by EditorApplication shortcuts
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Settings...", "Ctrl+Shift+B")) {
                // Show Build Settings panel
                for (auto& panel : m_panels) {
                    if (std::string(panel->name()) == "Build Settings") {
                        panel->set_visible(true);
                        break;
                    }
                }
            }
            if (ImGui::MenuItem("Build Game")) {
                // Show Build Settings panel and start build
                for (auto& panel : m_panels) {
                    if (std::string(panel->name()) == "Build Settings") {
                        panel->set_visible(true);
                        break;
                    }
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Eruption Editor")) {
                m_show_about_dialog = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Render About dialog if open
    render_about_dialog();
}

void PanelManager::begin_dockspace() {
    // Create a fullscreen dockspace (offset by toolbar height)
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImVec2 dockspace_pos = viewport->WorkPos;
    ImVec2 dockspace_size = viewport->WorkSize;

    // Offset by toolbar height if set
    if (m_toolbar_height > 0.0f) {
        dockspace_pos.y += m_toolbar_height;
        dockspace_size.y -= m_toolbar_height;
    }

    ImGui::SetNextWindowPos(dockspace_pos);
    ImGui::SetNextWindowSize(dockspace_size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("EditorDockspaceWindow", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // Create the dockspace
    ImGuiID dockspace_id = ImGui::GetID("EditorDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
}

void PanelManager::end_dockspace() {
    ImGui::End();
}

void PanelManager::setup_default_layout() {
    ImGuiID dockspace_id = ImGui::GetID("EditorDockspace");

    // Clear existing layout
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    // Split into left (20%), center, and right (25%)
    ImGuiID dock_left, dock_center, dock_right;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, &dock_left, &dock_center);
    ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.25f, &dock_right, &dock_center);

    // Split center bottom for file browser and console (25% height)
    ImGuiID dock_bottom;
    ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_center);

    // Split bottom into left (file browser) and right (console)
    ImGuiID dock_bottom_left, dock_bottom_right;
    ImGui::DockBuilderSplitNode(dock_bottom, ImGuiDir_Left, 0.5f, &dock_bottom_left, &dock_bottom_right);

    // Dock the panels
    ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow("Scene Manager", dock_left);
    ImGui::DockBuilderDockWindow("Viewport", dock_center);
    ImGui::DockBuilderDockWindow("Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Asset Preview", dock_right);  // Tabbed with Inspector
    ImGui::DockBuilderDockWindow("File Browser", dock_bottom_left);
    ImGui::DockBuilderDockWindow("Console", dock_bottom_right);

    ImGui::DockBuilderFinish(dockspace_id);
}

void PanelManager::reset_layout() {
    m_needs_layout_reset = true;
    m_first_frame = true;
}

void PanelManager::save_layout(const std::string& path) {
    m_layout_path = path;
    ImGui::SaveIniSettingsToDisk(path.c_str());
}

void PanelManager::load_layout(const std::string& path) {
    m_layout_path = path;

    // Check if file exists
    std::ifstream file(path);
    if (file.good()) {
        ImGui::LoadIniSettingsFromDisk(path.c_str());
        m_needs_layout_reset = false;
    } else {
        // File doesn't exist, will use default layout
        m_needs_layout_reset = true;
    }
}

void PanelManager::render_about_dialog() {
    if (!m_show_about_dialog) {
        return;
    }

    ImGui::OpenPopup("About Eruption Editor");

    // Center the modal
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("About Eruption Editor", &m_show_about_dialog, ImGuiWindowFlags_NoResize)) {
        // Logo area (placeholder)
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("ERUPTION").x) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f), "ERUPTION");
        ImGui::PopFont();

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Falling Sand Physics Editor").x) * 0.5f);
        ImGui::TextDisabled("Falling Sand Physics Editor");

        ImGui::Separator();
        ImGui::Spacing();

        // Version info
        ImGui::Text("Version: 0.1.0 (Development)");
        ImGui::Text("Build: Debug");
        ImGui::Spacing();

        // Description
        ImGui::TextWrapped(
            "Eruption is a 2D falling sand physics game engine with a "
            "Unity-like editor for creating games with particle simulation."
        );
        ImGui::Spacing();

        ImGui::Separator();
        ImGui::Spacing();

        // Credits
        ImGui::Text("Built with:");
        ImGui::BulletText("EnTT - Entity Component System");
        ImGui::BulletText("Dear ImGui - Immediate Mode GUI");
        ImGui::BulletText("GLFW - Window & Input");
        ImGui::BulletText("OpenGL - Graphics");
        ImGui::BulletText("Box2D - Physics");
        ImGui::BulletText("nlohmann/json - JSON parsing");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Close button
        float button_width = 120.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
        if (ImGui::Button("Close", ImVec2(button_width, 0))) {
            m_show_about_dialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

} // namespace editor
