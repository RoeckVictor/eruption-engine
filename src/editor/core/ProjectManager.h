#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace editor {

struct ProjectInfo {
    std::string name;
    std::string version;
    std::string engine_version;
    std::string guid;
    std::string default_scene;
    std::string script_assembly_name;
    std::vector<std::string> asset_paths;
};

class ProjectManager {
public:
    ProjectManager();
    ~ProjectManager();

    bool has_project() const { return m_project_loaded; }

    const std::string& project_path() const { return m_project_path; }
    const ProjectInfo& project_info() const { return m_project_info; }
    ProjectInfo& project_info() { return m_project_info; }

    bool save_project();
    bool open_project(const std::string& path);
    void close_project();
    bool create_project(const std::string& path, const std::string& name);

    const std::vector<std::string>& recent_projects() const { return m_recent_projects; }
    void add_to_recent(const std::string& path);
    void remove_from_recent(const std::string& path);
    void clear_recent();
    void load_recent_projects();
    void save_recent_projects();

    static bool is_valid_project(const std::string& path);

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

}