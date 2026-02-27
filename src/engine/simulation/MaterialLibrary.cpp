#include "MaterialLibrary.h"
#include "engine/core/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace engine::simulation {

bool MaterialLibrary::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().error("MaterialLibrary", "Failed to open file: %s", path.c_str());
        return false;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return load_from_json(json_str);
}

bool MaterialLibrary::load_from_json(const std::string& json_str) {
    return parse_json(json_str);
}

bool MaterialLibrary::parse_json(const std::string& json_str) {
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);

        // Read metadata
        if (j.contains("name")) {
            m_name = j["name"].get<std::string>();
        }
        if (j.contains("version")) {
            m_version = j["version"].get<std::string>();
        }

        // Parse materials array
        if (!j.contains("materials") || !j["materials"].is_array()) {
            Logger::instance().error("MaterialLibrary", "JSON missing 'materials' array");
            return false;
        }

        const auto& materials_array = j["materials"];
        m_materials.clear();
        m_materials.resize(256);
        m_name_to_id.clear();

        for (const auto& mat_json : materials_array) {
            MaterialDefinition def;

            // Required fields
            if (!mat_json.contains("id")) {
                Logger::instance().warning("MaterialLibrary", "Material missing 'id', skipping");
                continue;
            }
            def.id = mat_json["id"].get<uint8_t>();

            if (!mat_json.contains("internal_name")) {
                Logger::instance().warning("MaterialLibrary", "Material %d missing 'internal_name', skipping", def.id);
                continue;
            }
            def.internal_name = mat_json["internal_name"].get<std::string>();

            // Optional fields with defaults
            def.name = mat_json.value("name", def.internal_name);
            def.density = mat_json.value<uint8_t>("density", 0);

            // Category (string -> enum)
            std::string cat_str = mat_json.value("category", "empty");
            def.category = string_to_category(cat_str);

            // Thermal properties
            def.melt_point = mat_json.value<uint8_t>("melt_point", 0);
            def.boil_point = mat_json.value<uint8_t>("boil_point", 0);
            def.default_temp = mat_json.value<uint8_t>("default_temp", 128);

            // Phase transition targets (can be IDs or names)
            if (mat_json.contains("melt_into")) {
                if (mat_json["melt_into"].is_number()) {
                    def.melt_into = mat_json["melt_into"].get<uint8_t>();
                } else if (mat_json["melt_into"].is_string()) {
                    // Name-based resolution handled in second pass below
                }
            }

            if (mat_json.contains("boil_into")) {
                if (mat_json["boil_into"].is_number()) {
                    def.boil_into = mat_json["boil_into"].get<uint8_t>();
                } else if (mat_json["boil_into"].is_string()) {
                    // Name-based resolution handled in second pass below
                }
            }

            // Flags
            def.flags = 0;
            if (mat_json.contains("flags")) {
                if (mat_json["flags"].is_array()) {
                    for (const auto& flag_str : mat_json["flags"]) {
                        std::string flag = flag_str.get<std::string>();
                        if (flag == "hazard") def.flags |= MAT_FLAG_HAZARD;
                        else if (flag == "flammable") def.flags |= MAT_FLAG_FLAMMABLE;
                        else if (flag == "conductive") def.flags |= MAT_FLAG_CONDUCTIVE;
                    }
                }
            }

            // Color (parse hex string like "#RRGGBBAA")
            if (mat_json.contains("color")) {
                std::string color_str = mat_json["color"].get<std::string>();
                if (color_str[0] == '#' && color_str.size() == 9) {
                    def.color = static_cast<uint32_t>(std::stoul(color_str.substr(1), nullptr, 16));
                } else {
                    Logger::instance().warning("MaterialLibrary", "Invalid color format for material %d: %s",
                                          def.id, color_str.c_str());
                    def.color = 0xFFFFFFFF;
                }
            }

            // Store material
            m_materials[def.id] = def;
            m_name_to_id[def.internal_name] = def.id;
        }

        // Second pass: resolve name-based phase transitions
        for (const auto& mat_json : materials_array) {
            if (!mat_json.contains("id")) continue;
            uint8_t id = mat_json["id"].get<uint8_t>();
            auto& def = m_materials[id];

            if (mat_json.contains("melt_into") && mat_json["melt_into"].is_string()) {
                std::string target = mat_json["melt_into"].get<std::string>();
                auto it = m_name_to_id.find(target);
                if (it != m_name_to_id.end()) {
                    def.melt_into = it->second;
                } else {
                    Logger::instance().warning("MaterialLibrary", "Material '%s' melt_into references unknown material '%s'",
                                          def.internal_name.c_str(), target.c_str());
                }
            }

            if (mat_json.contains("boil_into") && mat_json["boil_into"].is_string()) {
                std::string target = mat_json["boil_into"].get<std::string>();
                auto it = m_name_to_id.find(target);
                if (it != m_name_to_id.end()) {
                    def.boil_into = it->second;
                } else {
                    Logger::instance().warning("MaterialLibrary", "Material '%s' boil_into references unknown material '%s'",
                                          def.internal_name.c_str(), target.c_str());
                }
            }
        }

        Logger::instance().info("MaterialLibrary", "Loaded material library '%s' with %zu materials",
                              m_name.c_str(), m_name_to_id.size());
        return true;

    } catch (const nlohmann::json::exception& e) {
        Logger::instance().error("MaterialLibrary", "JSON parse error: %s", e.what());
        return false;
    }
}

std::vector<MaterialSlot> MaterialLibrary::build_material_slots() const {
    std::vector<MaterialSlot> slots(256);
    for (size_t i = 0; i < m_materials.size(); i++) {
        slots[i] = m_materials[i].to_material_slot();
    }
    return slots;
}

std::vector<uint32_t> MaterialLibrary::build_color_palette() const {
    std::vector<uint32_t> palette(256, 0xFFFFFFFF);
    for (size_t i = 0; i < m_materials.size(); i++) {
        palette[i] = m_materials[i].color;
    }
    // Material ID 0 (air/empty) must always be fully transparent
    palette[0] = 0x00000000;
    return palette;
}

void MaterialLibrary::clear() {
    m_name.clear();
    m_version.clear();
    m_materials.clear();
    m_name_to_id.clear();
}

bool MaterialLibraryRegistry::load_library(const std::string& name, const std::string& path) {
    auto library = std::make_unique<MaterialLibrary>();
    if (!library->load_from_file(path)) {
        return false;
    }

    m_libraries[name] = std::move(library);
    return true;
}

MaterialLibrary* MaterialLibraryRegistry::get_library(const std::string& name) {
    auto it = m_libraries.find(name);
    if (it == m_libraries.end()) return nullptr;
    return it->second.get();
}

const MaterialLibrary* MaterialLibraryRegistry::get_library(const std::string& name) const {
    auto it = m_libraries.find(name);
    if (it == m_libraries.end()) return nullptr;
    return it->second.get();
}

MaterialLibrary* MaterialLibraryRegistry::get_or_create_library(const std::string& name) {
    auto it = m_libraries.find(name);
    if (it != m_libraries.end()) {
        return it->second.get();
    }

    auto library = std::make_unique<MaterialLibrary>();
    auto* ptr = library.get();
    m_libraries[name] = std::move(library);
    return ptr;
}

void MaterialLibraryRegistry::remove_library(const std::string& name) {
    m_libraries.erase(name);
}

std::vector<std::string> MaterialLibraryRegistry::get_library_names() const {
    std::vector<std::string> names;
    names.reserve(m_libraries.size());
    for (const auto& [name, _] : m_libraries) {
        names.push_back(name);
    }
    return names;
}

} // namespace engine::simulation
