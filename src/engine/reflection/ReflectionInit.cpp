#include "ReflectionInit.h"
#include "TypeRegistry.h"
#include "engine/core/Transform.h"
#include "engine/render/Camera2D.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/gameplay/PlayerController.h"
#include "engine/gameplay/CameraFollower.h"
#include "engine/simulation/SimSurface.h"
#include "engine/core/Logger.h"
#include "ReflectionMacros.h"

// Include the reflection definitions directly
// These define the TypeReflector specializations

// Transform reflection
REFLECT_TYPE_BEGIN(engine::Transform)
    REFLECT_PROPERTY(x, "X Position")
    REFLECT_PROPERTY(y, "Y Position")
    REFLECT_PROPERTY(rotation, "Rotation")
    REFLECT_PROPERTY(scale_x, "Scale X")
    REFLECT_PROPERTY(scale_y, "Scale Y")
REFLECT_TYPE_END()

// Camera2D reflection
REFLECT_TYPE_BEGIN(engine::render::Camera2D)
    REFLECT_PROPERTY(x, "X Position")
    REFLECT_PROPERTY(y, "Y Position")
    REFLECT_PROPERTY_RANGE(zoom, "Zoom", 0.1f, 10.0f, 0.1f)
    REFLECT_PROPERTY_RANGE(min_zoom, "Min Zoom", 0.01f, 1.0f, 0.01f)
    REFLECT_PROPERTY_RANGE(max_zoom, "Max Zoom", 1.0f, 20.0f, 0.5f)
    REFLECT_PROPERTY_RANGE(smoothing, "Smoothing", 0.0f, 20.0f, 0.5f)
REFLECT_TYPE_END()

// Animator reflection
REFLECT_TYPE_BEGIN(engine::animation::Animator)
    REFLECT_PROPERTY(enabled, "Enabled")
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

// PlayerController reflection
REFLECT_TYPE_BEGIN(engine::gameplay::PlayerController)
    REFLECT_PROPERTY_RANGE(move_accel, "Move Acceleration", 0.0f, 5000.0f, 10.0f)
    REFLECT_PROPERTY_RANGE(max_move_speed, "Max Move Speed", 0.0f, 500.0f, 5.0f)
    REFLECT_PROPERTY_RANGE(jump_velocity, "Jump Velocity", -500.0f, 0.0f, 5.0f)
    REFLECT_PROPERTY_RANGE(friction, "Friction", 0.0f, 50.0f, 0.5f)
    REFLECT_PROPERTY(move_dir, "Move Direction")
    REFLECT_PROPERTY(jump_pressed, "Jump Pressed")
REFLECT_TYPE_END()

// CameraFollower reflection
REFLECT_TYPE_BEGIN(engine::gameplay::CameraFollower)
    REFLECT_PROPERTY_RANGE(offset_y_fraction, "Vertical Offset", 0.0f, 1.0f, 0.01f)
    REFLECT_PROPERTY(active, "Active")
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
    auto& registry = TypeRegistry::instance();

    engine::Logger::instance().info("Reflection", "Initializing engine component reflections...");

    // Register Transform
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::Transform",
            sizeof(engine::Transform),
            std::type_index(typeid(engine::Transform))
        );
        TypeReflector<engine::Transform>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered Transform with %zu properties", prop_count);
    }

    // Register Camera2D
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::render::Camera2D",
            sizeof(engine::render::Camera2D),
            std::type_index(typeid(engine::render::Camera2D))
        );
        TypeReflector<engine::render::Camera2D>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered Camera2D with %zu properties", prop_count);
    }

    // Register Animator
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::animation::Animator",
            sizeof(engine::animation::Animator),
            std::type_index(typeid(engine::animation::Animator))
        );
        TypeReflector<engine::animation::Animator>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered Animator with %zu properties", prop_count);
    }

    // Register PixelGridComponent
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::simulation::PixelGridComponent",
            sizeof(engine::simulation::PixelGridComponent),
            std::type_index(typeid(engine::simulation::PixelGridComponent))
        );
        TypeReflector<engine::simulation::PixelGridComponent>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered PixelGridComponent with %zu properties", prop_count);
    }

    // Register PixelGridRenderer
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::render::PixelGridRenderer",
            sizeof(engine::render::PixelGridRenderer),
            std::type_index(typeid(engine::render::PixelGridRenderer))
        );
        TypeReflector<engine::render::PixelGridRenderer>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered PixelGridRenderer with %zu properties", prop_count);
    }

    // Register Rigidbody
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::physics::Rigidbody",
            sizeof(engine::physics::Rigidbody),
            std::type_index(typeid(engine::physics::Rigidbody))
        );
        TypeReflector<engine::physics::Rigidbody>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered Rigidbody with %zu properties", prop_count);
    }

    // Register BoxCollider
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::physics::BoxCollider",
            sizeof(engine::physics::BoxCollider),
            std::type_index(typeid(engine::physics::BoxCollider))
        );
        TypeReflector<engine::physics::BoxCollider>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered BoxCollider with %zu properties", prop_count);
    }

    // Register CapsuleCollider
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::physics::CapsuleCollider",
            sizeof(engine::physics::CapsuleCollider),
            std::type_index(typeid(engine::physics::CapsuleCollider))
        );
        TypeReflector<engine::physics::CapsuleCollider>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered CapsuleCollider with %zu properties", prop_count);
    }

    // Register CircleCollider
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::physics::CircleCollider",
            sizeof(engine::physics::CircleCollider),
            std::type_index(typeid(engine::physics::CircleCollider))
        );
        TypeReflector<engine::physics::CircleCollider>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered CircleCollider with %zu properties", prop_count);
    }

    // Register DynamicCollider
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::physics::DynamicCollider",
            sizeof(engine::physics::DynamicCollider),
            std::type_index(typeid(engine::physics::DynamicCollider))
        );
        TypeReflector<engine::physics::DynamicCollider>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered DynamicCollider with %zu properties", prop_count);
    }

    // Register PlayerController
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::gameplay::PlayerController",
            sizeof(engine::gameplay::PlayerController),
            std::type_index(typeid(engine::gameplay::PlayerController))
        );
        TypeReflector<engine::gameplay::PlayerController>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered PlayerController with %zu properties", prop_count);
    }

    // Register CameraFollower
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::gameplay::CameraFollower",
            sizeof(engine::gameplay::CameraFollower),
            std::type_index(typeid(engine::gameplay::CameraFollower))
        );
        TypeReflector<engine::gameplay::CameraFollower>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered CameraFollower with %zu properties", prop_count);
    }

    // Register SimSurface
    {
        auto info = std::make_unique<TypeInfo>(
            "engine::simulation::SimSurface",
            sizeof(engine::simulation::SimSurface),
            std::type_index(typeid(engine::simulation::SimSurface))
        );
        TypeReflector<engine::simulation::SimSurface>::reflect(*info);
        size_t prop_count = info->properties().size();
        registry.register_type(std::move(info));
        engine::Logger::instance().info("Reflection", "Registered SimSurface with %zu properties", prop_count);
    }

    auto all_types = registry.get_all_types();
    engine::Logger::instance().info("Reflection", "Finished initializing reflections. Total types registered: %zu", all_types.size());
}

} // namespace engine::reflection
