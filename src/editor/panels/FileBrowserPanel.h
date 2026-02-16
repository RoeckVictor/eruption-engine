#pragma once

#include "Panel.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <functional>

namespace editor {

class EditorContext;

/// File browser panel for navigating project assets.
class FileBrowserPanel : public Panel {
public:
    FileBrowserPanel();

    void on_gui() override;

    /// Set the root directory to browse.
    void set_root(const std::string& path);

    /// Set callback for when a file is selected (single click).
    using FileSelectedCallback = std::function<void(const std::string&)>;
    void set_file_selected_callback(FileSelectedCallback callback) { m_file_selected_callback = std::move(callback); }

    /// Set callback for when a file is opened (double-click or right-click > Open).
    using FileOpenedCallback = std::function<void(const std::string&)>;
    void set_file_opened_callback(FileOpenedCallback callback) { m_file_opened_callback = std::move(callback); }

    /// Get the currently selected file path (empty if none).
    const std::string& selected_file() const { return m_selected_file; }

    /// Set the editor context for entity drag-drop support.
    void set_editor_context(EditorContext* context) { m_editor_context = context; }

    /// Refresh the file list.
    void refresh();

private:
    struct FileEntry {
        std::string name;
        std::string path;
        bool is_directory;
        // TODO: Add icon, size, date, etc.
    };

    void render_toolbar();
    void render_folder_tree();
    void render_file_list();

    void navigate_to(const std::string& path);
    void select_file(const std::string& path);

    void perform_delete(const std::string& path);
    void perform_rename(const std::string& old_path, const std::string& new_name);
    void perform_paste(const std::string& dest_dir);
    void handle_keyboard_shortcuts();

    std::string m_root_path;
    std::string m_current_path;
    std::string m_selected_file;
    std::vector<FileEntry> m_entries;

    FileSelectedCallback m_file_selected_callback;
    FileOpenedCallback m_file_opened_callback;

    char m_filter[128] = "";
    bool m_show_hidden = false;

    // Clipboard
    std::string m_clipboard_path;
    bool m_clipboard_is_cut = false;

    // Inline rename
    std::string m_rename_target;
    char m_rename_buffer[256] = {};
    bool m_rename_focus_set = false;

    // Inline create
    bool m_creating_folder = false;
    bool m_creating_scene = false;
    bool m_creating_pxg = false;
    bool m_creating_prefab = false;
    bool m_creating_script = false;
    char m_create_buffer[256] = {};
    bool m_create_focus_set = false;

    // Delete confirmation
    std::string m_pending_delete_path;
    int m_pending_delete_prefab_usage = 0;  // Count of entities using this prefab

    // Editor context for entity drag-drop
    EditorContext* m_editor_context = nullptr;

    // Helper to count and unlink prefab instances
    int count_prefab_instances(const std::string& prefab_path);
    void unlink_prefab_instances(const std::string& prefab_path);
};

} // namespace editor
