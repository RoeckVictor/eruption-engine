#include "PhysicsPlayback.h"
#include "engine/core/Transform.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Logger.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace editor {

PhysicsPlayback::PhysicsPlayback(entt::registry& registry, engine::physics::PhysicsWorld& world)
    : m_registry(registry), m_world(world) {}

void PhysicsPlayback::init_bodies() {
    int body_count = 0;

    // 1) Create bodies for entities with explicit Rigidbody components
    auto rb_view = m_registry.view<engine::physics::Rigidbody, engine::Transform>();
    for (auto entity : rb_view) {
        auto& rb = rb_view.get<engine::physics::Rigidbody>(entity);
        if (!rb.enabled) continue;
        if (b2Body_IsValid(rb.body_id)) continue;

        create_body_for_entity(entity);
        body_count++;
    }

    // 2) Create implicit static bodies for collider-only entities (no Rigidbody).
    //    Box2D requires every shape to be attached to a body.
    //    Collect entities first to avoid modifying the registry during iteration.
    int static_count = 0;
    std::vector<entt::entity> collider_only_entities;

    auto collect_if_no_rigidbody = [&](entt::entity entity) {
        if (!m_registry.all_of<engine::Transform>(entity)) return;
        if (m_registry.all_of<engine::physics::Rigidbody>(entity)) return;
        collider_only_entities.push_back(entity);
    };

    m_registry.view<engine::physics::BoxCollider>().each(
        [&](entt::entity e, auto&) { collect_if_no_rigidbody(e); });
    m_registry.view<engine::physics::CircleCollider>().each(
        [&](entt::entity e, auto&) { collect_if_no_rigidbody(e); });
    m_registry.view<engine::physics::CapsuleCollider>().each(
        [&](entt::entity e, auto&) { collect_if_no_rigidbody(e); });

    // Deduplicate (entity may have multiple collider types)
    std::sort(collider_only_entities.begin(), collider_only_entities.end());
    collider_only_entities.erase(
        std::unique(collider_only_entities.begin(), collider_only_entities.end()),
        collider_only_entities.end());

    // Now safe to emplace components outside of iteration
    for (auto entity : collider_only_entities) {
        auto& rb = m_registry.emplace<engine::physics::Rigidbody>(entity);
        rb.body_type = engine::physics::BodyType::Static;
        rb.mass = 0.0f;
        rb.enabled = true;

        auto& transform = m_registry.get<engine::Transform>(entity);
        float angle_rad = transform.world_rotation * engine::DEG_TO_RAD;
        rb.body_id = m_world.create_static_body(
            transform.world_x, transform.world_y, angle_rad);

        attach_collider_shapes(entity, rb);
        static_count++;
    }

    engine::Logger::instance().info("Runtime", "Created %d physics bodies (%d implicit static)", body_count + static_count, static_count);
}

void PhysicsPlayback::create_body_for_entity(entt::entity entity) {
    if (!m_registry.all_of<engine::physics::Rigidbody, engine::Transform>(entity)) return;

    auto& rb = m_registry.get<engine::physics::Rigidbody>(entity);
    auto& transform = m_registry.get<engine::Transform>(entity);

    // Create body based on type using world position
    float angle_rad = transform.world_rotation * engine::DEG_TO_RAD;

    switch (rb.body_type) {
        case engine::physics::BodyType::Dynamic:
            rb.body_id = m_world.create_dynamic_body(
                transform.world_x, transform.world_y, angle_rad);
            break;
        case engine::physics::BodyType::Static:
            rb.body_id = m_world.create_static_body(
                transform.world_x, transform.world_y, angle_rad);
            break;
        case engine::physics::BodyType::Kinematic:
            rb.body_id = m_world.create_kinematic_body(
                transform.world_x, transform.world_y, angle_rad);
            break;
    }

    // Apply body properties
    m_world.set_gravity_scale(rb.body_id, rb.gravity_scale);
    m_world.set_fixed_rotation(rb.body_id, rb.lock_rotation);

    // Apply initial velocity for dynamic bodies
    if (rb.body_type == engine::physics::BodyType::Dynamic) {
        if (rb.initial_velocity_x != 0.0f || rb.initial_velocity_y != 0.0f) {
            m_world.set_body_linear_velocity(rb.body_id,
                rb.initial_velocity_x, rb.initial_velocity_y);
        }
        if (rb.initial_angular_velocity != 0.0f) {
            m_world.set_body_angular_velocity(rb.body_id,
                rb.initial_angular_velocity);
        }
    }

    // Attach collider shapes to the body
    attach_collider_shapes(entity, rb);

    // For dynamic bodies: ensure mass is set even without collider shapes.
    // Box2D bodies with no shapes have zero mass and won't respond to gravity.
    // Use Rigidbody.mass as override (like Unity behavior).
    if (rb.body_type == engine::physics::BodyType::Dynamic && rb.mass > 0.0f) {
        b2MassData mass_data = b2Body_GetMassData(rb.body_id);
        if (mass_data.mass <= 0.0f) {
            mass_data.mass = rb.mass;
            mass_data.center = {0.0f, 0.0f};
            mass_data.rotationalInertia = rb.mass * 0.01f;
            b2Body_SetMassData(rb.body_id, mass_data);
        }
    }
}

void PhysicsPlayback::attach_collider_shapes(entt::entity entity, engine::physics::Rigidbody& rb) {
    if (!m_registry.all_of<engine::Transform>(entity)) return;

    auto& transform = m_registry.get<engine::Transform>(entity);
    float scale_x = transform.world_scale_x;
    float scale_y = transform.world_scale_y;
    float avg_scale = (std::abs(scale_x) + std::abs(scale_y)) * 0.5f;

    // BoxCollider
    if (m_registry.all_of<engine::physics::BoxCollider>(entity)) {
        auto& box = m_registry.get<engine::physics::BoxCollider>(entity);
        if (box.enabled) {
            float hw = m_world.pixels_to_meters(box.width * 0.5f * std::abs(scale_x));
            float hh = m_world.pixels_to_meters(box.height * 0.5f * std::abs(scale_y));
            float ox = m_world.pixels_to_meters(box.offset_x * scale_x);
            float oy = m_world.pixels_to_meters(box.offset_y * scale_y);

            float rot_rad = box.rotation * engine::DEG_TO_RAD;
            float cos_r = std::cos(rot_rad);
            float sin_r = std::sin(rot_rad);

            float corners[4][2] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };
            b2Vec2 verts[4];
            for (int i = 0; i < 4; i++) {
                verts[i].x = corners[i][0] * cos_r - corners[i][1] * sin_r + ox;
                verts[i].y = corners[i][0] * sin_r + corners[i][1] * cos_r + oy;
            }

            box.shape_id = m_world.add_polygon_shape(
                rb.body_id, verts, 4, box.density, box.friction, box.restitution);
        }
    }

    // CircleCollider
    if (m_registry.all_of<engine::physics::CircleCollider>(entity)) {
        auto& circle = m_registry.get<engine::physics::CircleCollider>(entity);
        if (circle.enabled) {
            b2Circle c;
            c.center.x = m_world.pixels_to_meters(circle.offset_x * scale_x);
            c.center.y = m_world.pixels_to_meters(circle.offset_y * scale_y);
            c.radius = m_world.pixels_to_meters(circle.radius * avg_scale);

            b2ShapeDef shape_def = b2DefaultShapeDef();
            shape_def.density = circle.density;
            shape_def.material.friction = circle.friction;
            shape_def.material.restitution = circle.restitution;
            shape_def.isSensor = circle.is_trigger;

            circle.shape_id = b2CreateCircleShape(rb.body_id, &shape_def, &c);
        }
    }

    // CapsuleCollider
    if (m_registry.all_of<engine::physics::CapsuleCollider>(entity)) {
        auto& cap = m_registry.get<engine::physics::CapsuleCollider>(entity);
        if (cap.enabled) {
            float half_len = m_world.pixels_to_meters(cap.length * 0.5f * avg_scale);
            float rad = m_world.pixels_to_meters(cap.radius * avg_scale);
            float ox = m_world.pixels_to_meters(cap.offset_x * scale_x);
            float oy = m_world.pixels_to_meters(cap.offset_y * scale_y);
            float cap_rot = cap.rotation * engine::DEG_TO_RAD;

            float ax = -std::sin(cap_rot);
            float ay = std::cos(cap_rot);

            float bw = rad;
            float bh = half_len;
            float cos_c = std::cos(cap_rot);
            float sin_c = std::sin(cap_rot);

            float box_corners[4][2] = {
                {-bw, -bh}, {bw, -bh}, {bw, bh}, {-bw, bh}
            };
            b2Vec2 box_verts[4];
            for (int i = 0; i < 4; i++) {
                box_verts[i].x = box_corners[i][0] * cos_c - box_corners[i][1] * sin_c + ox;
                box_verts[i].y = box_corners[i][0] * sin_c + box_corners[i][1] * cos_c + oy;
            }
            cap.shape_ids.push_back(m_world.add_polygon_shape(
                rb.body_id, box_verts, 4, cap.density, cap.friction, cap.restitution));

            b2ShapeDef shape_def = b2DefaultShapeDef();
            shape_def.density = cap.density;
            shape_def.material.friction = cap.friction;
            shape_def.material.restitution = cap.restitution;
            shape_def.isSensor = cap.is_trigger;

            b2Circle top_circle;
            top_circle.center.x = ox + ax * half_len;
            top_circle.center.y = oy + ay * half_len;
            top_circle.radius = rad;
            cap.shape_ids.push_back(b2CreateCircleShape(rb.body_id, &shape_def, &top_circle));

            b2Circle bottom_circle;
            bottom_circle.center.x = ox - ax * half_len;
            bottom_circle.center.y = oy - ay * half_len;
            bottom_circle.radius = rad;
            cap.shape_ids.push_back(b2CreateCircleShape(rb.body_id, &shape_def, &bottom_circle));
        }
    }
}

void PhysicsPlayback::sync_to_transforms() {
    auto view = m_registry.view<engine::physics::Rigidbody, engine::Transform>();

    for (auto entity : view) {
        auto& rb = view.get<engine::physics::Rigidbody>(entity);
        auto& transform = view.get<engine::Transform>(entity);

        if (!rb.enabled) continue;

        // Auto-create body for dynamically spawned entities
        if (!b2Body_IsValid(rb.body_id)) {
            create_body_for_entity(entity);
            if (!b2Body_IsValid(rb.body_id)) continue;
        }

        if (rb.body_type == engine::physics::BodyType::Dynamic) {
            b2Vec2 pos = m_world.get_body_position(rb.body_id);
            float angle = m_world.get_body_angle(rb.body_id);

            transform.x = pos.x;
            transform.y = pos.y;
            transform.rotation = angle * engine::RAD_TO_DEG;

            if (rb.lock_position_x || rb.lock_position_y) {
                b2Vec2 vel = m_world.get_body_linear_velocity(rb.body_id);
                if (rb.lock_position_x) vel.x = 0.0f;
                if (rb.lock_position_y) vel.y = 0.0f;
                m_world.set_body_linear_velocity(rb.body_id, vel.x, vel.y);
            }
        }
        else if (rb.body_type == engine::physics::BodyType::Kinematic) {
            m_world.set_body_transform(rb.body_id,
                transform.world_x, transform.world_y,
                transform.world_rotation * engine::DEG_TO_RAD);
        }
    }
}

}