#include "engine/save/SaveSystem.h"
#include "engine/core/Log.h"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace engine::save {

// Platform-specific user data directory
static std::string get_user_save_base() {
#ifdef _WIN32
    const char* appdata = std::getenv("LOCALAPPDATA");
    if (appdata) return std::string(appdata);
    return ".";
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.local/share";
    return ".";
#endif
}

void SaveSystem::init(const std::string& project_name) {
    m_project_name = project_name;
    m_base_path = get_user_save_base() + "/" + project_name + "/";

    ensure_directory(m_base_path);
    ensure_directory(m_base_path + "saves/");

    load_prefs();
    ENGINE_LOG("SaveSystem: Initialized at '%s'", m_base_path.c_str());
}

void SaveSystem::shutdown() {
    save_prefs();
}

bool SaveSystem::save_game_simple(const std::string& slot, entt::registry& /*registry*/) {
    std::string dir = slot_path(slot);
    ensure_directory(dir);

    // The actual entity serialization is delegated to RuntimeContext which owns
    // the SceneSerializer and ComponentTypeRegistry (editor-layer code).
    // This method creates the save slot directory and stores a marker file.
    // RuntimeContext::host_save_game calls this, then writes entity data.

    nlohmann::json root;
    root["version"] = 1;
    root["slot"] = slot;
    root["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

    std::string file_path = dir + "save.json";
    std::ofstream file(file_path);
    if (!file.is_open()) {
        ENGINE_ERR("SaveSystem: Cannot write '%s'", file_path.c_str());
        return false;
    }
    file << root.dump(2);
    ENGINE_LOG("SaveSystem: Game saved to slot '%s'", slot.c_str());
    return true;
}

bool SaveSystem::load_game_simple(const std::string& slot, entt::registry& /*registry*/) {
    std::string file_path = slot_path(slot) + "save.json";
    if (!fs::exists(file_path)) {
        ENGINE_ERR("SaveSystem: Save file not found: '%s'", file_path.c_str());
        return false;
    }

    // Full deserialization will be implemented by RuntimeContext which
    // owns SceneSerializer and can properly rebuild the ECS registry.
    ENGINE_LOG("SaveSystem: Load requested for slot '%s'", slot.c_str());
    return true;
}

bool SaveSystem::has_save(const std::string& slot) const {
    return fs::exists(slot_path(slot) + "save.json");
}

bool SaveSystem::delete_save(const std::string& slot) {
    std::string dir = slot_path(slot);
    if (!fs::exists(dir)) return false;

    std::error_code ec;
    fs::remove_all(dir, ec);
    if (ec) {
        ENGINE_ERR("SaveSystem: Failed to delete slot '%s': %s", slot.c_str(), ec.message().c_str());
        return false;
    }
    ENGINE_LOG("SaveSystem: Deleted save slot '%s'", slot.c_str());
    return true;
}

std::vector<std::string> SaveSystem::list_saves() const {
    std::vector<std::string> result;
    std::string saves_dir = m_base_path + "saves/";
    if (!fs::exists(saves_dir)) return result;

    for (auto& entry : fs::directory_iterator(saves_dir)) {
        if (entry.is_directory()) {
            result.push_back(entry.path().filename().string());
        }
    }
    return result;
}

// ---- Player Preferences ----

void SaveSystem::set_int(const std::string& key, int value) { m_prefs[key] = value; }

int SaveSystem::get_int(const std::string& key, int default_val) const {
    auto it = m_prefs.find(key);
    return (it != m_prefs.end() && it->is_number_integer()) ? it->get<int>() : default_val;
}

void SaveSystem::set_float(const std::string& key, float value) { m_prefs[key] = value; }

float SaveSystem::get_float(const std::string& key, float default_val) const {
    auto it = m_prefs.find(key);
    return (it != m_prefs.end() && it->is_number()) ? it->get<float>() : default_val;
}

void SaveSystem::set_string(const std::string& key, const std::string& value) { m_prefs[key] = value; }

std::string SaveSystem::get_string(const std::string& key, const std::string& default_val) const {
    auto it = m_prefs.find(key);
    return (it != m_prefs.end() && it->is_string()) ? it->get<std::string>() : default_val;
}

void SaveSystem::save_prefs() {
    std::string path = prefs_path();
    ensure_directory(m_base_path);
    std::ofstream file(path);
    if (file.is_open()) {
        file << m_prefs.dump(2);
    }
}

void SaveSystem::load_prefs() {
    std::string path = prefs_path();
    std::ifstream file(path);
    if (!file.is_open()) {
        m_prefs = nlohmann::json::object();
        return;
    }
    try {
        file >> m_prefs;
    } catch (...) {
        m_prefs = nlohmann::json::object();
    }
}

// ---- Helpers ----

std::string SaveSystem::slot_path(const std::string& slot) const {
    return m_base_path + "saves/" + slot + "/";
}

std::string SaveSystem::prefs_path() const {
    return m_base_path + "prefs.json";
}

bool SaveSystem::ensure_directory(const std::string& path) const {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
}

} // namespace engine::save
