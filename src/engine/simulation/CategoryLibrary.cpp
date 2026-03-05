#include "engine/simulation/CategoryLibrary.h"
#include "engine/core/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace engine::simulation {

namespace fs = std::filesystem;

// GPU packing constants
static constexpr int WORDS_PER_CATEGORY = 10;

bool CategoryLibrary::load_from_directory(const std::string& path, bool is_engine_default) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
        ENGINE_LOG_WARN("CategoryLibrary: Directory not found: %s", path.c_str());
        return false;
    }

    int loaded_count = 0;
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".phys") {
            if (load_category(entry.path().string(), is_engine_default)) {
                loaded_count++;
            }
        }
    }

    ENGINE_LOG("CategoryLibrary: Loaded %d categories from %s", loaded_count, path.c_str());
    return loaded_count > 0;
}

bool CategoryLibrary::load_category(const std::string& path, bool is_engine_default) {
    CategoryDefinition cat;
    if (!parse_phys_file(path, cat)) {
        return false;
    }

    cat.source_path = path;
    cat.is_engine_default = is_engine_default;

    // Check for ID conflicts
    for (auto& existing : m_categories) {
        if (existing.id == cat.id) {
            // Replace existing category with same ID
            existing = cat;
            rebuild_lookup_map();
            ENGINE_LOG("CategoryLibrary: Replaced category '%s' (ID %d)", cat.name.c_str(), cat.id);
            return true;
        }
    }

    // Add new category
    m_categories.push_back(cat);
    rebuild_lookup_map();
    ENGINE_LOG("CategoryLibrary: Loaded category '%s' (ID %d) from %s",
               cat.name.c_str(), cat.id, path.c_str());
    return true;
}

bool CategoryLibrary::parse_phys_file(const std::string& path, CategoryDefinition& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ENGINE_LOG_WARN("CategoryLibrary: Failed to open %s", path.c_str());
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        // Required fields
        out.id = j.value<uint8_t>("id", 0);
        out.name = j.value("name", "Unknown");
        out.internal_name = j.value("internal_name", "unknown");

        // Validate ID range
        if (out.id >= MAX_CATEGORIES) {
            ENGINE_LOG_WARN("CategoryLibrary: Category ID %d exceeds max %d in %s",
                           out.id, MAX_CATEGORIES - 1, path.c_str());
            return false;
        }

        // Optional fields
        out.mobile = j.value("mobile", false);
        out.randomize_equal_priority = j.value("randomize_equal_priority", true);

        // Parse movement rules
        out.movement_rules.clear();
        if (j.contains("movement_rules") && j["movement_rules"].is_array()) {
            for (const auto& rule_j : j["movement_rules"]) {
                if (out.movement_rules.size() >= MAX_MOVEMENT_RULES) {
                    ENGINE_LOG_WARN("CategoryLibrary: Category '%s' has more than %d rules, extras ignored",
                                   out.name.c_str(), MAX_MOVEMENT_RULES);
                    break;
                }

                MovementRule rule;
                rule.direction = direction_from_name(rule_j.value("direction", "none"));
                rule.priority = rule_j.value<uint8_t>("priority", 0);
                rule.density_check = rule_j.value("density_check", false);

                // Parse swap_with array
                if (rule_j.contains("swap_with") && rule_j["swap_with"].is_array()) {
                    for (const auto& cat_name : rule_j["swap_with"]) {
                        if (cat_name.is_string()) {
                            rule.swap_with.push_back(cat_name.get<std::string>());
                        }
                    }
                }

                out.movement_rules.push_back(rule);
            }
        }

        // Sort rules by priority (lower = first)
        std::stable_sort(out.movement_rules.begin(), out.movement_rules.end(),
            [](const MovementRule& a, const MovementRule& b) {
                return a.priority < b.priority;
            });

        // Parse dissipation
        out.dissipation = DissipationConfig{};
        if (j.contains("dissipation") && j["dissipation"].is_object()) {
            const auto& diss_j = j["dissipation"];
            out.dissipation.enabled = diss_j.value("enabled", false);
            out.dissipation.rate = diss_j.value("rate", 0.0f);
            out.dissipation.into_material = diss_j.value("into", "air");
        }

        return true;

    } catch (const nlohmann::json::exception& e) {
        ENGINE_LOG_WARN("CategoryLibrary: JSON parse error in %s: %s", path.c_str(), e.what());
        return false;
    }
}

bool CategoryLibrary::save_category(const CategoryDefinition& cat, const std::string& path) {
    try {
        nlohmann::json j;
        j["schema_version"] = 1;
        j["id"] = cat.id;
        j["name"] = cat.name;
        j["internal_name"] = cat.internal_name;
        j["mobile"] = cat.mobile;
        j["randomize_equal_priority"] = cat.randomize_equal_priority;

        // Movement rules
        nlohmann::json rules_arr = nlohmann::json::array();
        for (const auto& rule : cat.movement_rules) {
            nlohmann::json rule_j;
            rule_j["direction"] = direction_name(rule.direction);
            rule_j["priority"] = rule.priority;
            rule_j["density_check"] = rule.density_check;

            nlohmann::json swap_arr = nlohmann::json::array();
            for (const auto& name : rule.swap_with) {
                swap_arr.push_back(name);
            }
            rule_j["swap_with"] = swap_arr;

            rules_arr.push_back(rule_j);
        }
        j["movement_rules"] = rules_arr;

        // Dissipation
        nlohmann::json diss_j;
        diss_j["enabled"] = cat.dissipation.enabled;
        if (cat.dissipation.enabled) {
            diss_j["rate"] = cat.dissipation.rate;
            diss_j["into"] = cat.dissipation.into_material;
        }
        j["dissipation"] = diss_j;

        // Write to file
        std::ofstream file(path);
        if (!file.is_open()) {
            ENGINE_LOG_WARN("CategoryLibrary: Failed to open %s for writing", path.c_str());
            return false;
        }
        file << j.dump(2);
        ENGINE_LOG("CategoryLibrary: Saved category '%s' to %s", cat.name.c_str(), path.c_str());
        return true;

    } catch (const std::exception& e) {
        ENGINE_LOG_WARN("CategoryLibrary: Failed to save %s: %s", path.c_str(), e.what());
        return false;
    }
}

const CategoryDefinition* CategoryLibrary::get_by_id(uint8_t id) const {
    for (const auto& cat : m_categories) {
        if (cat.id == id) {
            return &cat;
        }
    }
    return nullptr;
}

const CategoryDefinition* CategoryLibrary::get_by_name(const std::string& internal_name) const {
    auto it = m_name_to_id.find(internal_name);
    if (it != m_name_to_id.end()) {
        return get_by_id(it->second);
    }
    return nullptr;
}

uint8_t CategoryLibrary::get_id_by_name(const std::string& internal_name) const {
    auto it = m_name_to_id.find(internal_name);
    if (it != m_name_to_id.end()) {
        return it->second;
    }
    return 255;  // Invalid ID
}

void CategoryLibrary::ensure_empty_category() {
    // Check if EMPTY already exists
    if (get_by_id(EMPTY_CATEGORY_ID) != nullptr) {
        return;
    }

    // Create default EMPTY category
    CategoryDefinition empty;
    empty.id = EMPTY_CATEGORY_ID;
    empty.name = "Empty";
    empty.internal_name = "empty";
    empty.mobile = false;
    empty.randomize_equal_priority = false;
    empty.is_engine_default = true;

    m_categories.push_back(empty);
    rebuild_lookup_map();
}

uint8_t CategoryLibrary::next_available_id() const {
    // Find first unused ID starting from 1 (0 is EMPTY)
    for (uint8_t id = 1; id < MAX_CATEGORIES; id++) {
        bool used = false;
        for (const auto& cat : m_categories) {
            if (cat.id == id) {
                used = true;
                break;
            }
        }
        if (!used) {
            return id;
        }
    }
    return 255;  // All slots full
}

void CategoryLibrary::add_or_update(CategoryDefinition cat) {
    // Check for existing category with same ID
    for (auto& existing : m_categories) {
        if (existing.id == cat.id) {
            existing = cat;
            rebuild_lookup_map();
            return;
        }
    }

    // Add new category
    m_categories.push_back(cat);
    rebuild_lookup_map();
}

bool CategoryLibrary::remove_by_id(uint8_t id) {
    // Cannot remove EMPTY
    if (id == EMPTY_CATEGORY_ID) {
        return false;
    }

    auto it = std::remove_if(m_categories.begin(), m_categories.end(),
        [id](const CategoryDefinition& cat) { return cat.id == id; });

    if (it != m_categories.end()) {
        m_categories.erase(it, m_categories.end());
        rebuild_lookup_map();
        return true;
    }
    return false;
}

void CategoryLibrary::clear() {
    m_categories.clear();
    m_name_to_id.clear();
}

void CategoryLibrary::rebuild_lookup_map() {
    m_name_to_id.clear();
    for (const auto& cat : m_categories) {
        m_name_to_id[cat.internal_name] = cat.id;
    }
}

uint16_t CategoryLibrary::build_swap_mask(const std::vector<std::string>& swap_with) const {
    uint16_t mask = 0;
    for (const auto& name : swap_with) {
        uint8_t id = get_id_by_name(name);
        if (id < MAX_CATEGORIES) {
            mask |= (1u << id);
        }
    }
    return mask;
}

void CategoryLibrary::pack_category(const CategoryDefinition& cat, uint32_t* out_words) const {
    // Initialize all words to 0
    for (int i = 0; i < WORDS_PER_CATEGORY; i++) {
        out_words[i] = 0;
    }

    // Word 0 (header):
    //   bit 0:      mobile
    //   bit 1:      randomize_equal_priority
    //   bit 2:      dissipation_enabled
    //   bits 3-6:   rule_count (0-8)
    //   bits 7-14:  dissipation_rate (0-255)
    //   bits 15-22: dissipation_into_mat_id (TODO: needs MaterialLibrary reference)
    //   bits 23-31: reserved

    uint32_t header = 0;
    if (cat.mobile) header |= 1u;
    if (cat.randomize_equal_priority) header |= 2u;
    if (cat.dissipation.enabled) header |= 4u;

    uint32_t rule_count = static_cast<uint32_t>(std::min(static_cast<size_t>(MAX_MOVEMENT_RULES), cat.movement_rules.size()));
    header |= (rule_count & 0xFu) << 3;

    // Dissipation rate (0.0-1.0 -> 0-255)
    uint8_t diss_rate = static_cast<uint8_t>(std::clamp(cat.dissipation.rate * 255.0f, 0.0f, 255.0f));
    header |= static_cast<uint32_t>(diss_rate) << 7;

    // Note: dissipation_into_mat_id requires MaterialLibrary, set to 0 (air) for now
    // This will be updated when we integrate with MaterialLibrary

    out_words[0] = header;

    // Words 1-8 (movement_rules[8]):
    //   bits 0-3:   direction (0-7, or 15 for unused)
    //   bits 4-7:   priority (0-15)
    //   bit 8:      density_check
    //   bits 9-24:  swap_category_mask (16 bits)
    //   bits 25-31: reserved

    for (size_t i = 0; i < MAX_MOVEMENT_RULES; i++) {
        uint32_t rule_word = 0;

        if (i < cat.movement_rules.size()) {
            const auto& rule = cat.movement_rules[i];

            uint32_t dir = static_cast<uint32_t>(rule.direction) & 0xFu;
            uint32_t priority = static_cast<uint32_t>(rule.priority) & 0xFu;
            uint32_t density = rule.density_check ? 1u : 0u;
            uint32_t swap_mask = build_swap_mask(rule.swap_with);

            rule_word = dir
                      | (priority << 4)
                      | (density << 8)
                      | (swap_mask << 9);
        } else {
            // Unused slot - direction = NONE (15)
            rule_word = 15u;  // DIR_NONE
        }

        out_words[1 + i] = rule_word;
    }

    // Word 9: reserved for future use
    out_words[9] = 0;
}

std::vector<uint32_t> CategoryLibrary::compile_gpu_table() const {
    // Allocate space for all 16 categories
    std::vector<uint32_t> table(MAX_CATEGORIES * WORDS_PER_CATEGORY, 0);

    // Pack each loaded category into its slot
    for (const auto& cat : m_categories) {
        if (cat.id < MAX_CATEGORIES) {
            pack_category(cat, &table[cat.id * WORDS_PER_CATEGORY]);
        }
    }

    return table;
}

} // namespace engine::simulation
