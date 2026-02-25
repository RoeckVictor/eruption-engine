#pragma once

#include "runtime/ComponentScript.h"
#include <entt/entt.hpp>
#include <string>

/// Debug tool script for spawning and erasing pixels on SimSurface grids.
///
/// When attached to an entity, this script enables:
/// - Left-click: Spawn pixels at mouse cursor position on any SimSurface
/// - Right-click: Erase pixels from any destructible PixelGrid
/// - Numpad 1-9: Select which material to spawn (maps to material IDs 1-9)
/// - +/-: Increase/decrease brush radius
///
/// This is a utility script for testing and debugging pixel simulations.
/// Requires a Camera2D entity reference to be set for proper coordinate conversion.
class PixelBrush : public runtime::ComponentScript {
public:
    const char* type_name() const override { return "PixelBrush"; }

    // Lifecycle
    void on_create() override;
    void on_update() override;

    // Inspector GUI for editing properties
    void on_inspector_gui(nlohmann::json& properties) override;

    // Property serialization
    void serialize_properties(nlohmann::json& out) const override;
    void deserialize_properties(const nlohmann::json& data) override;

private:
    bool m_enabled = true;

    /// Camera entity to use for screen-to-world conversion (resolved at runtime).
    entt::entity m_camera_entity = entt::null;

    /// Camera entity GUID for persistence (entity IDs change on scene reload).
    std::string m_camera_guid;

    /// Brush radius in pixels (1-50).
    int m_brush_radius = 3;

    /// Minimum brush radius.
    int m_min_radius = 1;

    /// Maximum brush radius.
    int m_max_radius = 50;

    /// Currently selected material ID (1-255, 0 is reserved for empty/air).
    int m_material_id = 1;
};
