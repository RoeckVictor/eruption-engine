#pragma once

#include "engine/save/SaveData.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>

namespace engine::save {

// Runtime game save/load system
// Handles save slots, player preferences, and game state persistence
// Registered as a subsystem via SubsystemRegistry
class SaveSystem {
public:
    void init(const std::string& project_name);
    void shutdown();

    bool save_game_simple(const std::string& slot, entt::registry& registry);
    bool load_game_simple(const std::string& slot, entt::registry& registry);

    bool has_save(const std::string& slot) const;
    bool delete_save(const std::string& slot);
    std::vector<std::string> list_saves() const;

    void set_int(const std::string& key, int value);
    int get_int(const std::string& key, int default_val = 0) const;

    void set_float(const std::string& key, float value);
    float get_float(const std::string& key, float default_val = 0.0f) const;

    void set_string(const std::string& key, const std::string& value);
    std::string get_string(const std::string& key, const std::string& default_val = "") const;

    void save_prefs();
    void load_prefs();

    const std::string& base_path() const { return m_base_path; }

private:
    std::string slot_path(const std::string& slot) const;
    std::string prefs_path() const;
    bool ensure_directory(const std::string& path) const;

    std::string m_base_path;
    std::string m_project_name;
    nlohmann::json m_prefs;
};

}
