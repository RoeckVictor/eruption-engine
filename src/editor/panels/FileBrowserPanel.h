#pragma once

#include "Panel.h"
#include <string>
#include <vector>
#include <functional>

namespace editor {

/// File browser panel for navigating project assets.
class FileBrowserPanel : public Panel {
public:
    FileBrowserPanel();

    void on_gui() override;

    /// Set the root directory to browse.
    void set_root(const std::string& path);

    /// Set callback for when a file is selected.
    using FileSelectedCallback = std::function<void(const std::string&)>;
    void set_file_selected_callback(FileSelectedCallback callback) { m_file_selected_callback = std::move(callback); }

    /// Get the currently selected file path (empty if none).
    const std::string& selected_file() const { return m_selected_file; }

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
    void refresh();

    void navigate_to(const std::string& path);
    void select_file(const std::string& path);

    std::string m_root_path;
    std::string m_current_path;
    std::string m_selected_file;
    std::vector<FileEntry> m_entries;

    FileSelectedCallback m_file_selected_callback;

    char m_filter[128] = "";
    bool m_show_hidden = false;
};

} // namespace editor
