#pragma once

#include "engine/simulation/MaterialDefs.h"
#include "engine/simulation/MaterialDefinition.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace engine::simulation {

class MaterialLibrary;
class CategoryLibrary;

/// Compiles material interactions from JSON/CPU format to GPU-packed format.
///
/// The compiler takes loaded MaterialDefinitions and:
/// 1. Resolves material name references to IDs
/// 2. Packs interactions into 16-byte GPU format
/// 3. Sorts by priority (descending)
/// 4. Updates MaterialSlots with interaction offsets/counts
///
/// Output is two buffers:
/// - Material table (256 * 8 bytes): one MaterialSlot per material
/// - Interaction table (N * 16 bytes): packed interactions, indexed by offset
class InteractionCompiler {
public:
    InteractionCompiler() = default;

    /// Compile all interactions from a material library.
    /// @param library The source material library with loaded definitions.
    /// @param category_library Optional category library for resolving category names.
    /// @return true on success, false if any material references couldn't be resolved.
    bool compile(const MaterialLibrary& library, const CategoryLibrary* category_library = nullptr);

    /// Get the compiled material table (for GPU upload).
    /// Contains 256 entries, each packed into 2 uint32s.
    const std::vector<uint32_t>& get_material_table() const { return m_material_table; }

    /// Get the compiled interaction table (for GPU upload).
    /// Contains packed interactions as uvec4s (4 uint32s each).
    const std::vector<uint32_t>& get_interaction_table() const { return m_interaction_table; }

    /// Get the number of compiled interactions.
    size_t interaction_count() const { return m_interaction_count; }

    /// Check if compilation produced any warnings.
    const std::vector<std::string>& get_warnings() const { return m_warnings; }

private:
    /// Pack a single interaction into GPU format.
    /// @param interaction Source interaction definition.
    /// @param name_to_id Map of material names to IDs.
    /// @param category_library Optional category library for resolving category names.
    /// @return Packed interaction data.
    PackedInteraction pack_interaction(
        const Interaction& interaction,
        const std::unordered_map<std::string, uint8_t>& name_to_id,
        const CategoryLibrary* category_library);

    /// Pack a MaterialDefinition into a MaterialSlot.
    /// @param def The material definition.
    /// @param interaction_offset Offset into interaction table.
    /// @param interaction_count Number of interactions for this material.
    /// @return Packed material slot data (2 uint32s).
    std::pair<uint32_t, uint32_t> pack_material_slot(
        const MaterialDefinition& def,
        uint16_t interaction_offset,
        uint8_t interaction_count);

    std::vector<uint32_t> m_material_table;      // 256 * 2 uint32s
    std::vector<uint32_t> m_interaction_table;   // N * 4 uint32s
    size_t m_interaction_count = 0;
    std::vector<std::string> m_warnings;
};

} // namespace engine::simulation
