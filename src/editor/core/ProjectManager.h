#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace editor {

/// Information about a project.
struct ProjectInfo {
    std::string name;
    std::string version;
    std::string engine_version;
    std::string guid;
    std::string default_scene;
    std::string script_assembly_name;
    std::vector<std::string> asset_paths;
};

/// Manages project loading, saving, and recent projects list.
class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();

    /// Check if a project is currently loaded.
    bool has_project() const { return m_project_loaded; }

    /// Get the current project path (empty if no project).
    const std::string& project_path() const { return m_project_path; }

    /// Get the current project info.
    const ProjectInfo& project_info() const { return m_project_info; }
    ProjectInfo& project_info() { return m_project_info; }

    /// Save the current project settings.
    bool save_project();

    /// Open a project from the given directory path.
    /// Returns true if successful.
    bool open_project(const std::string& path);

    /// Close the current project.
    void close_project();

    /// Create a new project at the given path.
    /// Returns true if successful.
    bool create_project(const std::string& path, const std::string& name);

    /// Get list of recent projects (paths).
    const std::vector<std::string>& recent_projects() const { return m_recent_projects; }

    /// Add a path to recent projects.
    void add_to_recent(const std::string& path);

    /// Remove a path from recent projects.
    void remove_from_recent(const std::string& path);

    /// Clear all recent projects.
    void clear_recent();

    /// Load recent projects from user preferences file.
    void load_recent_projects();

    /// Save recent projects to user preferences file.
    void save_recent_projects();

    /// Validate that a directory contains a valid project.
    static bool is_valid_project(const std::string& path);

    /// Get the project file path for a directory.
    static std::string get_project_file_path(const std::string& dir);

private:
    bool load_project_file(const std::string& path);
    bool save_project_file();
    void create_project_directories(const std::string& path);
    std::string get_prefs_file_path() const;

    bool m_project_loaded = false;
    std::string m_project_path;
    ProjectInfo m_project_info;
    std::vector<std::string> m_recent_projects;

    static constexpr const char* PROJECT_FILE_NAME = "project.eruption";
    static constexpr size_t MAX_RECENT_PROJECTS = 10;
};

} // namespace editor
