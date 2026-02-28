#pragma once

#include "engine/simulation/MaterialDefinition.h"
#include <nlohmann/json_fwd.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace engine::simulation {

class CategoryLibrary;

class MaterialLibrary {
public:
    MaterialLibrary() = default;

    bool load_from_file(const std::string& path);
    bool load_from_directory(const std::string& dir_path);

    bool load_material_file(const std::string& path);

    bool load_from_json(const std::string& json_str);

    const MaterialDefinition* get_material(uint8_t id) const {
        if (id >= m_materials.size()) return nullptr;
        return &m_materials[id];
    }

    const MaterialDefinition* get_material(const std::string& internal_name) const {
        auto it = m_name_to_id.find(internal_name);
        if (it == m_name_to_id.end()) return nullptr;
        return &m_materials[it->second];
    }

    const std::vector<MaterialDefinition>& get_all_materials() const {
        return m_materials;
    }

    uint32_t get_color(uint8_t id) const {
        if (id >= m_materials.size()) return 0xFFFFFFFF;
        return m_materials[id].color;
    }

    size_t material_count() const {
        return m_materials.size();
    }

    const std::string& name() const { return m_name; }
    const std::string& version() const { return m_version; }


    std::vector<MaterialSlot> build_material_slots() const;
    std::vector<uint32_t> build_color_palette() const;

    void clear();

    void set_category_library(CategoryLibrary* cat_lib) { m_category_library = cat_lib; }

    void resolve_interaction_references();

    bool compile_for_gpu();

    const std::vector<uint32_t>& get_material_table() const { return m_compiled_materials; }
    const std::vector<uint32_t>& get_interaction_table() const { return m_compiled_interactions; }

    bool is_compiled() const { return m_is_compiled; }

    const std::unordered_map<std::string, uint8_t>& get_name_to_id() const { return m_name_to_id; }

private:
    std::string m_name;
    std::string m_version;
    std::vector<MaterialDefinition> m_materials;
    std::unordered_map<std::string, uint8_t> m_name_to_id;

    std::vector<uint32_t> m_compiled_materials;
    std::vector<uint32_t> m_compiled_interactions;
    bool m_is_compiled = false;

    bool parse_json(const std::string& json_str);
    bool parse_material_v2(const nlohmann::json& j, MaterialDefinition& def);
    void parse_interactions(const nlohmann::json& arr, MaterialDefinition& def);

    CategoryLibrary* m_category_library = nullptr;
};


class MaterialLibraryRegistry {
public:
    static MaterialLibraryRegistry& instance() {
        static MaterialLibraryRegistry s_instance;
        return s_instance;
    }

    bool load_library(const std::string& name, const std::string& path);

    MaterialLibrary* get_library(const std::string& name);
    const MaterialLibrary* get_library(const std::string& name) const;

    MaterialLibrary* get_or_create_library(const std::string& name);

    void remove_library(const std::string& name);

    std::vector<std::string> get_library_names() const;

private:
    MaterialLibraryRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<MaterialLibrary>> m_libraries;
};

}
