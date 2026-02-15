#include "Box2DPhysicsSystem.h"
#include "engine/core/Engine.h"
#include "engine/core/Transform.h"
#include "engine/core/Logger.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/simulation/PixelGridComponent.h"
#include "PixelGridLoaderSystem.h"
#include "engine/core/EngineContext.h"
#include "editor/core/EditorComponents.h"
#include <box2d/box2d.h>
#include <cmath>

namespace engine {

bool Box2DPhysicsSystem::init(Engine& engine) {
    auto& ctx = engine.app_context<EngineContext>();
    m_registry = &ctx.registry;
    m_physics_world = &ctx.physics_world;

    // Listen for Rigidbody component destruction to clean up Box2D bodies
    m_registry->on_destroy<physics::Rigidbody>().connect<&Box2DPhysicsSystem::on_rigidbody_destroyed>(this);

    Logger::instance().info("Box2DPhysics", "Box2DPhysicsSystem initialized");
    return true;
}

void Box2DPhysicsSystem::shutdown() {
    // Disconnect listener
    if (m_registry) {
        m_registry->on_destroy<physics::Rigidbody>().disconnect(this);
    }
}

void Box2DPhysicsSystem::fixed_update(Engine& /*engine*/, float /*dt*/) {
    // Step 1: Create bodies for new Rigidbody entities (entities without b2BodyId)
    {
        auto view = m_registry->view<physics::Rigidbody, Transform>();
        for (auto entity : view) {
            // Skip disabled entities
            if (m_registry->all_of<editor::EntityInfo>(entity)) {
                if (!m_registry->get<editor::EntityInfo>(entity).enabled_in_hierarchy) {
                    continue;
                }
            }

            auto& rb = view.get<physics::Rigidbody>(entity);
            auto& transform = view.get<Transform>(entity);

            // Skip if body already created or rigidbody disabled
            if (!rb.enabled || b2Body_IsValid(rb.body_id)) {
                continue;
            }

            // Create Box2D body for this entity
            create_body_for_entity(entity, rb, transform);
        }
    }

    // Step 2: Sync kinematic bodies (Transform → Physics) before physics step
    sync_kinematic_bodies();

    // Step 3: Create collider shapes for bodies that need them
    {
        // BoxCollider
        auto box_view = m_registry->view<physics::Rigidbody, physics::BoxCollider, Transform>();
        for (auto entity : box_view) {
            auto& rb = box_view.get<physics::Rigidbody>(entity);
            auto& collider = box_view.get<physics::BoxCollider>(entity);
            auto& transform = box_view.get<Transform>(entity);

            // Skip if body not created yet or collider disabled
            if (!b2Body_IsValid(rb.body_id) || !collider.enabled) {
                continue;
            }

            // Skip if shape already created
            if (b2Shape_IsValid(collider.shape_id)) {
                continue;
            }

            // Create box shape
            create_box_collider(rb.body_id, collider, transform);
        }

        // CircleCollider
        auto circle_view = m_registry->view<physics::Rigidbody, physics::CircleCollider, Transform>();
        for (auto entity : circle_view) {
            auto& rb = circle_view.get<physics::Rigidbody>(entity);
            auto& collider = circle_view.get<physics::CircleCollider>(entity);
            auto& transform = circle_view.get<Transform>(entity);

            if (!b2Body_IsValid(rb.body_id) || !collider.enabled) {
                continue;
            }

            if (b2Shape_IsValid(collider.shape_id)) {
                continue;
            }

            create_circle_collider(rb.body_id, collider, transform);
        }

        // CapsuleCollider
        auto capsule_view = m_registry->view<physics::Rigidbody, physics::CapsuleCollider, Transform>();
        for (auto entity : capsule_view) {
            auto& rb = capsule_view.get<physics::Rigidbody>(entity);
            auto& collider = capsule_view.get<physics::CapsuleCollider>(entity);
            auto& transform = capsule_view.get<Transform>(entity);

            if (!b2Body_IsValid(rb.body_id) || !collider.enabled) {
                continue;
            }

            // Check if shapes already created (capsule has multiple shapes)
            if (!collider.shape_ids.empty()) {
                continue;
            }

            create_capsule_collider(rb.body_id, collider, transform);
        }

        // DynamicCollider (auto-generated from pixel grids)
        auto dynamic_view = m_registry->view<physics::Rigidbody, physics::DynamicCollider, simulation::PixelGridComponent>();
        for (auto entity : dynamic_view) {
            auto& rb = dynamic_view.get<physics::Rigidbody>(entity);
            auto& collider = dynamic_view.get<physics::DynamicCollider>(entity);
            auto& grid_comp = dynamic_view.get<simulation::PixelGridComponent>(entity);

            if (!b2Body_IsValid(rb.body_id) || !collider.enabled || !grid_comp.loaded) {
                continue;
            }

            // Skip if already generated
            if (collider.generated && !collider.shape_ids.empty()) {
                continue;
            }

            // Generate collision mesh from pixel grid
            create_dynamic_collider(entity, rb.body_id, collider);
        }
    }

    // Step 4: Apply mass overrides for dynamic bodies that have shapes
    {
        auto view = m_registry->view<physics::Rigidbody>();
        for (auto entity : view) {
            auto& rb = view.get<physics::Rigidbody>(entity);
            if (rb.body_type != physics::BodyType::Dynamic) continue;
            if (!b2Body_IsValid(rb.body_id)) continue;
            if (rb.mass <= 0.0f || rb.mass_applied) continue;

            // Only apply after at least one shape exists
            if (b2Body_GetShapeCount(rb.body_id) == 0) continue;

            b2MassData mass_data = b2Body_GetMassData(rb.body_id);
            mass_data.mass = rb.mass;
            b2Body_SetMassData(rb.body_id, mass_data);
            rb.mass_applied = true;
        }
    }
}

void Box2DPhysicsSystem::create_body_for_entity(entt::entity entity, physics::Rigidbody& rb, const Transform& transform) {
    // Convert body type to Box2D type
    b2BodyType b2_type = b2_dynamicBody;
    switch (rb.body_type) {
        case physics::BodyType::Static:    b2_type = b2_staticBody; break;
        case physics::BodyType::Kinematic: b2_type = b2_kinematicBody; break;
        case physics::BodyType::Dynamic:   b2_type = b2_dynamicBody; break;
    }

    // Create body definition
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.type = b2_type;

    // Set position (convert from pixels to meters)
    b2Vec2 pos = m_physics_world->pixels_to_meters(transform.x, transform.y);
    body_def.position = pos;

    // Set rotation (convert degrees to radians, then to b2Rot)
    float angle_rad = transform.rotation * (B2_PI / 180.0f);
    body_def.rotation = b2MakeRot(angle_rad);

    // Apply rigidbody properties
    body_def.linearDamping = rb.linear_damping;
    body_def.angularDamping = rb.angular_damping;
    body_def.gravityScale = rb.gravity_scale;
    body_def.fixedRotation = rb.lock_rotation;
    body_def.enableSleep = rb.allow_sleep;
    body_def.isAwake = rb.awake;
    body_def.isBullet = rb.bullet;

    // Create body in Box2D world
    rb.body_id = b2CreateBody(m_physics_world->world_id(), &body_def);

    // Apply initial velocity for dynamic bodies
    if (rb.body_type == physics::BodyType::Dynamic) {
        if (rb.initial_velocity_x != 0.0f || rb.initial_velocity_y != 0.0f) {
            m_physics_world->set_body_linear_velocity(rb.body_id, rb.initial_velocity_x, rb.initial_velocity_y);
        }
        if (rb.initial_angular_velocity != 0.0f) {
            m_physics_world->set_body_angular_velocity(rb.body_id, rb.initial_angular_velocity);
        }
    }

    // Mass override will be applied after shapes are created (see fixed_update step 4)

    // Apply position locks (implemented via constraints in Box2D)
    // Note: Box2D doesn't have built-in axis locking, we'll handle this in sync
    // For now, just log if locks are requested
    if (rb.lock_position_x || rb.lock_position_y) {
        // Will be handled in sync_kinematic_bodies by ignoring velocity on locked axes
        Logger::instance().info("Box2DPhysics", "Entity has position locks (x=%d, y=%d)",
                                 rb.lock_position_x, rb.lock_position_y);
    }

    Logger::instance().info("Box2DPhysics", "Created body for entity (type=%d)", static_cast<int>(rb.body_type));

    // Note: Collider shapes are created separately via attach_collider_shapes()
}

void Box2DPhysicsSystem::sync_kinematic_bodies() {
    auto view = m_registry->view<physics::Rigidbody, Transform>();

    for (auto entity : view) {
        // Skip disabled entities
        if (m_registry->all_of<editor::EntityInfo>(entity)) {
            if (!m_registry->get<editor::EntityInfo>(entity).enabled_in_hierarchy) {
                continue;
            }
        }

        auto& rb = view.get<physics::Rigidbody>(entity);
        auto& transform = view.get<Transform>(entity);

        // Only sync kinematic bodies (script-controlled movement)
        if (!rb.enabled || !b2Body_IsValid(rb.body_id) || rb.body_type != physics::BodyType::Kinematic) {
            continue;
        }

        // Sync Transform → Physics for kinematic bodies
        float angle_rad = transform.rotation * (B2_PI / 180.0f);
        m_physics_world->set_body_transform(rb.body_id, transform.x, transform.y, angle_rad);
    }
}

void Box2DPhysicsSystem::on_rigidbody_destroyed(entt::registry& /*registry*/, entt::entity entity) {
    // This is called when a Rigidbody component is removed
    // We need to destroy the Box2D body before the component is gone
    auto* rb = m_registry->try_get<physics::Rigidbody>(entity);
    if (rb && b2Body_IsValid(rb->body_id)) {
        m_physics_world->destroy_body(rb->body_id);
        rb->body_id = b2_nullBodyId;
        Logger::instance().info("Box2DPhysics", "Destroyed body for entity");
    }
}

// --- Collider Creation (stubs for now, will implement in Steps 3-4) ---

void Box2DPhysicsSystem::create_box_collider(b2BodyId body, physics::BoxCollider& collider, const Transform& entity_transform) {
    // Apply entity scale to collider dimensions
    float scale_x = std::abs(entity_transform.scale_x);
    float scale_y = std::abs(entity_transform.scale_y);

    float half_width = m_physics_world->pixels_to_meters(collider.width * 0.5f * scale_x);
    float half_height = m_physics_world->pixels_to_meters(collider.height * 0.5f * scale_y);

    // Create box shape polygon (centered at origin)
    b2Polygon box = b2MakeBox(half_width, half_height);

    // Apply offset (scaled) if specified
    float ox = collider.offset_x * entity_transform.scale_x;
    float oy = collider.offset_y * entity_transform.scale_y;

    if (ox != 0.0f || oy != 0.0f) {
        b2Vec2 offset = m_physics_world->pixels_to_meters(ox, oy);
        b2Transform transform = {offset, b2MakeRot(collider.rotation * (B2_PI / 180.0f))};
        box = b2MakeOffsetBox(half_width, half_height, transform.p, transform.q);
    } else if (collider.rotation != 0.0f) {
        b2Rot rotation = b2MakeRot(collider.rotation * (B2_PI / 180.0f));
        b2Transform transform = {{0.0f, 0.0f}, rotation};
        box = b2MakeOffsetBox(half_width, half_height, transform.p, transform.q);
    }

    // Create shape definition
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = collider.density;
    shape_def.material.friction = collider.friction;
    shape_def.material.restitution = collider.restitution;
    shape_def.isSensor = collider.is_trigger;

    // Create shape and attach to body
    b2ShapeId shape_id = b2CreatePolygonShape(body, &shape_def, &box);

    // Store shape ID for later cleanup
    collider.shape_id = shape_id;

    Logger::instance().info("Box2DPhysics", "Created BoxCollider shape (w=%.2f, h=%.2f)",
                             collider.width, collider.height);
}

void Box2DPhysicsSystem::create_circle_collider(b2BodyId body, physics::CircleCollider& collider, const Transform& entity_transform) {
    // Apply entity scale to radius and offset
    float avg_scale = (std::abs(entity_transform.scale_x) + std::abs(entity_transform.scale_y)) * 0.5f;

    float radius_meters = m_physics_world->pixels_to_meters(collider.radius * avg_scale);

    b2Circle circle;
    circle.radius = radius_meters;

    float ox = collider.offset_x * entity_transform.scale_x;
    float oy = collider.offset_y * entity_transform.scale_y;
    if (ox != 0.0f || oy != 0.0f) {
        circle.center = m_physics_world->pixels_to_meters(ox, oy);
    } else {
        circle.center = {0.0f, 0.0f};
    }

    // Create shape definition
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = collider.density;
    shape_def.material.friction = collider.friction;
    shape_def.material.restitution = collider.restitution;
    shape_def.isSensor = collider.is_trigger;

    // Create shape and attach to body
    b2ShapeId shape_id = b2CreateCircleShape(body, &shape_def, &circle);

    // Store shape ID
    collider.shape_id = shape_id;

    Logger::instance().info("Box2DPhysics", "Created CircleCollider shape (r=%.2f)", collider.radius);
}

void Box2DPhysicsSystem::create_capsule_collider(b2BodyId body, physics::CapsuleCollider& collider, const Transform& entity_transform) {
    // Apply entity scale to dimensions
    float avg_scale = (std::abs(entity_transform.scale_x) + std::abs(entity_transform.scale_y)) * 0.5f;

    float radius_meters = m_physics_world->pixels_to_meters(collider.radius * avg_scale);
    float length_meters = m_physics_world->pixels_to_meters(collider.length * avg_scale);

    // Calculate rotation in radians
    float rotation_rad = collider.rotation * (B2_PI / 180.0f);
    b2Rot rot = b2MakeRot(rotation_rad);

    // Calculate offset (scaled)
    float ox = collider.offset_x * entity_transform.scale_x;
    float oy = collider.offset_y * entity_transform.scale_y;
    b2Vec2 offset = m_physics_world->pixels_to_meters(ox, oy);

    // Create shape definition
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = collider.density;
    shape_def.material.friction = collider.friction;
    shape_def.material.restitution = collider.restitution;
    shape_def.isSensor = collider.is_trigger;

    // Reserve space for 3 shapes (box + 2 circles)
    collider.shape_ids.reserve(3);

    // Determine capsule orientation based on rotation angle.
    // Normalize to [0, 180) range, then check if closer to 0 (vertical) or 90 (horizontal).
    float norm_angle = std::fmod(std::fabs(collider.rotation), 180.0f);
    bool is_vertical = (norm_angle < 45.0f || norm_angle > 135.0f);

    if (is_vertical) {
        float half_length = length_meters * 0.5f;

        b2Polygon box = b2MakeOffsetBox(radius_meters, half_length, offset, rot);
        b2ShapeId box_shape = b2CreatePolygonShape(body, &shape_def, &box);
        collider.shape_ids.push_back(box_shape);

        b2Circle top_circle;
        top_circle.radius = radius_meters;
        top_circle.center = {offset.x, offset.y - half_length};
        b2ShapeId top_shape = b2CreateCircleShape(body, &shape_def, &top_circle);
        collider.shape_ids.push_back(top_shape);

        b2Circle bottom_circle;
        bottom_circle.radius = radius_meters;
        bottom_circle.center = {offset.x, offset.y + half_length};
        b2ShapeId bottom_shape = b2CreateCircleShape(body, &shape_def, &bottom_circle);
        collider.shape_ids.push_back(bottom_shape);
    } else {
        float half_length = length_meters * 0.5f;

        b2Polygon box = b2MakeOffsetBox(half_length, radius_meters, offset, rot);
        b2ShapeId box_shape = b2CreatePolygonShape(body, &shape_def, &box);
        collider.shape_ids.push_back(box_shape);

        b2Circle left_circle;
        left_circle.radius = radius_meters;
        left_circle.center = {offset.x - half_length, offset.y};
        b2ShapeId left_shape = b2CreateCircleShape(body, &shape_def, &left_circle);
        collider.shape_ids.push_back(left_shape);

        b2Circle right_circle;
        right_circle.radius = radius_meters;
        right_circle.center = {offset.x + half_length, offset.y};
        b2ShapeId right_shape = b2CreateCircleShape(body, &shape_def, &right_circle);
        collider.shape_ids.push_back(right_shape);
    }

    Logger::instance().info("Box2DPhysics", "Created CapsuleCollider shape (length=%.2f, radius=%.2f, shapes=%zu)",
                             collider.length, collider.radius, collider.shape_ids.size());
}

void Box2DPhysicsSystem::create_dynamic_collider(entt::entity entity, b2BodyId body, physics::DynamicCollider& collider) {
    // Triangulate pixel grid if not already generated
    if (!collider.generated) {
        triangulate_pixel_grid(entity, collider);
    }

    // If triangulation failed or produced no geometry, skip
    if (!collider.generated || collider.triangle_count == 0) {
        return;
    }

    // Get pixel grid component to access triangulation results
    auto* grid_comp = m_registry->try_get<simulation::PixelGridComponent>(entity);
    if (!grid_comp) {
        return;
    }

    // Known limitation: DynamicCollider polygon shape creation from triangulated
    // pixel data is not yet implemented. Requires marching squares + polygon
    // simplification to convert pixel grids into Box2D convex hull shapes.

    Logger::instance().info("Box2DPhysics", "Created DynamicCollider with %d triangles", collider.triangle_count);
    collider.generated = true;
}

void Box2DPhysicsSystem::triangulate_pixel_grid(entt::entity entity, physics::DynamicCollider& collider) {
    // Get the pixel grid component
    auto* grid_comp = m_registry->try_get<simulation::PixelGridComponent>(entity);
    if (!grid_comp || !grid_comp->loaded) {
        return;
    }

    // Known limitation: Pixel grid triangulation is not yet implemented.
    // Would need PixelGridLoaderSystem pixel data → bool array → marching squares → simplification.

    Logger::instance().info("Box2DPhysics", "Triangulating pixel grid (%dx%d) with simplification=%.2f",
                            grid_comp->width, grid_comp->height, collider.simplification);

    // Mark as generated (placeholder)
    collider.generated = true;
    collider.triangle_count = 0;  // No actual triangles yet
}

} // namespace engine
