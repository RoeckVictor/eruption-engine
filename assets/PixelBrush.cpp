#include "PixelBrush.h"
#include "engine/render/Camera2D.h"
#include "engine/core/Transform.h"
#include <imgui.h>
#include <algorithm>
#include <vector>
#include <string>

// Register the script with the engine
REGISTER_COMPONENT_SCRIPT(PixelBrush)

void PixelBrush::on_create() {
    // Resolve camera entity from stored GUID (properties are deserialized before on_create)
    if (!m_camera_guid.empty()) {
        m_camera_entity = find_entity_by_guid(m_camera_guid.c_str());
        if (m_camera_entity == entt::null) {
            log("PixelBrush: Warning - Camera with GUID not found, may have been deleted.");
        }
    }

    log("PixelBrush: Ready. Left-click to spawn, right-click to erase.");
    log("PixelBrush: Use numpad 1-9 to select material, +/- to change brush size.");
    if (m_camera_entity == entt::null) {
        log("PixelBrush: Set a Camera2D entity in the inspector for proper coordinate conversion.");
    }
}

void PixelBrush::on_update() {
    if (!m_enabled) return;

    // Handle brush radius changes (+/- keys)
    if (is_key_pressed(engine::platform::KeyCode::NumpadAdd) ||
        is_key_pressed(engine::platform::KeyCode::Equal)) {
        m_brush_radius = (std::min)(m_brush_radius + 1, m_max_radius);
    }
    if (is_key_pressed(engine::platform::KeyCode::NumpadSubtract) ||
        is_key_pressed(engine::platform::KeyCode::Minus)) {
        m_brush_radius = (std::max)(m_brush_radius - 1, m_min_radius);
    }

    // Handle material selection (numpad 1-9)
    if (is_key_pressed(engine::platform::KeyCode::Numpad1)) m_material_id = 1;
    if (is_key_pressed(engine::platform::KeyCode::Numpad2)) m_material_id = 2;
    if (is_key_pressed(engine::platform::KeyCode::Numpad3)) m_material_id = 3;
    if (is_key_pressed(engine::platform::KeyCode::Numpad4)) m_material_id = 4;
    if (is_key_pressed(engine::platform::KeyCode::Numpad5)) m_material_id = 5;
    if (is_key_pressed(engine::platform::KeyCode::Numpad6)) m_material_id = 6;
    if (is_key_pressed(engine::platform::KeyCode::Numpad7)) m_material_id = 7;
    if (is_key_pressed(engine::platform::KeyCode::Numpad8)) m_material_id = 8;
    if (is_key_pressed(engine::platform::KeyCode::Numpad9)) m_material_id = 9;

    // Check for mouse input
    bool left_click = is_mouse_held(engine::platform::MouseButton::Left);
    bool right_click = is_mouse_held(engine::platform::MouseButton::Right);

    if (!left_click && !right_click) return;

    // Need a valid camera entity for coordinate conversion
    if (m_camera_entity == entt::null || !m_registry || !m_registry->valid(m_camera_entity)) {
        return;
    }

    // Get camera components
    auto* camera = m_registry->try_get<engine::render::Camera2D>(m_camera_entity);
    auto* cam_transform = m_registry->try_get<engine::Transform>(m_camera_entity);
    if (!camera || !cam_transform) {
        return;
    }

    // Get camera world position (transform position + camera offset)
    float cam_x = cam_transform->world_x + camera->x;
    float cam_y = cam_transform->world_y + camera->y;
    float cam_zoom = camera->zoom;

    // Get screen/viewport dimensions
    runtime::Vec2 screen_size = get_screen_size();
    float screen_w = screen_size.x;
    float screen_h = screen_size.y;

    // Get viewport offset (non-zero in editor panels, zero in standalone/fullscreen)
    runtime::Vec2 vp_offset = get_viewport_offset();

    // Get mouse position relative to viewport (not window)
    float screen_x = static_cast<float>(mouse_x()) - vp_offset.x;
    float screen_y = static_cast<float>(mouse_y()) - vp_offset.y;

    // Convert screen coords to world coords with proper Y flip
    // Screen: (0,0) at top-left, Y increases downward
    // World: Y increases upward
    float world_x = cam_x + (screen_x - screen_w * 0.5f) / cam_zoom;
    float world_y = cam_y - (screen_y - screen_h * 0.5f) / cam_zoom;  // Note: minus for Y flip

    // Spawn or erase pixels
    if (left_click) {
        spawn_pixels_at_world(world_x, world_y, m_brush_radius, m_material_id);
    } else if (right_click) {
        erase_pixels_at_world(world_x, world_y, m_brush_radius);
    }
}

void PixelBrush::on_inspector_gui(nlohmann::json& properties) {
    // Load current values from properties (needed for edit mode where temp scripts are created)
    if (properties.contains("enabled")) m_enabled = properties["enabled"].get<bool>();
    if (properties.contains("brush_radius")) m_brush_radius = properties["brush_radius"].get<int>();
    if (properties.contains("material_id")) m_material_id = properties["material_id"].get<int>();
    if (properties.contains("camera_guid") && properties["camera_guid"].is_string()) {
        m_camera_guid = properties["camera_guid"].get<std::string>();
    }

    ImGui::Text("=== Pixel Brush Tool ===");
    ImGui::Separator();

    ImGui::Checkbox("Enabled", &m_enabled);

    ImGui::Separator();

    // Camera entity picker
    ImGui::Text("Camera:");

    // Read available cameras from properties (injected by InspectorPanel to avoid DLL boundary issues)
    std::vector<entt::entity> camera_entities;
    std::vector<std::string> camera_names;
    std::vector<std::string> camera_guids;
    camera_names.push_back("(None)");
    camera_entities.push_back(entt::null);
    camera_guids.push_back("");

    int current_index = 0;

    if (properties.contains("__available_cameras__") && properties["__available_cameras__"].is_array()) {
        for (const auto& cam_info : properties["__available_cameras__"]) {
            if (cam_info.contains("id") && cam_info.contains("name")) {
                auto entity = static_cast<entt::entity>(cam_info["id"].get<uint32_t>());
                std::string name = cam_info["name"].get<std::string>();
                std::string guid = cam_info.contains("guid") ? cam_info["guid"].get<std::string>() : "";

                camera_entities.push_back(entity);
                camera_names.push_back(name);
                // Use GUID if available, otherwise use name as fallback identifier
                std::string identifier = !guid.empty() ? guid : name;
                camera_guids.push_back(identifier);

                // Match by identifier (GUID or name fallback)
                if (!m_camera_guid.empty() && identifier == m_camera_guid) {
                    current_index = static_cast<int>(camera_entities.size()) - 1;
                    m_camera_entity = entity;  // Resolve entity from identifier
                }
            }
        }
    }

    // Show combo box
    if (ImGui::BeginCombo("##CameraEntity", camera_names[current_index].c_str())) {
        for (size_t i = 0; i < camera_names.size(); ++i) {
            bool is_selected = (current_index == static_cast<int>(i));
            if (ImGui::Selectable(camera_names[i].c_str(), is_selected)) {
                m_camera_entity = camera_entities[i];
                m_camera_guid = camera_guids[i];
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (m_camera_guid.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: No camera selected!");
    }

    ImGui::Separator();
    ImGui::SliderInt("Brush Radius", &m_brush_radius, m_min_radius, m_max_radius);
    ImGui::SliderInt("Material ID", &m_material_id, 1, 255);

    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::BulletText("Left Click: Spawn pixels");
    ImGui::BulletText("Right Click: Erase pixels");
    ImGui::BulletText("Numpad 1-9: Select material");
    ImGui::BulletText("+/-: Change brush size");

    // Store all properties for persistence (needed for edit mode)
    // Use GUID instead of entity ID since entity IDs change on scene reload
    properties["enabled"] = m_enabled;
    properties["brush_radius"] = m_brush_radius;
    properties["material_id"] = m_material_id;
    properties["camera_guid"] = m_camera_guid;
}

void PixelBrush::serialize_properties(nlohmann::json& out) const {
    out["enabled"] = m_enabled;
    out["brush_radius"] = m_brush_radius;
    out["min_radius"] = m_min_radius;
    out["max_radius"] = m_max_radius;
    out["material_id"] = m_material_id;
    out["camera_guid"] = m_camera_guid;
}

void PixelBrush::deserialize_properties(const nlohmann::json& data) {
    if (data.contains("enabled")) m_enabled = data["enabled"].get<bool>();
    if (data.contains("brush_radius")) m_brush_radius = data["brush_radius"].get<int>();
    if (data.contains("min_radius")) m_min_radius = data["min_radius"].get<int>();
    if (data.contains("max_radius")) m_max_radius = data["max_radius"].get<int>();
    if (data.contains("material_id")) m_material_id = data["material_id"].get<int>();
    if (data.contains("camera_guid") && data["camera_guid"].is_string()) {
        m_camera_guid = data["camera_guid"].get<std::string>();
    }
}
