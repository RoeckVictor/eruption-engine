#include "ProjectManager.h"
#include "engine/core/Log.h"
#include "engine/platform/PlatformUtils.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace editor {

static std::string generate_guid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    auto hex = [&](int bytes) {
        std::ostringstream oss;
        uint32_t val = dist(gen);
        oss << std::hex << std::setfill('0') << std::setw(bytes * 2) << (val & ((1ull << (bytes * 8)) - 1));
        return oss.str();
    };

    return hex(4) + "-" + hex(2) + "-" + hex(2) + "-" + hex(2) + "-" + hex(4) + hex(2);
}

ProjectManager::ProjectManager() {
    load_recent_projects();
}

ProjectManager::~ProjectManager() {
    save_recent_projects();
}

bool ProjectManager::open_project(const std::string& path) {
    std::string project_file = get_project_file_path(path);

    if (!load_project_file(project_file)) {
        return false;
    }

    m_project_path = path;
    m_project_loaded = true;

    add_to_recent(path);

    return true;
}

void ProjectManager::close_project() {
    m_project_loaded = false;
    m_project_path.clear();
    m_project_info = ProjectInfo{};
}

bool ProjectManager::save_project() {
    if (!m_project_loaded) {
        return false;
    }
    return save_project_file();
}

bool ProjectManager::create_project(const std::string& path, const std::string& name) {
    try {
        fs::create_directories(path);
    } catch (const std::exception& e) {
        ENGINE_ERR("Failed to create project directory '%s': %s", path.c_str(), e.what());
        return false;
    }

    m_project_info = ProjectInfo{};
    m_project_info.name = name;
    m_project_info.version = "1.0.0";
    m_project_info.engine_version = "0.1.0";
    m_project_info.guid = generate_guid();
    m_project_info.default_scene = "Assets/Main.scene";
    m_project_info.script_assembly_name = "GameScripts";
    m_project_info.asset_paths = {"Assets"};

    m_project_path = path;

    create_project_directories(path);

    if (!save_project_file()) {
        return false;
    }

    m_project_loaded = true;
    add_to_recent(path);

    return true;
}

void ProjectManager::add_to_recent(const std::string& path) {
    // Make a copy because 'path' may be a reference into m_recent_projects
    // which gets invalidated by remove_from_recent
    std::string path_copy = path;

    remove_from_recent(path_copy);

    m_recent_projects.insert(m_recent_projects.begin(), path_copy);

    if (m_recent_projects.size() > MAX_RECENT_PROJECTS) {
        m_recent_projects.resize(MAX_RECENT_PROJECTS);
    }

    m_recent_projects_modified = true;
    save_recent_projects();
}

void ProjectManager::remove_from_recent(const std::string& path) {
    auto it = std::remove(m_recent_projects.begin(), m_recent_projects.end(), path);
    if (it != m_recent_projects.end()) {
        m_recent_projects.erase(it, m_recent_projects.end());
        m_recent_projects_modified = true;
    }
}

void ProjectManager::clear_recent() {
    m_recent_projects.clear();
    m_recent_projects_modified = true;
    save_recent_projects();
}

void ProjectManager::load_recent_projects() {
    std::string prefs_path = get_prefs_file_path();

    std::ifstream file(prefs_path);
    if (!file.is_open()) {
        return;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("recent_projects") && j["recent_projects"].is_array()) {
            m_recent_projects.clear();
            size_t total_count = 0;
            for (const auto& p : j["recent_projects"]) {
                if (p.is_string()) {
                    ++total_count;
                    std::string path = p.get<std::string>();
                    // Only add if the project still exists
                    if (is_valid_project(path)) {
                        m_recent_projects.push_back(path);
                    }
                }
            }
            // Mark as modified if we filtered out any invalid projects
            if (m_recent_projects.size() < total_count) {
                m_recent_projects_modified = true;
            }
        }
    } catch (const std::exception& e) {
        ENGINE_LOG_WARN("Failed to parse recent projects: %s", e.what());
    }
}

void ProjectManager::save_recent_projects() {
    if (!m_recent_projects_modified) {
        return;
    }

    std::string prefs_path = get_prefs_file_path();

    fs::path path(prefs_path);
    try {
        fs::create_directories(path.parent_path());
    } catch (const std::exception& e) {
        ENGINE_LOG_WARN("Failed to create preferences directory: %s", e.what());
        return;
    }

    nlohmann::json j;
    j["recent_projects"] = m_recent_projects;

    std::ofstream file(prefs_path);
    if (file.is_open()) {
        file << j.dump(2);
        m_recent_projects_modified = false;
    }
}

bool ProjectManager::is_valid_project(const std::string& path) {
    std::string project_file = get_project_file_path(path);
    return fs::exists(project_file);
}

std::string ProjectManager::get_project_file_path(const std::string& dir) {
    return (fs::path(dir) / PROJECT_FILE_NAME).string();
}

bool ProjectManager::load_project_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        m_project_info.name = j.value("name", "Untitled");
        m_project_info.version = j.value("version", "1.0.0");
        m_project_info.engine_version = j.value("engineVersion", "0.1.0");
        m_project_info.guid = j.value("guid", "");
        m_project_info.default_scene = j.value("defaultScene", "");
        m_project_info.script_assembly_name = j.value("scriptAssemblyName", "GameScripts");

        if (j.contains("assetPaths") && j["assetPaths"].is_array()) {
            m_project_info.asset_paths.clear();
            for (const auto& p : j["assetPaths"]) {
                if (p.is_string()) {
                    m_project_info.asset_paths.push_back(p.get<std::string>());
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        ENGINE_ERR("Failed to parse project file '%s': %s", path.c_str(), e.what());
        return false;
    }
}

bool ProjectManager::save_project_file() {
    std::string path = get_project_file_path(m_project_path);

    nlohmann::json j;
    j["name"] = m_project_info.name;
    j["version"] = m_project_info.version;
    j["engineVersion"] = m_project_info.engine_version;
    j["guid"] = m_project_info.guid;
    j["defaultScene"] = m_project_info.default_scene;
    j["scriptAssemblyName"] = m_project_info.script_assembly_name;
    j["assetPaths"] = m_project_info.asset_paths;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << j.dump(2);
    return true;
}

void ProjectManager::create_project_directories(const std::string& path) {
    try {
        fs::create_directories(fs::path(path) / "Assets");
        fs::create_directories(fs::path(path) / "Scripts" / "Components");
        fs::create_directories(fs::path(path) / "Scripts" / "Systems");
        fs::create_directories(fs::path(path) / "Scripts" / "Utils");
        fs::create_directories(fs::path(path) / "ProjectSettings");
        fs::create_directories(fs::path(path) / "Library");
        fs::create_directories(fs::path(path) / "Logs");
    } catch (const std::exception& e) {
        ENGINE_LOG_WARN("Failed to create some project directories: %s", e.what());
    }
}

std::string ProjectManager::get_prefs_file_path() const {
    std::string config_dir = engine::platform::user_config_directory();
    if (!config_dir.empty()) {
        return (fs::path(config_dir) / "eruption" / "editor_prefs.json").string();
    }
    return "editor_prefs.json";
}

}
