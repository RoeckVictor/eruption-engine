#include "MaterialLibrary.h"
#include "engine/simulation/CategoryLibrary.h"
#include "engine/simulation/InteractionCompiler.h"
#include "engine/core/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

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

bool MaterialLibrary::load_from_directory(const std::string& dir_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        Logger::instance().error("MaterialLibrary", "Directory not found: %s", dir_path.c_str());
        return false;
    }

    // Initialize materials array
    m_materials.clear();
    m_materials.resize(256);
    m_name_to_id.clear();
    m_name = "Materials from " + dir_path;
    m_version = "2.0";

    int loaded_count = 0;
    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".material") {
            if (load_material_file(entry.path().string())) {
                loaded_count++;
            }
        }
    }

    if (loaded_count == 0) {
        Logger::instance().warning("MaterialLibrary", "No .material files found in: %s", dir_path.c_str());
        return false;
    }

    // Resolve material name references in interactions
    resolve_interaction_references();

    Logger::instance().info("MaterialLibrary", "Loaded %d materials from directory: %s",
                          loaded_count, dir_path.c_str());
    return true;
}

bool MaterialLibrary::load_material_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        Logger::instance().error("MaterialLibrary", "Failed to open material file: %s", path.c_str());
        return false;
    }

    std::string json_str((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    try {
        nlohmann::json j = nlohmann::json::parse(json_str);

        // Check schema version
        int schema_version = j.value("schema_version", 1);

        MaterialDefinition def;

        if (schema_version >= 2) {
            // New v2 schema with physical/thermal/rendering sections
            if (!parse_material_v2(j, def)) {
                return false;
            }
        } else {
            // Legacy v1 schema (flat structure)
            if (!j.contains("id")) {
                Logger::instance().warning("MaterialLibrary", "Material missing 'id' in: %s", path.c_str());
                return false;
            }
            def.id = j["id"].get<uint8_t>();
            def.internal_name = j.value("internal_name", "unknown");
            def.name = j.value("name", def.internal_name);
            def.density = j.value<uint8_t>("density", 0);

            // Resolve category name via CategoryLibrary
            std::string cat_name = j.value("category", "empty");
            uint8_t cat_id = 0;  // Default to EMPTY
            if (m_category_library) {
                cat_id = m_category_library->get_id_by_name(cat_name);
            }
            def.category = static_cast<MaterialCategory>(cat_id);

            def.melt_point = j.value<uint8_t>("melt_point", 0);
            def.boil_point = j.value<uint8_t>("boil_point", 0);
            def.default_temp = j.value<uint8_t>("default_temp", 128);

            // Parse color
            if (j.contains("color")) {
                std::string color_str = j["color"].get<std::string>();
                if (color_str[0] == '#' && color_str.size() == 9) {
                    def.color = static_cast<uint32_t>(std::stoul(color_str.substr(1), nullptr, 16));
                }
            }
        }

        // Set source path for editor
        def.source_path = path;

        // Ensure materials array is large enough
        if (m_materials.size() <= def.id) {
            m_materials.resize(256);
        }

        m_materials[def.id] = def;
        m_name_to_id[def.internal_name] = def.id;

        return true;

    } catch (const nlohmann::json::exception& e) {
        Logger::instance().error("MaterialLibrary", "JSON parse error in %s: %s", path.c_str(), e.what());
        return false;
    }
}

bool MaterialLibrary::parse_material_v2(const nlohmann::json& j, MaterialDefinition& def) {
    // Required fields
    if (!j.contains("id")) {
        Logger::instance().warning("MaterialLibrary", "v2 material missing 'id'");
        return false;
    }
    def.id = j["id"].get<uint8_t>();
    def.internal_name = j.value("internal_name", "unknown");
    def.name = j.value("name", def.internal_name);

    // Physical section
    if (j.contains("physical")) {
        const auto& phys = j["physical"];
        def.density = phys.value<uint8_t>("density", 0);

        // Resolve category name via CategoryLibrary
        std::string cat_name = phys.value("category", "empty");
        uint8_t cat_id = 0;  // Default to EMPTY
        if (m_category_library) {
            cat_id = m_category_library->get_id_by_name(cat_name);
        }
        def.category = static_cast<MaterialCategory>(cat_id);
    }

    // Thermal section
    if (j.contains("thermal")) {
        const auto& therm = j["thermal"];
        def.default_temp = therm.value<uint8_t>("default_temp", 128);
        def.conductivity = therm.value<float>("conductivity", 0.5f);
        // Legacy melt/boil points (for backwards compatibility)
        def.melt_point = therm.value<uint8_t>("melt_point", 0);
        def.boil_point = therm.value<uint8_t>("boil_point", 0);
    }

    // Rendering section
    if (j.contains("rendering")) {
        const auto& rend = j["rendering"];
        if (rend.contains("default_color")) {
            std::string color_str = rend["default_color"].get<std::string>();
            if (color_str[0] == '#' && color_str.size() == 9) {
                def.color = static_cast<uint32_t>(std::stoul(color_str.substr(1), nullptr, 16));
            } else if (color_str[0] == '#' && color_str.size() == 7) {
                // #RRGGBB format (no alpha) - default to FF
                def.color = static_cast<uint32_t>(std::stoul(color_str.substr(1), nullptr, 16)) << 8 | 0xFF;
            }
        }
        if (rend.contains("color_variance")) {
            const auto& var = rend["color_variance"];
            def.color_variance_hue = var.value<uint8_t>("hue", 0);
            def.color_variance_saturation = var.value<uint8_t>("saturation", 0);
            def.color_variance_lightness = var.value<uint8_t>("lightness", 0);
        }
    }

    // Flags
    def.flags = 0;
    if (j.contains("flags") && j["flags"].is_array()) {
        std::vector<std::string> flag_strs;
        for (const auto& f : j["flags"]) {
            flag_strs.push_back(f.get<std::string>());
        }
        def.flags = strings_to_flags(flag_strs);
    }

    // Interactions
    if (j.contains("interactions") && j["interactions"].is_array()) {
        parse_interactions(j["interactions"], def);
    }

    return true;
}

void MaterialLibrary::parse_interactions(const nlohmann::json& arr, MaterialDefinition& def) {
    for (const auto& int_json : arr) {
        Interaction interaction;

        interaction.id = int_json.value("id", "");
        interaction.type = string_to_interaction_type(int_json.value("type", "temperature"));
        interaction.priority = int_json.value<uint8_t>("priority", 50);
        interaction.probability = int_json.value<float>("probability", 1.0f);
        interaction.sim_step_threshold = int_json.value<uint16_t>("sim_step_threshold", 0);

        // Parse conditions
        if (int_json.contains("conditions")) {
            const auto& cond = int_json["conditions"];
            interaction.conditions.temp_above = cond.value<uint8_t>("temp_above", 0);
            interaction.conditions.temp_below = cond.value<uint8_t>("temp_below", 255);

            if (cond.contains("contact_with") && cond["contact_with"].is_array()) {
                for (const auto& c : cond["contact_with"]) {
                    interaction.conditions.contact_with.push_back(c.get<std::string>());
                }
            }

            interaction.conditions.contact_with_category = cond.value("contact_with_category", "");
            interaction.conditions.self_has_flag = cond.value<uint8_t>("self_has_flag", 0);
            interaction.conditions.self_not_has_flag = cond.value<uint8_t>("self_not_has_flag", 0);
            interaction.conditions.neighbor_has_flag = cond.value<uint8_t>("neighbor_has_flag", 0);
            interaction.conditions.contact_with_flag = cond.value<uint8_t>("contact_with_flag", 0);
        }

        // Parse effects
        if (int_json.contains("effects") && int_json["effects"].is_array()) {
            for (const auto& eff_json : int_json["effects"]) {
                InteractionEffect effect;
                effect.type = string_to_effect_type(eff_json.value("type", "transform"));
                effect.material_name = eff_json.value("into", "");
                if (effect.material_name.empty()) {
                    effect.material_name = eff_json.value("material", "");
                }
                effect.delta = eff_json.value<int16_t>("delta", 0);
                effect.value = eff_json.value<uint8_t>("value", 0);
                effect.flag = string_to_flag(eff_json.value("flag", ""));

                // Color behavior: default to "replace" for transforms, "none" otherwise
                bool is_transform = (effect.type == EffectType::TRANSFORM ||
                                    effect.type == EffectType::TRANSFORM_NEIGHBOR);
                std::string default_cb = is_transform ? "replace" : "none";
                effect.color_behavior = string_to_color_behavior(eff_json.value("color_behavior", default_cb));

                // Color delta for ADD/SUBTRACT modes (hex string "#RRGGBBAA")
                if (eff_json.contains("color_delta")) {
                    std::string color_str = eff_json["color_delta"].get<std::string>();
                    if (color_str[0] == '#' && color_str.size() == 9) {
                        effect.color_delta = static_cast<uint32_t>(std::stoul(color_str.substr(1), nullptr, 16));
                    } else if (color_str[0] == '#' && color_str.size() == 7) {
                        effect.color_delta = (static_cast<uint32_t>(std::stoul(color_str.substr(1), nullptr, 16)) << 8) | 0xFF;
                    }
                }

                interaction.effects.push_back(effect);
            }
        }

        def.interactions.push_back(interaction);
    }
}

void MaterialLibrary::resolve_interaction_references() {
    for (auto& mat : m_materials) {
        for (auto& interaction : mat.interactions) {
            // Resolve contact_with names to IDs (stored as strings, used for GPU lookup)
            // The actual ID resolution happens in InteractionCompiler

            // For now, just validate that referenced materials exist
            for (const auto& name : interaction.conditions.contact_with) {
                if (m_name_to_id.find(name) == m_name_to_id.end()) {
                    Logger::instance().warning("MaterialLibrary",
                        "Material '%s' interaction '%s' references unknown material '%s'",
                        mat.internal_name.c_str(), interaction.id.c_str(), name.c_str());
                }
            }

            // Validate effect targets
            for (auto& effect : interaction.effects) {
                if (!effect.material_name.empty()) {
                    if (m_name_to_id.find(effect.material_name) == m_name_to_id.end()) {
                        Logger::instance().warning("MaterialLibrary",
                            "Material '%s' effect references unknown material '%s'",
                            mat.internal_name.c_str(), effect.material_name.c_str());
                    }
                }
            }
        }
    }
}

bool MaterialLibrary::compile_for_gpu() {
    InteractionCompiler compiler;

    if (!compiler.compile(*this, m_category_library)) {
        Logger::instance().error("MaterialLibrary", "Failed to compile interactions for GPU");
        return false;
    }

    m_compiled_materials = compiler.get_material_table();
    m_compiled_interactions = compiler.get_interaction_table();
    m_is_compiled = true;

    // Log any warnings from compilation
    for (const auto& warning : compiler.get_warnings()) {
        Logger::instance().warning("MaterialLibrary", "Compilation: %s", warning.c_str());
    }

    Logger::instance().info("MaterialLibrary",
        "Compiled %zu materials with %zu interactions for GPU",
        m_name_to_id.size(), compiler.interaction_count());

    return true;
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

            // Category (string -> ID via CategoryLibrary)
            std::string cat_str = mat_json.value("category", "empty");
            uint8_t cat_id = 0;  // Default to EMPTY
            if (m_category_library) {
                cat_id = m_category_library->get_id_by_name(cat_str);
            }
            def.category = static_cast<MaterialCategory>(cat_id);

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
    m_materials.resize(256); // Pre-allocate for all possible material IDs
    m_name_to_id.clear();
}

bool MaterialLibraryRegistry::load_library(const std::string& name, const std::string& path) {
    namespace fs = std::filesystem;

    auto library = std::make_unique<MaterialLibrary>();

    if (fs::is_directory(path)) {
        // Load all .material files from directory
        if (!library->load_from_directory(path)) {
            return false;
        }
    } else if (fs::exists(path)) {
        // Load single file (legacy JSON or .material)
        if (path.ends_with(".material")) {
            // Initialize materials array first
            library->clear();
            if (!library->load_material_file(path)) {
                return false;
            }
            library->resolve_interaction_references();
        } else {
            // Legacy JSON format
            if (!library->load_from_file(path)) {
                return false;
            }
        }
    } else {
        Logger::instance().error("MaterialLibrary", "Path not found: %s", path.c_str());
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
