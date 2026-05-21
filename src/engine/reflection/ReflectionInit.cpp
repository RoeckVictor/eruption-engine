#include "ReflectionInit.h"
#include "ReflectionSerializer.h"
#include "EngineComponentList.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/render/Camera2D.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/simulation/SimSurface.h"
#include "engine/ui/UIComponents.h"
#include "engine/audio/AudioSource.h"
#include "engine/audio/AudioListener.h"
#include "engine/core/Logger.h"
#include "ReflectionMacros.h"

// Include the reflection definitions directly
// These define the TypeReflector specializations

// Transform reflection
REFLECT_TYPE_BEGIN(engine::Transform)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(x, "X Position")
    REFLECT_PROPERTY(y, "Y Position")
    REFLECT_PROPERTY(rotation, "Rotation")
    REFLECT_PROPERTY(scale_x, "Scale X")
    REFLECT_PROPERTY(scale_y, "Scale Y")
REFLECT_TYPE_END()

// ScreenRect reflection (screen-space UI entities)
REFLECT_TYPE_BEGIN(engine::ScreenRect)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY_RANGE(anchor_x, "Anchor X", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(anchor_y, "Anchor Y", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(pivot_x, "Pivot X", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(pivot_y, "Pivot Y", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY(width, "Width")
    REFLECT_PROPERTY(height, "Height")
REFLECT_TYPE_END()

// Camera2D reflection
REFLECT_TYPE_BEGIN(engine::render::Camera2D)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(x, "X Position")
    REFLECT_PROPERTY(y, "Y Position")
    REFLECT_PROPERTY_RANGE(zoom, "Zoom", 0.1f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(min_zoom, "Min Zoom", 0.01f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(max_zoom, "Max Zoom", 1.0f, 20.0f, 0.5f)
    REFLECT_PROPERTY_RANGE(smoothing, "Smoothing", 0.0f, 20.0f, 0.5f)
REFLECT_TYPE_END()

// Image reflection
REFLECT_TYPE_BEGIN(engine::render::Image)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(layer, "Render Layer")
    REFLECT_PROPERTY(sprite_path, "Sprite")
    REFLECT_PROPERTY_RANGE(color_r, "Color R", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(color_g, "Color G", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(color_b, "Color B", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(color_a, "Color A", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(uv_min_x, "UV Min X", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(uv_min_y, "UV Min Y", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(uv_max_x, "UV Max X", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(uv_max_y, "UV Max Y", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(flip_x, "Flip X")
    REFLECT_PROPERTY(flip_y, "Flip Y")
REFLECT_TYPE_END()

// Text reflection
REFLECT_TYPE_BEGIN(engine::render::Text)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(layer, "Render Layer")
    REFLECT_PROPERTY_FLAGS(content, "Text", PropertyFlags::Multiline)
    REFLECT_PROPERTY(font_path, "Font")
    REFLECT_PROPERTY_RANGE(font_size, "Font Size", 1.0f, 200.0f, 1.0f)
    REFLECT_PROPERTY_RANGE(color_r, "Color R", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(color_g, "Color G", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(color_b, "Color B", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(color_a, "Color A", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(h_align, "Horizontal Align")
    REFLECT_PROPERTY(v_align, "Vertical Align")
    REFLECT_PROPERTY_RANGE(line_height, "Line Height", 0.5f, 3.0f, 0.1f)
    REFLECT_PROPERTY(bold, "Bold")
    REFLECT_PROPERTY(italic, "Italic")
    REFLECT_PROPERTY(underline, "Underline")
    REFLECT_PROPERTY(max_width, "Max Width")
REFLECT_TYPE_END()

// Animator reflection
// Note: Only serializable properties are reflected here.
// Runtime state (current_state, parameters, etc.) is managed by AnimationSystem.
REFLECT_TYPE_BEGIN(engine::animation::Animator)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(controller_path, "Controller")
REFLECT_TYPE_END()

// PixelGridComponent reflection
// Note: 'loaded' is NOT serialized - it's a runtime flag set when data is actually loaded
REFLECT_TYPE_BEGIN(engine::simulation::PixelGridComponent)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(pixel_grid_path, "Pixel Grid Path")
    REFLECT_PROPERTY(width, "Width")
    REFLECT_PROPERTY(height, "Height")
    REFLECT_PROPERTY(origin_x, "Origin X")
    REFLECT_PROPERTY(origin_y, "Origin Y")
    REFLECT_PROPERTY(destructible, "Destructible")
REFLECT_TYPE_END()

// PixelGridRenderer reflection
REFLECT_TYPE_BEGIN(engine::render::PixelGridRenderer)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(layer, "Render Layer")
    REFLECT_PROPERTY_RANGE(opacity, "Opacity", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(pixel_perfect, "Pixel Perfect")
    REFLECT_PROPERTY_RANGE(tint_r, "Tint Red", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(tint_g, "Tint Green", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(tint_b, "Tint Blue", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(tint_a, "Tint Alpha", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// Rigidbody reflection
REFLECT_TYPE_BEGIN(engine::physics::Rigidbody)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(body_type, "Body Type")
    REFLECT_PROPERTY_RANGE(mass, "Mass", 0.0f, 1000.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(linear_damping, "Linear Damping", 0.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(angular_damping, "Angular Damping", 0.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(gravity_scale, "Gravity Scale", -10.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY(lock_rotation, "Lock Rotation")
    REFLECT_PROPERTY(lock_position_x, "Lock Position X")
    REFLECT_PROPERTY(lock_position_y, "Lock Position Y")
    REFLECT_PROPERTY(bullet, "Continuous Collision")
    REFLECT_PROPERTY(allow_sleep, "Allow Sleep")
    REFLECT_PROPERTY(awake, "Start Awake")
    REFLECT_PROPERTY(initial_velocity_x, "Initial Velocity X")
    REFLECT_PROPERTY(initial_velocity_y, "Initial Velocity Y")
    REFLECT_PROPERTY(initial_angular_velocity, "Initial Angular Velocity")
REFLECT_TYPE_END()

// ColliderMaterial reflection
REFLECT_TYPE_BEGIN(engine::physics::ColliderMaterial)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// BoxCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::BoxCollider)
    REFLECT_PROPERTY(material.enabled, "Enabled")
    REFLECT_PROPERTY(material.is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(width, "Width", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(height, "Height", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(rotation, "Rotation", -180.0f, 180.0f, 1.0f)
    REFLECT_PROPERTY_RANGE(material.density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(material.friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(material.restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// CapsuleCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::CapsuleCollider)
    REFLECT_PROPERTY(material.enabled, "Enabled")
    REFLECT_PROPERTY(material.is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(length, "Length", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(radius, "Radius", 0.1f, 50.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(rotation, "Rotation", -180.0f, 180.0f, 1.0f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(material.density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(material.friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(material.restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// CircleCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::CircleCollider)
    REFLECT_PROPERTY(material.enabled, "Enabled")
    REFLECT_PROPERTY(material.is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(radius, "Radius", 0.1f, 50.0f, 0.1f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(material.density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(material.friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(material.restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// DynamicCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::DynamicCollider)
    REFLECT_PROPERTY(material.enabled, "Enabled")
    REFLECT_PROPERTY(material.is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(simplification, "Simplification", 0.0f, 5.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(min_contour_area, "Min Contour Area", 0.0f, 100.0f, 1.0f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(material.density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(material.friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(material.restitution, "Restitution", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(generated, "Generated")
    REFLECT_PROPERTY(triangle_count, "Triangle Count")
REFLECT_TYPE_END()

// SimSurface reflection
REFLECT_TYPE_BEGIN(engine::simulation::SimSurface)
    REFLECT_PROPERTY(simulation_enabled, "Simulation Enabled")
    REFLECT_PROPERTY_RANGE(simulation_speed, "Simulation Speed", 0.0f, 10.0f, 0.1f)
    REFLECT_PROPERTY(material_set, "Material Set")
    REFLECT_PROPERTY_RANGE(chunk_size_x, "Chunk Size X", 8, 128, 8)
    REFLECT_PROPERTY_RANGE(chunk_size_y, "Chunk Size Y", 8, 128, 8)
    REFLECT_PROPERTY(generate_colliders, "Generate Colliders")
    REFLECT_PROPERTY(initialized, "Initialized")
REFLECT_TYPE_END()

// --- UI Components ---

// UIInteractable reflection
REFLECT_TYPE_BEGIN(engine::ui::UIInteractable)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(interactable, "Interactable")
    REFLECT_PROPERTY_ENUM(transition_mode, "Transition Mode", "None", "Color Tint", "Sprite Swap")
    // Color tint mode
    REFLECT_PROPERTY(normal_color, "Normal Color")
    REFLECT_PROPERTY(hovered_color, "Hovered Color")
    REFLECT_PROPERTY(pressed_color, "Pressed Color")
    REFLECT_PROPERTY(disabled_color, "Disabled Color")
    // Sprite swap mode
    REFLECT_PROPERTY(normal_sprite, "Normal Sprite")
    REFLECT_PROPERTY(hovered_sprite, "Hovered Sprite")
    REFLECT_PROPERTY(pressed_sprite, "Pressed Sprite")
    REFLECT_PROPERTY(disabled_sprite, "Disabled Sprite")
REFLECT_TYPE_END()

// Button reflection
// Note: 'enabled' is on UIInteractable, not duplicated here
REFLECT_TYPE_BEGIN(engine::ui::Button)
    REFLECT_PROPERTY(click_sound, "Click Sound")
REFLECT_TYPE_END()

// Slider reflection
// Note: 'enabled' is on UIInteractable, not duplicated here
REFLECT_TYPE_BEGIN(engine::ui::Slider)
    REFLECT_PROPERTY_RANGE(value, "Value", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(min_value, "Min Value")
    REFLECT_PROPERTY(max_value, "Max Value")
    REFLECT_PROPERTY(whole_numbers, "Whole Numbers")
    REFLECT_PROPERTY_ENUM(direction, "Direction", "Left to Right", "Right to Left", "Bottom to Top", "Top to Bottom")
    REFLECT_PROPERTY(fill_rect, "Fill Rect")
    REFLECT_PROPERTY(handle, "Handle")
REFLECT_TYPE_END()

// Checkbox reflection
// Note: 'enabled' is on UIInteractable, not duplicated here
REFLECT_TYPE_BEGIN(engine::ui::Checkbox)
    REFLECT_PROPERTY(checked, "Checked")
    REFLECT_PROPERTY(toggle_group, "Toggle Group")
    REFLECT_PROPERTY(checkmark, "Checkmark")
REFLECT_TYPE_END()

// Panel reflection
REFLECT_TYPE_BEGIN(engine::ui::Panel)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(blocks_raycast, "Blocks Raycast")
    REFLECT_PROPERTY(draggable, "Draggable")
    REFLECT_PROPERTY(drag_handle, "Drag Handle")
    REFLECT_PROPERTY(close_button, "Close Button")
REFLECT_TYPE_END()

// ScrollView reflection
REFLECT_TYPE_BEGIN(engine::ui::ScrollView)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(horizontal, "Horizontal")
    REFLECT_PROPERTY(vertical, "Vertical")
    REFLECT_PROPERTY_RANGE(scroll_sensitivity, "Scroll Sensitivity", 1.0f, 100.0f, 1.0f)
    REFLECT_PROPERTY(inertia, "Inertia")
    REFLECT_PROPERTY_RANGE(deceleration_rate, "Deceleration Rate", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(elastic, "Elastic")
    REFLECT_PROPERTY_RANGE(elasticity, "Elasticity", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_ENUM(horizontal_scrollbar_visibility, "H Scrollbar Visibility", "Permanent", "Auto Hide", "Auto Hide & Expand")
    REFLECT_PROPERTY_ENUM(vertical_scrollbar_visibility, "V Scrollbar Visibility", "Permanent", "Auto Hide", "Auto Hide & Expand")
    REFLECT_PROPERTY(viewport, "Viewport")
    REFLECT_PROPERTY(content, "Content")
    REFLECT_PROPERTY(horizontal_scrollbar, "H Scrollbar")
    REFLECT_PROPERTY(vertical_scrollbar, "V Scrollbar")
REFLECT_TYPE_END()

// Dropdown reflection
// Note: 'enabled' is on UIInteractable, not duplicated here
REFLECT_TYPE_BEGIN(engine::ui::Dropdown)
    REFLECT_PROPERTY(selected_index, "Selected Index")
    REFLECT_PROPERTY(options, "Options")
    REFLECT_PROPERTY(max_visible_items, "Max Visible Items")
    REFLECT_PROPERTY_RANGE(item_height, "Item Height", 16.0f, 64.0f, 1.0f)
    REFLECT_PROPERTY(selected_text, "Selected Text")
    REFLECT_PROPERTY(arrow, "Arrow")
    REFLECT_PROPERTY(options_panel, "Options Panel")
    REFLECT_PROPERTY(options_scrollview, "Options ScrollView")
    REFLECT_PROPERTY(options_content, "Options Content")
REFLECT_TYPE_END()

// AudioSource reflection
REFLECT_TYPE_BEGIN(engine::audio::AudioSource)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(clip_path, "Audio Clip")
    REFLECT_PROPERTY_RANGE(volume, "Volume", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(pitch, "Pitch", 0.1f, 3.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(pan, "Pan", -1.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(loop, "Loop")
    REFLECT_PROPERTY(play_on_start, "Play On Start")
    REFLECT_PROPERTY(spatial, "Spatial")
    REFLECT_PROPERTY_RANGE(min_distance, "Min Distance", 1.0f, 1000.0f, 1.0f)
    REFLECT_PROPERTY_RANGE(max_distance, "Max Distance", 10.0f, 5000.0f, 10.0f)
    REFLECT_PROPERTY_ENUM(channel_group, "Channel", "Master", "SFX", "Music", "UI")
REFLECT_TYPE_END()

// AudioListener reflection
REFLECT_TYPE_BEGIN(engine::audio::AudioListener)
    REFLECT_PROPERTY(enabled, "Enabled")
REFLECT_TYPE_END()

namespace engine::reflection {

void init_engine_reflections() {
    engine::Logger::instance().info("Reflection", "Initializing engine component reflections...");

    // Register all engine components from the central list (EngineComponentList.h)
    #define REGISTER_REFLECTION(T) register_type<T>();
    ENGINE_COMPONENT_LIST(REGISTER_REFLECTION)
    #undef REGISTER_REFLECTION

    auto count = TypeRegistry::instance().all_types().size();
    engine::Logger::instance().info("Reflection", "Finished initializing reflections. Total types registered: %zu", count);
}

} // namespace engine::reflection
