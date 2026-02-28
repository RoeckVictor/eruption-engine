#pragma once

#include "engine/simulation/CategoryDefinition.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace engine::simulation {

/// Manages loading, saving, and lookup of physical categories.
/// Categories define movement behavior for pixels (powder, liquid, gas, etc.).
class CategoryLibrary {
public:
    /// Load categories from a directory (scans for .phys files).
    /// @param path Directory path to scan.
    /// @param is_engine_default True if these are read-only engine defaults.
    /// @return True if at least one category was loaded successfully.
    bool load_from_directory(const std::string& path, bool is_engine_default = false);

    /// Load a single category file.
    /// @param path Path to the .phys file.
    /// @param is_engine_default True if this is a read-only engine default.
    /// @return True if the category was loaded successfully.
    bool load_category(const std::string& path, bool is_engine_default = false);

    /// Save a category to a file.
    /// @param cat Category definition to save.
    /// @param path Path to write to (should end in .phys).
    /// @return True if saved successfully.
    bool save_category(const CategoryDefinition& cat, const std::string& path);

    /// Look up a category by ID.
    /// @return Pointer to category or nullptr if not found.
    const CategoryDefinition* get_by_id(uint8_t id) const;

    /// Look up a category by internal name.
    /// @return Pointer to category or nullptr if not found.
    const CategoryDefinition* get_by_name(const std::string& internal_name) const;

    /// Get category ID by internal name.
    /// @return Category ID or 255 if not found.
    uint8_t get_id_by_name(const std::string& internal_name) const;

    /// Get all loaded categories.
    const std::vector<CategoryDefinition>& all_categories() const { return m_categories; }

    /// Get mutable reference to all categories (for editor).
    std::vector<CategoryDefinition>& all_categories_mutable() { return m_categories; }

    /// Compile categories to GPU-ready format.
    /// @return Vector of uint32s (10 words per category, 16 categories = 160 words).
    std::vector<uint32_t> compile_gpu_table() const;

    /// Ensure the EMPTY category (ID 0) exists.
    void ensure_empty_category();

    /// Get the next available category ID for creating a new category.
    /// @return ID in range 1-15, or 255 if all slots are full.
    uint8_t next_available_id() const;

    /// Add or update a category (for editor).
    /// If a category with the same ID exists, it will be replaced.
    void add_or_update(CategoryDefinition cat);

    /// Remove a category by ID (for editor).
    /// Cannot remove EMPTY (ID 0).
    /// @return True if removed, false if not found or is EMPTY.
    bool remove_by_id(uint8_t id);

    /// Clear all categories.
    void clear();

    /// Get count of loaded categories.
    size_t count() const { return m_categories.size(); }

private:
    std::vector<CategoryDefinition> m_categories;
    std::unordered_map<std::string, uint8_t> m_name_to_id;

    /// Parse a .phys JSON file.
    bool parse_phys_file(const std::string& path, CategoryDefinition& out);

    /// Rebuild the name-to-ID lookup map.
    void rebuild_lookup_map();

    /// Build a 16-bit swap mask from category names.
    uint16_t build_swap_mask(const std::vector<std::string>& swap_with) const;

    /// Pack a single category to GPU format (10 uint32s).
    void pack_category(const CategoryDefinition& cat, uint32_t* out_words) const;
};

} // namespace engine::simulation
