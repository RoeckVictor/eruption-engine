#pragma once

#include "engine/simulation/MaterialDefinition.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace engine::simulation {

/// Material library loaded from JSON.
///
/// A material library defines a complete set of materials (up to 256)
/// with their properties, colors, and behaviors. Multiple libraries
/// can exist (e.g., "default", "sci-fi", "fantasy") and be swapped
/// at runtime.
///
/// JSON format:
/// ```json
/// {
///   "name": "Default Materials",
///   "version": "1.0",
///   "materials": [
///     {
///       "id": 0,
///       "name": "Air",
///       "internal_name": "air",
///       "density": 0,
///       "category": "empty",
///       "color": "#1A1A2EFF"
///     },
///     {
///       "id": 1,
///       "name": "Rock",
///       "internal_name": "rock",
///       "density": 255,
///       "category": "static",
///       "melt_point": 250,
///       "melt_into": "lava",
///       "default_temp": 128,
///       "color": "#6B6B6BFF"
///     }
///   ]
/// }
/// ```
class MaterialLibrary {
public:
    MaterialLibrary() = default;

    /// Load material library from JSON file.
    /// Returns true on success, false on failure.
    bool load_from_file(const std::string& path);

    /// Load material library from JSON string.
    /// Returns true on success, false on failure.
    bool load_from_json(const std::string& json_str);

    /// Get material definition by ID.
    const MaterialDefinition* get_material(uint8_t id) const {
        if (id >= m_materials.size()) return nullptr;
        return &m_materials[id];
    }

    /// Get material definition by internal name.
    const MaterialDefinition* get_material(const std::string& internal_name) const {
        auto it = m_name_to_id.find(internal_name);
        if (it == m_name_to_id.end()) return nullptr;
        return &m_materials[it->second];
    }

    /// Get all material definitions.
    const std::vector<MaterialDefinition>& get_all_materials() const {
        return m_materials;
    }

    /// Get material color (for rendering).
    uint32_t get_color(uint8_t id) const {
        if (id >= m_materials.size()) return 0xFFFFFFFF;
        return m_materials[id].color;
    }

    /// Get material count.
    size_t material_count() const {
        return m_materials.size();
    }

    /// Get library name.
    const std::string& name() const { return m_name; }

    /// Get library version.
    const std::string& version() const { return m_version; }

    /// Build material slots array for GPU upload.
    std::vector<MaterialSlot> build_material_slots() const;

    /// Build color palette array for rendering (256 RGBA values).
    std::vector<uint32_t> build_color_palette() const;

    /// Clear library (remove all materials).
    void clear();

private:
    std::string m_name;
    std::string m_version;
    std::vector<MaterialDefinition> m_materials;  // Index by material ID
    std::unordered_map<std::string, uint8_t> m_name_to_id;

    bool parse_json(const std::string& json_str);
};

/// Global material library registry.
///
/// Manages multiple material libraries by name. Games can load
/// different material sets and switch between them.
class MaterialLibraryRegistry {
public:
    static MaterialLibraryRegistry& instance() {
        static MaterialLibraryRegistry s_instance;
        return s_instance;
    }

    /// Load material library from file and register it.
    /// Returns true on success.
    bool load_library(const std::string& name, const std::string& path);

    /// Get material library by name.
    MaterialLibrary* get_library(const std::string& name);
    const MaterialLibrary* get_library(const std::string& name) const;

    /// Get or create material library.
    MaterialLibrary* get_or_create_library(const std::string& name);

    /// Remove material library.
    void remove_library(const std::string& name);

    /// Get all library names.
    std::vector<std::string> get_library_names() const;

private:
    MaterialLibraryRegistry() = default;
    std::unordered_map<std::string, std::unique_ptr<MaterialLibrary>> m_libraries;
};

} // namespace engine::simulation
