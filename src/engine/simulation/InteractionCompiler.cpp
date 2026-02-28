#include "engine/simulation/InteractionCompiler.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/simulation/CategoryLibrary.h"
#include "engine/core/Logger.h"
#include <algorithm>

namespace engine::simulation {

bool InteractionCompiler::compile(const MaterialLibrary& library, const CategoryLibrary* category_library) {
    m_material_table.clear();
    m_interaction_table.clear();
    m_warnings.clear();
    m_interaction_count = 0;

    // Reserve space for 256 materials (2 uint32s each)
    m_material_table.resize(512, 0);

    // Build name-to-ID map for resolving references
    std::unordered_map<std::string, uint8_t> name_to_id;
    const auto& materials = library.get_all_materials();
    for (const auto& mat : materials) {
        name_to_id[mat.internal_name] = mat.id;
    }

    // Track current offset in interaction table
    uint16_t current_offset = 0;

    // Process each material
    for (const auto& mat : materials) {
        if (mat.id >= 256) {
            m_warnings.push_back("Material ID " + std::to_string(mat.id) + " out of range");
            continue;
        }

        // Sort interactions by priority (descending) so highest priority is first
        std::vector<Interaction> sorted_interactions = mat.interactions;
        std::sort(sorted_interactions.begin(), sorted_interactions.end(),
            [](const Interaction& a, const Interaction& b) {
                return a.priority > b.priority;
            });

        // Limit to 15 interactions per material (4-bit count)
        uint8_t interaction_count = static_cast<uint8_t>(
            std::min(sorted_interactions.size(), size_t(15)));

        if (sorted_interactions.size() > 15) {
            m_warnings.push_back("Material '" + mat.internal_name +
                "' has " + std::to_string(sorted_interactions.size()) +
                " interactions, truncating to 15");
        }

        // Pack interactions into GPU format (6 uint32s per interaction)
        for (size_t i = 0; i < interaction_count; ++i) {
            PackedInteraction packed = pack_interaction(sorted_interactions[i], name_to_id, category_library);
            m_interaction_table.push_back(packed.conditions_0);
            m_interaction_table.push_back(packed.conditions_1);
            m_interaction_table.push_back(packed.effects);
            m_interaction_table.push_back(packed.metadata);
            m_interaction_table.push_back(packed.color_delta_1);
            m_interaction_table.push_back(packed.color_delta_2);
            m_interaction_count++;
        }

        // Pack material slot
        auto [word0, word1] = pack_material_slot(mat, current_offset, interaction_count);
        m_material_table[mat.id * 2] = word0;
        m_material_table[mat.id * 2 + 1] = word1;

        current_offset += interaction_count;

        // Check for overflow (12-bit offset = max 4095)
        if (current_offset > 4095) {
            Logger::instance().error("InteractionCompiler", "Interaction table overflow! Too many total interactions.");
            return false;
        }
    }

    Logger::instance().info("InteractionCompiler", "Compiled %zu interactions for %zu materials",
        m_interaction_count, materials.size());

    return true;
}

PackedInteraction InteractionCompiler::pack_interaction(
    const Interaction& interaction,
    const std::unordered_map<std::string, uint8_t>& name_to_id,
    const CategoryLibrary* category_library)
{
    PackedInteraction packed{};

    const auto& cond = interaction.conditions;

    // Word 0: temp_above(8) | temp_below(8) | contact_mat_1(8) | contact_mat_2(8)
    uint8_t contact_mats[4] = {255, 255, 255, 255};  // 255 = none
    for (size_t i = 0; i < std::min(cond.contact_with.size(), size_t(4)); ++i) {
        auto it = name_to_id.find(cond.contact_with[i]);
        if (it != name_to_id.end()) {
            contact_mats[i] = it->second;
        } else {
            m_warnings.push_back("Unknown material reference: " + cond.contact_with[i]);
        }
    }

    packed.conditions_0 =
        static_cast<uint32_t>(cond.temp_above) |
        (static_cast<uint32_t>(cond.temp_below) << 8) |
        (static_cast<uint32_t>(contact_mats[0]) << 16) |
        (static_cast<uint32_t>(contact_mats[1]) << 24);

    // Word 1: contact_mat_3(8) | contact_mat_4(8) | type(4) | self_flag(4) | contact_flag(4) | contact_cat(4)
    uint8_t contact_category = 15;  // 15 = any category
    if (!cond.contact_with_category.empty() && category_library) {
        contact_category = category_library->get_id_by_name(cond.contact_with_category);
        if (contact_category == 255) {
            contact_category = 15;  // Fallback to "any" if not found
            m_warnings.push_back("Unknown category reference: " + cond.contact_with_category);
        }
    }

    packed.conditions_1 =
        static_cast<uint32_t>(contact_mats[2]) |
        (static_cast<uint32_t>(contact_mats[3]) << 8) |
        (static_cast<uint32_t>(interaction.type) << 16) |
        ((static_cast<uint32_t>(cond.self_has_flag) & 0xF) << 20) |
        ((static_cast<uint32_t>(cond.contact_with_flag) & 0xF) << 24) |
        ((static_cast<uint32_t>(contact_category) & 0xF) << 28);

    // Word 2: effect_type_1(4) | effect_param_1(8) | effect_type_2(4) | effect_param_2(8) | color_behavior_1(3) | color_behavior_2(3) | reserved(2)
    uint8_t effect_type_1 = 15;  // 15 = none
    uint8_t effect_param_1 = 0;
    uint8_t effect_type_2 = 15;
    uint8_t effect_param_2 = 0;
    uint8_t color_behavior_1 = 0;  // NONE
    uint8_t color_behavior_2 = 0;  // NONE
    uint32_t color_delta_1 = 0;
    uint32_t color_delta_2 = 0;

    if (!interaction.effects.empty()) {
        const auto& eff0 = interaction.effects[0];
        effect_type_1 = static_cast<uint8_t>(eff0.type);
        color_behavior_1 = static_cast<uint8_t>(eff0.color_behavior);
        color_delta_1 = eff0.color_delta;

        // Determine parameter based on effect type
        switch (eff0.type) {
            case EffectType::TRANSFORM:
            case EffectType::TRANSFORM_NEIGHBOR:
            case EffectType::SPAWN_PARTICLE: {
                auto it = name_to_id.find(eff0.material_name);
                if (it != name_to_id.end()) {
                    effect_param_1 = it->second;
                } else if (!eff0.material_name.empty()) {
                    m_warnings.push_back("Unknown material in effect: " + eff0.material_name);
                }
                break;
            }
            case EffectType::SET_TEMP:
            case EffectType::SET_NEIGHBOR_TEMP:
                effect_param_1 = eff0.value;
                break;
            case EffectType::CHANGE_TEMP:
            case EffectType::CHANGE_NEIGHBOR_TEMP:
                // Delta is int16_t, clamp to int8_t range
                effect_param_1 = static_cast<uint8_t>(
                    std::clamp(static_cast<int>(eff0.delta), -128, 127));
                break;
            case EffectType::SET_FLAG:
            case EffectType::CLEAR_FLAG:
            case EffectType::SET_NEIGHBOR_FLAG:
                effect_param_1 = eff0.flag;
                break;
            case EffectType::DESTROY:
                effect_param_1 = 0;  // Air
                break;
        }
    }

    if (interaction.effects.size() > 1) {
        const auto& eff1 = interaction.effects[1];
        effect_type_2 = static_cast<uint8_t>(eff1.type);
        color_behavior_2 = static_cast<uint8_t>(eff1.color_behavior);
        color_delta_2 = eff1.color_delta;

        switch (eff1.type) {
            case EffectType::TRANSFORM:
            case EffectType::TRANSFORM_NEIGHBOR:
            case EffectType::SPAWN_PARTICLE: {
                auto it = name_to_id.find(eff1.material_name);
                if (it != name_to_id.end()) {
                    effect_param_2 = it->second;
                }
                break;
            }
            case EffectType::SET_TEMP:
            case EffectType::SET_NEIGHBOR_TEMP:
                effect_param_2 = eff1.value;
                break;
            case EffectType::CHANGE_TEMP:
            case EffectType::CHANGE_NEIGHBOR_TEMP:
                effect_param_2 = static_cast<uint8_t>(
                    std::clamp(static_cast<int>(eff1.delta), -128, 127));
                break;
            case EffectType::SET_FLAG:
            case EffectType::CLEAR_FLAG:
            case EffectType::SET_NEIGHBOR_FLAG:
                effect_param_2 = eff1.flag;
                break;
            case EffectType::DESTROY:
                effect_param_2 = 0;
                break;
        }
    }

    // Word 2: eff_type_1(4) | eff_param_1(8) | eff_type_2(4) | eff_param_2(8) | color_behavior_1(3) | color_behavior_2(3) | reserved(2)
    packed.effects =
        (static_cast<uint32_t>(effect_type_1) & 0xF) |
        (static_cast<uint32_t>(effect_param_1) << 4) |
        ((static_cast<uint32_t>(effect_type_2) & 0xF) << 12) |
        (static_cast<uint32_t>(effect_param_2) << 16) |
        ((static_cast<uint32_t>(color_behavior_1) & 0x7) << 24) |
        ((static_cast<uint32_t>(color_behavior_2) & 0x7) << 27);

    // Words 4-5: color deltas
    packed.color_delta_1 = color_delta_1;
    packed.color_delta_2 = color_delta_2;

    // Word 3: priority(8) | probability_u8(8) | sim_step_threshold(16)
    uint8_t probability_u8 = static_cast<uint8_t>(
        std::clamp(interaction.probability * 255.0f, 0.0f, 255.0f));

    packed.metadata =
        static_cast<uint32_t>(interaction.priority) |
        (static_cast<uint32_t>(probability_u8) << 8) |
        (static_cast<uint32_t>(interaction.sim_step_threshold) << 16);

    return packed;
}

std::pair<uint32_t, uint32_t> InteractionCompiler::pack_material_slot(
    const MaterialDefinition& def,
    uint16_t interaction_offset,
    uint8_t interaction_count)
{
    // Word 0: density(8) | category(4) | interaction_offset(12) | interaction_count(4) | flags(4)
    uint32_t word0 =
        static_cast<uint32_t>(def.density) |
        ((static_cast<uint32_t>(def.category) & 0xF) << 8) |
        ((static_cast<uint32_t>(interaction_offset) & 0xFFF) << 12) |
        ((static_cast<uint32_t>(interaction_count) & 0xF) << 24) |
        ((static_cast<uint32_t>(def.flags) & 0xF) << 28);

    // Word 1: default_temp(8) | conductivity_u8(8) | melt_point(8) | melt_into(8)
    // Note: melt_point/melt_into are legacy fields kept for backwards compatibility.
    // New materials should use temperature interactions instead.
    uint8_t conductivity_u8 = static_cast<uint8_t>(
        std::clamp(def.conductivity * 255.0f, 0.0f, 255.0f));

    uint32_t word1 =
        static_cast<uint32_t>(def.default_temp) |
        (static_cast<uint32_t>(conductivity_u8) << 8) |
        (static_cast<uint32_t>(def.melt_point) << 16) |
        (static_cast<uint32_t>(def.melt_into) << 24);

    return {word0, word1};
}

} // namespace engine::simulation
