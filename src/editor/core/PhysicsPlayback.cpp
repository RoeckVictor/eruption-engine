#include "PhysicsPlayback.h"
#include "engine/core/Transform.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Logger.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/physics/PixelGridTriangulation.h"
#include "engine/simulation/PixelGridComponent.h"
#include <box2d/box2d.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace editor {

PhysicsPlayback::PhysicsPlayback(entt::registry& registry, engine::physics::PhysicsWorld& world,
                                 engine::simulation::IPixelGridLoader* pixel_grid_loader)
    : m_registry(registry), m_world(world), m_pixel_grid_loader(pixel_grid_loader) {}

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

    // CapsuleCollider - use native Box2D capsule for better ghost collision prevention
    if (m_registry.all_of<engine::physics::CapsuleCollider>(entity)) {
        auto& cap = m_registry.get<engine::physics::CapsuleCollider>(entity);
        if (cap.enabled) {
            float half_len = m_world.pixels_to_meters(cap.length * 0.5f * avg_scale);
            float rad = m_world.pixels_to_meters(cap.radius * avg_scale);
            float ox = m_world.pixels_to_meters(cap.offset_x * scale_x);
            float oy = m_world.pixels_to_meters(cap.offset_y * scale_y);
            float cap_rot = cap.rotation * engine::DEG_TO_RAD;

            // Calculate capsule axis direction
            float ax = -std::sin(cap_rot);
            float ay = std::cos(cap_rot);

            b2ShapeDef shape_def = b2DefaultShapeDef();
            shape_def.density = cap.density;
            shape_def.material.friction = cap.friction;
            shape_def.material.restitution = cap.restitution;
            shape_def.isSensor = cap.is_trigger;

            // Create native Box2D capsule
            b2Capsule capsule;
            capsule.center1 = {ox + ax * half_len, oy + ay * half_len};
            capsule.center2 = {ox - ax * half_len, oy - ay * half_len};
            capsule.radius = rad;

            b2ShapeId shape_id = b2CreateCapsuleShape(rb.body_id, &shape_def, &capsule);
            if (b2Shape_IsValid(shape_id)) {
                cap.shape_ids.push_back(shape_id);
            }
        }
    }

    // DynamicCollider - defer to update_dynamic_colliders() since pixel grid may not be loaded yet
    // Just mark as needing generation
    if (m_registry.all_of<engine::physics::DynamicCollider>(entity)) {
        auto& dynamic = m_registry.get<engine::physics::DynamicCollider>(entity);
        if (dynamic.enabled) {
            // Will be processed in update_dynamic_colliders()
            dynamic.generated = false;
        }
    }
}

void PhysicsPlayback::update_dynamic_colliders() {
    auto view = m_registry.view<engine::physics::Rigidbody, engine::physics::DynamicCollider,
                                 engine::simulation::PixelGridComponent>();

    for (auto entity : view) {
        auto& rb = view.get<engine::physics::Rigidbody>(entity);
        auto& collider = view.get<engine::physics::DynamicCollider>(entity);
        auto& grid_comp = view.get<engine::simulation::PixelGridComponent>(entity);

        if (!b2Body_IsValid(rb.body_id) || !collider.enabled || !grid_comp.loaded) {
            continue;
        }

        // Check if regeneration is needed
        if (!collider.generated && !collider.shape_ids.empty()) {
            // Destroy old shapes before regenerating
            destroy_dynamic_collider_shapes(collider);
            m_collider_meshes.erase(entity);
        }

        // Skip if already generated
        if (collider.generated && !collider.shape_ids.empty()) {
            continue;
        }

        // Generate and attach collider
        attach_dynamic_collider(entity, rb);
    }
}

void PhysicsPlayback::attach_dynamic_collider(entt::entity entity, engine::physics::Rigidbody& rb) {
    auto* collider = m_registry.try_get<engine::physics::DynamicCollider>(entity);
    if (!collider || !collider->enabled) return;

    // Triangulate if needed
    if (!collider->generated) {
        triangulate_pixel_grid(entity, *collider);
    }

    if (!collider->generated) return;

    // Get cached mesh
    auto mesh_it = m_collider_meshes.find(entity);
    if (mesh_it == m_collider_meshes.end() || mesh_it->second.triangles.empty()) {
        engine::Logger::instance().info("PhysicsPlayback", "DynamicCollider has no geometry");
        return;
    }

    const auto& mesh = mesh_it->second;

    // Get pixel grid component for origin offset
    auto* grid_comp = m_registry.try_get<engine::simulation::PixelGridComponent>(entity);
    if (!grid_comp) return;

    // Get transform for scaling
    auto* transform = m_registry.try_get<engine::Transform>(entity);
    float scale_x = transform ? std::abs(transform->world_scale_x) : 1.0f;
    float scale_y = transform ? std::abs(transform->world_scale_y) : 1.0f;

    // Create shape definition
    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = collider->density;
    shape_def.material.friction = collider->friction;
    shape_def.material.restitution = collider->restitution;
    shape_def.isSensor = collider->is_trigger;

    // Set up coordinate transform parameters
    engine::physics::GridToLocalParams params;
    params.origin_x = static_cast<float>(grid_comp->origin_x);
    params.origin_y = static_cast<float>(grid_comp->origin_y);
    params.grid_height = static_cast<float>(grid_comp->height);
    params.scale_x = scale_x;
    params.scale_y = scale_y;
    params.offset_x = collider->offset_x;
    params.offset_y = collider->offset_y;

    // Create a Box2D polygon for each triangle
    collider->shape_ids.reserve(mesh.triangles.size());

    for (const auto& tri : mesh.triangles) {
        auto [lx_a, ly_a] = engine::physics::PixelGridTriangulation::grid_to_local(tri.a, params);
        auto [lx_b, ly_b] = engine::physics::PixelGridTriangulation::grid_to_local(tri.b, params);
        auto [lx_c, ly_c] = engine::physics::PixelGridTriangulation::grid_to_local(tri.c, params);

        b2Vec2 verts[3] = {
            m_world.pixels_to_meters(lx_a, ly_a),
            m_world.pixels_to_meters(lx_b, ly_b),
            m_world.pixels_to_meters(lx_c, ly_c)
        };

        // Check winding and ensure CCW
        if (engine::physics::PixelGridTriangulation::is_clockwise(
                verts[0].x, verts[0].y, verts[1].x, verts[1].y, verts[2].x, verts[2].y)) {
            std::swap(verts[1], verts[2]);
        }

        // Skip degenerate triangles
        float area = engine::physics::PixelGridTriangulation::triangle_area(
            verts[0].x, verts[0].y, verts[1].x, verts[1].y, verts[2].x, verts[2].y);
        if (area < 1e-6f) continue;

        b2Hull hull = b2ComputeHull(verts, 3);
        if (hull.count < 3) continue;

        b2Polygon polygon = b2MakePolygon(&hull, 0.0f);
        b2ShapeId shape_id = b2CreatePolygonShape(rb.body_id, &shape_def, &polygon);
        collider->shape_ids.push_back(shape_id);
    }

    collider->triangle_count = static_cast<int>(collider->shape_ids.size());
    engine::Logger::instance().info("PhysicsPlayback", "Created DynamicCollider with %d shapes",
                                    collider->triangle_count);
}

void PhysicsPlayback::triangulate_pixel_grid(entt::entity entity, engine::physics::DynamicCollider& collider) {
    auto* grid_comp = m_registry.try_get<engine::simulation::PixelGridComponent>(entity);
    if (!grid_comp || !grid_comp->loaded) return;

    const engine::simulation::LoadedPixelGridData* loaded_grid = nullptr;
    if (m_pixel_grid_loader) {
        loaded_grid = m_pixel_grid_loader->get_loaded_grid_data(entity);
    }

    if (!loaded_grid || loaded_grid->material_ids.empty()) {
        engine::Logger::instance().warning("PhysicsPlayback", "DynamicCollider: No pixel data");
        collider.generated = true;
        collider.triangle_count = 0;
        return;
    }

    engine::Logger::instance().info("PhysicsPlayback", "Triangulating pixel grid (%dx%d)",
                                    loaded_grid->width, loaded_grid->height);

    // Use shared triangulation utility
    auto result = engine::physics::PixelGridTriangulation::triangulate(
        loaded_grid, collider.simplification, collider.min_contour_area);

    if (result.triangles.empty()) {
        engine::Logger::instance().info("PhysicsPlayback", "DynamicCollider: No geometry generated");
        collider.generated = true;
        collider.triangle_count = 0;
        return;
    }

    engine::Logger::instance().info("PhysicsPlayback", "Generated %zu triangles",
                                    result.triangles.size());

    // Cache the mesh
    m_collider_meshes[entity] = std::move(result);

    collider.generated = true;
    collider.triangle_count = static_cast<int>(m_collider_meshes[entity].triangles.size());
}

void PhysicsPlayback::destroy_dynamic_collider_shapes(engine::physics::DynamicCollider& collider) {
    for (auto shape_id : collider.shape_ids) {
        if (b2Shape_IsValid(shape_id)) {
            b2DestroyShape(shape_id, true);
        }
    }
    collider.shape_ids.clear();
    collider.triangle_count = 0;
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