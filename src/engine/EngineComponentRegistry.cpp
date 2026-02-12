#include "EngineComponentRegistry.h"
#include "engine/prefab/ComponentRegistry.h"
#include "engine/core/Transform.h"
#include "engine/render/Camera2D.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/SimSurface.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/gameplay/PlayerController.h"
#include "engine/gameplay/CameraFollower.h"
#include <nlohmann/json.hpp>

namespace engine {

void register_engine_components(engine::prefab::ComponentRegistry& registry) {
    // Transform: position, rotation, scale
    registry.register_component<Transform>("engine::Transform", [](const nlohmann::json& j) {
        Transform t;
        if (j.contains("x")) t.x = j["x"].get<float>();
        if (j.contains("y")) t.y = j["y"].get<float>();
        if (j.contains("rotation")) t.rotation = j["rotation"].get<float>();

        // Prefer snake_case (matches C++ member names and reflection)
        if (j.contains("scale_x")) t.scale_x = j["scale_x"].get<float>();
        if (j.contains("scale_y")) t.scale_y = j["scale_y"].get<float>();

        // Backward compatibility with old camelCase format
        if (j.contains("scaleX")) t.scale_x = j["scaleX"].get<float>();
        if (j.contains("scaleY")) t.scale_y = j["scaleY"].get<float>();

        // World transforms are computed at runtime, not serialized
        return t;
    });

    // Camera2D: position, zoom, and camera parameters
    registry.register_component<render::Camera2D>("engine::render::Camera2D", [](const nlohmann::json& j) {
        render::Camera2D cam;
        if (j.contains("x")) cam.x = j["x"].get<float>();
        if (j.contains("y")) cam.y = j["y"].get<float>();
        if (j.contains("zoom")) cam.zoom = j["zoom"].get<float>();

        // Prefer snake_case (matches C++ member names and reflection)
        if (j.contains("min_zoom")) cam.min_zoom = j["min_zoom"].get<float>();
        if (j.contains("max_zoom")) cam.max_zoom = j["max_zoom"].get<float>();
        if (j.contains("smoothing")) cam.smoothing = j["smoothing"].get<float>();

        // Backward compatibility with old camelCase format
        if (j.contains("minZoom")) cam.min_zoom = j["minZoom"].get<float>();
        if (j.contains("maxZoom")) cam.max_zoom = j["maxZoom"].get<float>();

        return cam;
    });

    // Animator: animation playback state
    registry.register_component<animation::Animator>("engine::animation::Animator", [](const nlohmann::json& j) {
        animation::Animator anim;
        if (j.contains("enabled")) anim.enabled = j["enabled"].get<bool>();
        if (j.contains("playing")) anim.playing = j["playing"].get<bool>();

        // Prefer snake_case (matches C++ member names and reflection)
        if (j.contains("current_clip")) {
            anim.current_clip = j["current_clip"].get<std::string>();
        }

        // Backward compatibility with old camelCase format
        if (j.contains("currentClip")) {
            anim.current_clip = j["currentClip"].get<std::string>();
        }

        // Note: Animation clips are typically loaded through the asset system
        return anim;
    });

    // PixelGridComponent: references pixel grid data
    registry.register_component<simulation::PixelGridComponent>("engine::simulation::PixelGridComponent", [](const nlohmann::json& j) {
        simulation::PixelGridComponent comp;
        if (j.contains("enabled")) comp.enabled = j["enabled"].get<bool>();
        if (j.contains("pixel_grid_path")) comp.pixel_grid_path = j["pixel_grid_path"].get<std::string>();
        if (j.contains("width")) comp.width = j["width"].get<int>();
        if (j.contains("height")) comp.height = j["height"].get<int>();
        if (j.contains("origin_x")) comp.origin_x = j["origin_x"].get<int>();
        if (j.contains("origin_y")) comp.origin_y = j["origin_y"].get<int>();
        if (j.contains("loaded")) comp.loaded = j["loaded"].get<bool>();
        return comp;
    });

    // PixelGridRenderer: renders pixel grid
    registry.register_component<render::PixelGridRenderer>("engine::render::PixelGridRenderer", [](const nlohmann::json& j) {
        render::PixelGridRenderer renderer;
        if (j.contains("enabled")) renderer.enabled = j["enabled"].get<bool>();
        if (j.contains("layer")) renderer.layer = j["layer"].get<int>();
        if (j.contains("opacity")) renderer.opacity = j["opacity"].get<float>();
        if (j.contains("pixel_perfect")) renderer.pixel_perfect = j["pixel_perfect"].get<bool>();
        if (j.contains("tint_r")) renderer.tint_r = j["tint_r"].get<float>();
        if (j.contains("tint_g")) renderer.tint_g = j["tint_g"].get<float>();
        if (j.contains("tint_b")) renderer.tint_b = j["tint_b"].get<float>();
        if (j.contains("tint_a")) renderer.tint_a = j["tint_a"].get<float>();
        return renderer;
    });

    // Rigidbody: Box2D physics body
    registry.register_component<physics::Rigidbody>("engine::physics::Rigidbody", [](const nlohmann::json& j) {
        physics::Rigidbody rb;
        if (j.contains("enabled")) rb.enabled = j["enabled"].get<bool>();

        // Body type (enum as int)
        if (j.contains("body_type")) {
            int type = j["body_type"].get<int>();
            rb.body_type = static_cast<physics::BodyType>(type);
        }

        if (j.contains("mass")) rb.mass = j["mass"].get<float>();
        if (j.contains("linear_damping")) rb.linear_damping = j["linear_damping"].get<float>();
        if (j.contains("angular_damping")) rb.angular_damping = j["angular_damping"].get<float>();
        if (j.contains("gravity_scale")) rb.gravity_scale = j["gravity_scale"].get<float>();
        if (j.contains("lock_rotation")) rb.lock_rotation = j["lock_rotation"].get<bool>();
        if (j.contains("lock_position_x")) rb.lock_position_x = j["lock_position_x"].get<bool>();
        if (j.contains("lock_position_y")) rb.lock_position_y = j["lock_position_y"].get<bool>();
        if (j.contains("bullet")) rb.bullet = j["bullet"].get<bool>();
        if (j.contains("allow_sleep")) rb.allow_sleep = j["allow_sleep"].get<bool>();
        if (j.contains("awake")) rb.awake = j["awake"].get<bool>();
        if (j.contains("initial_velocity_x")) rb.initial_velocity_x = j["initial_velocity_x"].get<float>();
        if (j.contains("initial_velocity_y")) rb.initial_velocity_y = j["initial_velocity_y"].get<float>();
        if (j.contains("initial_angular_velocity")) rb.initial_angular_velocity = j["initial_angular_velocity"].get<float>();

        return rb;
    });

    // BoxCollider
    registry.register_component<physics::BoxCollider>("engine::physics::BoxCollider", [](const nlohmann::json& j) {
        physics::BoxCollider collider;
        if (j.contains("enabled")) collider.enabled = j["enabled"].get<bool>();
        if (j.contains("is_trigger")) collider.is_trigger = j["is_trigger"].get<bool>();
        if (j.contains("width")) collider.width = j["width"].get<float>();
        if (j.contains("height")) collider.height = j["height"].get<float>();
        if (j.contains("offset_x")) collider.offset_x = j["offset_x"].get<float>();
        if (j.contains("offset_y")) collider.offset_y = j["offset_y"].get<float>();
        if (j.contains("rotation")) collider.rotation = j["rotation"].get<float>();
        if (j.contains("density")) collider.density = j["density"].get<float>();
        if (j.contains("friction")) collider.friction = j["friction"].get<float>();
        if (j.contains("restitution")) collider.restitution = j["restitution"].get<float>();
        return collider;
    });

    // CapsuleCollider
    registry.register_component<physics::CapsuleCollider>("engine::physics::CapsuleCollider", [](const nlohmann::json& j) {
        physics::CapsuleCollider collider;
        if (j.contains("enabled")) collider.enabled = j["enabled"].get<bool>();
        if (j.contains("is_trigger")) collider.is_trigger = j["is_trigger"].get<bool>();
        if (j.contains("length")) collider.length = j["length"].get<float>();
        if (j.contains("radius")) collider.radius = j["radius"].get<float>();
        if (j.contains("rotation")) collider.rotation = j["rotation"].get<float>();
        if (j.contains("offset_x")) collider.offset_x = j["offset_x"].get<float>();
        if (j.contains("offset_y")) collider.offset_y = j["offset_y"].get<float>();
        if (j.contains("density")) collider.density = j["density"].get<float>();
        if (j.contains("friction")) collider.friction = j["friction"].get<float>();
        if (j.contains("restitution")) collider.restitution = j["restitution"].get<float>();
        return collider;
    });

    // CircleCollider
    registry.register_component<physics::CircleCollider>("engine::physics::CircleCollider", [](const nlohmann::json& j) {
        physics::CircleCollider collider;
        if (j.contains("enabled")) collider.enabled = j["enabled"].get<bool>();
        if (j.contains("is_trigger")) collider.is_trigger = j["is_trigger"].get<bool>();
        if (j.contains("radius")) collider.radius = j["radius"].get<float>();
        if (j.contains("offset_x")) collider.offset_x = j["offset_x"].get<float>();
        if (j.contains("offset_y")) collider.offset_y = j["offset_y"].get<float>();
        if (j.contains("density")) collider.density = j["density"].get<float>();
        if (j.contains("friction")) collider.friction = j["friction"].get<float>();
        if (j.contains("restitution")) collider.restitution = j["restitution"].get<float>();
        return collider;
    });

    // DynamicCollider
    registry.register_component<physics::DynamicCollider>("engine::physics::DynamicCollider", [](const nlohmann::json& j) {
        physics::DynamicCollider collider;
        if (j.contains("enabled")) collider.enabled = j["enabled"].get<bool>();
        if (j.contains("is_trigger")) collider.is_trigger = j["is_trigger"].get<bool>();
        if (j.contains("simplification")) collider.simplification = j["simplification"].get<float>();
        if (j.contains("min_contour_area")) collider.min_contour_area = j["min_contour_area"].get<float>();
        if (j.contains("offset_x")) collider.offset_x = j["offset_x"].get<float>();
        if (j.contains("offset_y")) collider.offset_y = j["offset_y"].get<float>();
        if (j.contains("density")) collider.density = j["density"].get<float>();
        if (j.contains("friction")) collider.friction = j["friction"].get<float>();
        if (j.contains("restitution")) collider.restitution = j["restitution"].get<float>();
        if (j.contains("generated")) collider.generated = j["generated"].get<bool>();
        if (j.contains("triangle_count")) collider.triangle_count = j["triangle_count"].get<int>();
        return collider;
    });

    // PlayerController
    registry.register_component<gameplay::PlayerController>("engine::gameplay::PlayerController", [](const nlohmann::json& j) {
        gameplay::PlayerController ctrl;
        if (j.contains("move_accel")) ctrl.move_accel = j["move_accel"].get<float>();
        if (j.contains("max_move_speed")) ctrl.max_move_speed = j["max_move_speed"].get<float>();
        if (j.contains("jump_velocity")) ctrl.jump_velocity = j["jump_velocity"].get<float>();
        if (j.contains("friction")) ctrl.friction = j["friction"].get<float>();
        // Runtime state (move_dir, jump_pressed) not serialized
        return ctrl;
    });

    // CameraFollower
    registry.register_component<gameplay::CameraFollower>("engine::gameplay::CameraFollower", [](const nlohmann::json& j) {
        gameplay::CameraFollower follower;
        if (j.contains("offset_y_fraction")) follower.offset_y_fraction = j["offset_y_fraction"].get<float>();
        if (j.contains("active")) follower.active = j["active"].get<bool>();
        return follower;
    });

    // SimSurface: pixel simulation surface configuration
    // Requires a PixelGridComponent on the same entity for grid dimensions and initial data
    registry.register_component<simulation::SimSurface>("engine::simulation::SimSurface", [](const nlohmann::json& j) {
        simulation::SimSurface surface;
        if (j.contains("simulation_enabled")) surface.simulation_enabled = j["simulation_enabled"].get<bool>();
        if (j.contains("simulation_speed")) surface.simulation_speed = j["simulation_speed"].get<float>();
        if (j.contains("material_set")) surface.material_set = j["material_set"].get<std::string>();
        if (j.contains("chunk_size_x")) surface.chunk_size_x = j["chunk_size_x"].get<int>();
        if (j.contains("chunk_size_y")) surface.chunk_size_y = j["chunk_size_y"].get<int>();
        if (j.contains("generate_colliders")) surface.generate_colliders = j["generate_colliders"].get<bool>();
        return surface;
    });

    // Note: PixelBody is not registered here because it's too complex for simple JSON deserialization
    // It requires pixel buffer data and physics setup. Consider custom prefab handling for PixelBody.
}

} // namespace engine
