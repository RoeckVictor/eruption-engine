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
    REFLECT_PROPERTY(align, "Alignment")
    REFLECT_PROPERTY_RANGE(line_height, "Line Height", 0.5f, 3.0f, 0.1f)
    REFLECT_PROPERTY(bold, "Bold")
    REFLECT_PROPERTY(italic, "Italic")
    REFLECT_PROPERTY(underline, "Underline")
    REFLECT_PROPERTY(max_width, "Max Width")
REFLECT_TYPE_END()

// Animator reflection
REFLECT_TYPE_BEGIN(engine::animation::Animator)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(current_clip, "Current Clip")
    REFLECT_PROPERTY(playing, "Playing")
REFLECT_TYPE_END()

// PixelGridComponent reflection
REFLECT_TYPE_BEGIN(engine::simulation::PixelGridComponent)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(pixel_grid_path, "Pixel Grid Path")
    REFLECT_PROPERTY(width, "Width")
    REFLECT_PROPERTY(height, "Height")
    REFLECT_PROPERTY(origin_x, "Origin X")
    REFLECT_PROPERTY(origin_y, "Origin Y")
    REFLECT_PROPERTY(loaded, "Loaded")
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

// BoxCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::BoxCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(width, "Width", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(height, "Height", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(rotation, "Rotation", -180.0f, 180.0f, 1.0f)
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// CapsuleCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::CapsuleCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(length, "Length", 0.1f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(radius, "Radius", 0.1f, 50.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(rotation, "Rotation", -180.0f, 180.0f, 1.0f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// CircleCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::CircleCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(radius, "Radius", 0.1f, 50.0f, 0.1f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
REFLECT_TYPE_END()

// DynamicCollider reflection
REFLECT_TYPE_BEGIN(engine::physics::DynamicCollider)
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(is_trigger, "Is Trigger")
    REFLECT_PROPERTY_RANGE(simplification, "Simplification", 0.0f, 5.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(min_contour_area, "Min Contour Area", 0.0f, 100.0f, 1.0f)
    REFLECT_PROPERTY(offset_x, "Offset X")
    REFLECT_PROPERTY(offset_y, "Offset Y")
    REFLECT_PROPERTY_RANGE(density, "Density", 0.0f, 100.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(restitution, "Restitution", 0.0f, 1.0f, 0.01f)
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
