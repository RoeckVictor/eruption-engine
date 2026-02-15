#include "engine/physics/PhysicsWorld.h"
#include "engine/core/Log.h"
#include <vector>
#include <cmath>

namespace engine::physics {

Result<void, ErrorInfo> PhysicsWorld::init(float gravity_x, float gravity_y,
                                             float pixels_per_meter) {
    m_pixels_per_meter = pixels_per_meter;

    b2WorldDef world_def = b2DefaultWorldDef();
    world_def.gravity = {gravity_x, gravity_y};
    m_world_id = b2CreateWorld(&world_def);

    if (!b2World_IsValid(m_world_id)) {
        ENGINE_ERR("Failed to create Box2D world");
        return Err(EngineError::PhysicsWorldInvalid, "Failed to create Box2D world");
    }

    ENGINE_LOG("PhysicsWorld initialized (gravity=%.1f, %.1f, scale=%.1fpx/m)",
               gravity_x, gravity_y, m_pixels_per_meter);
    return Ok();
}

void PhysicsWorld::shutdown() {
    if (b2World_IsValid(m_world_id)) {
        b2DestroyWorld(m_world_id);
        m_world_id = b2_nullWorldId;
    }
}

void PhysicsWorld::step(float dt, int sub_step_count) {
    if (!b2World_IsValid(m_world_id)) return;
    b2World_Step(m_world_id, dt, sub_step_count);
}

// --- Coordinate conversion ---

b2Vec2 PhysicsWorld::pixels_to_meters(float px, float py) const {
    return {px / m_pixels_per_meter, py / m_pixels_per_meter};
}

void PhysicsWorld::meters_to_pixels(b2Vec2 meters, float& px, float& py) const {
    px = meters.x * m_pixels_per_meter;
    py = meters.y * m_pixels_per_meter;
}

float PhysicsWorld::pixels_to_meters(float pixels) const {
    return pixels / m_pixels_per_meter;
}

float PhysicsWorld::meters_to_pixels(float meters) const {
    return meters * m_pixels_per_meter;
}

// --- Body management ---

static b2BodyId create_body_impl(b2WorldId world, b2BodyType type,
                                  b2Vec2 position, float angle_rad) {
    b2BodyDef body_def = b2DefaultBodyDef();
    body_def.type = type;
    body_def.position = position;
    body_def.rotation = b2MakeRot(angle_rad);
    return b2CreateBody(world, &body_def);
}

b2BodyId PhysicsWorld::create_dynamic_body(float pixel_x, float pixel_y, float angle_rad) {
    return create_body_impl(m_world_id, b2_dynamicBody, pixels_to_meters(pixel_x, pixel_y), angle_rad);
}

b2BodyId PhysicsWorld::create_static_body(float pixel_x, float pixel_y, float angle_rad) {
    return create_body_impl(m_world_id, b2_staticBody, pixels_to_meters(pixel_x, pixel_y), angle_rad);
}

b2BodyId PhysicsWorld::create_kinematic_body(float pixel_x, float pixel_y, float angle_rad) {
    return create_body_impl(m_world_id, b2_kinematicBody, pixels_to_meters(pixel_x, pixel_y), angle_rad);
}

void PhysicsWorld::destroy_body(b2BodyId body) {
    if (b2Body_IsValid(body)) {
        b2DestroyBody(body);
    }
}

// --- Shape management ---

b2ShapeId PhysicsWorld::add_polygon_shape(b2BodyId body, const b2Vec2* verts, int count,
                                          float density, float friction, float restitution) {
    b2Hull hull = b2ComputeHull(verts, count);
    if (hull.count < 3 || !b2ValidateHull(&hull)) {
        ENGINE_LOG_WARN("PhysicsWorld::add_polygon_shape() - Degenerate hull (count=%d, input=%d)", hull.count, count);
        return {};
    }
    b2Polygon polygon = b2MakePolygon(&hull, 0.0f);

    b2ShapeDef shape_def = b2DefaultShapeDef();
    shape_def.density = density;
    shape_def.material.friction = friction;
    shape_def.material.restitution = restitution;

    b2ShapeId shape = b2CreatePolygonShape(body, &shape_def, &polygon);
    if (!b2Shape_IsValid(shape)) {
        ENGINE_ERR("PhysicsWorld::add_polygon_shape() - b2CreatePolygonShape returned invalid shape");
        return {};
    }
    return shape;
}

b2ChainId PhysicsWorld::add_chain_shape(b2BodyId body, const b2Vec2* verts, int count,
                                        float friction, float restitution) {
    b2SurfaceMaterial surface = b2DefaultSurfaceMaterial();
    surface.friction = friction;
    surface.restitution = restitution;

    b2ChainDef chain_def = b2DefaultChainDef();
    chain_def.points = verts;
    chain_def.count = count;
    chain_def.isLoop = true;
    chain_def.materials = &surface;
    chain_def.materialCount = 1;

    b2ChainId chain = b2CreateChain(body, &chain_def);
    if (!b2Chain_IsValid(chain)) {
        ENGINE_ERR("PhysicsWorld::add_chain_shape() - b2CreateChain returned invalid chain (count=%d)", count);
        return {};
    }
    return chain;
}

void PhysicsWorld::destroy_all_shapes(b2BodyId body) {
    if (!b2Body_IsValid(body)) return;

    int shape_count = b2Body_GetShapeCount(body);
    if (shape_count > 0) {
        std::vector<b2ShapeId> shapes(shape_count);
        b2Body_GetShapes(body, shapes.data(), shape_count);

        for (int i = 0; i < shape_count; i++) {
            b2DestroyShape(shapes[i], true);
        }
    }
}

// --- Queries ---

b2Vec2 PhysicsWorld::get_body_position(b2BodyId body) const {
    b2Vec2 meters = b2Body_GetPosition(body);
    return {meters.x * m_pixels_per_meter, meters.y * m_pixels_per_meter};
}

float PhysicsWorld::get_body_angle(b2BodyId body) const {
    return b2Rot_GetAngle(b2Body_GetRotation(body));
}

b2Vec2 PhysicsWorld::get_body_linear_velocity(b2BodyId body) const {
    b2Vec2 v = b2Body_GetLinearVelocity(body);
    return {v.x * m_pixels_per_meter, v.y * m_pixels_per_meter};
}

float PhysicsWorld::get_body_angular_velocity(b2BodyId body) const {
    return b2Body_GetAngularVelocity(body);
}

float PhysicsWorld::get_body_mass(b2BodyId body) const {
    return b2Body_GetMass(body);
}

void PhysicsWorld::set_body_transform(b2BodyId body, float pixel_x, float pixel_y, float angle_rad) {
    b2Body_SetTransform(body, pixels_to_meters(pixel_x, pixel_y), b2MakeRot(angle_rad));
}

void PhysicsWorld::set_body_linear_velocity(b2BodyId body, float vx_pixels, float vy_pixels) {
    b2Body_SetLinearVelocity(body, pixels_to_meters(vx_pixels, vy_pixels));
}

void PhysicsWorld::set_body_angular_velocity(b2BodyId body, float angular_vel) {
    b2Body_SetAngularVelocity(body, angular_vel);
}

void PhysicsWorld::apply_force(b2BodyId body, float fx_pixels, float fy_pixels) {
    b2Vec2 center = b2Body_GetWorldCenterOfMass(body);
    b2Body_ApplyForce(body, pixels_to_meters(fx_pixels, fy_pixels), center, true);
}

void PhysicsWorld::apply_impulse(b2BodyId body, float ix_pixels, float iy_pixels) {
    b2Vec2 center = b2Body_GetWorldCenterOfMass(body);
    b2Body_ApplyLinearImpulse(body, pixels_to_meters(ix_pixels, iy_pixels), center, true);
}

void PhysicsWorld::set_fixed_rotation(b2BodyId body, bool fixed) {
    if (b2Body_IsValid(body)) {
        b2Body_SetFixedRotation(body, fixed);
    }
}

void PhysicsWorld::set_gravity_scale(b2BodyId body, float scale) {
    if (b2Body_IsValid(body)) {
        b2Body_SetGravityScale(body, scale);
    }
}

bool PhysicsWorld::is_grounded(b2BodyId body, float tolerance_pixels) const {
    if (!b2Body_IsValid(body)) return false;

    int contact_capacity = b2Body_GetContactCapacity(body);
    if (contact_capacity == 0) return false;

    std::vector<b2ContactData> contacts(contact_capacity);
    int contact_count = b2Body_GetContactData(body, contacts.data(), contact_capacity);
    if (contact_count == 0) return false;

    b2Vec2 body_pos = b2Body_GetPosition(body);
    float tolerance_m = pixels_to_meters(tolerance_pixels);

    for (int i = 0; i < contact_count; i++) {
        const b2ContactData& contact = contacts[i];
        const b2Manifold& manifold = contact.manifold;
        if (manifold.pointCount == 0) continue;

        // Check for a significant vertical normal component.
        // We use fabs because the manifold normal direction depends on
        // which body is shape A vs B in the contact pair, which we can't control.
        if (std::fabs(manifold.normal.y) < 0.3f) continue;

        // Verify the contact point is at or below the body center.
        // In our Y-down coordinate system, greater Y = below.
        for (int j = 0; j < manifold.pointCount; j++) {
            float point_y = manifold.points[j].point.y;
            if (point_y > body_pos.y - tolerance_m) {
                return true;
            }
        }
    }

    return false;
}

bool PhysicsWorld::valid() const {
    return b2World_IsValid(m_world_id);
}

} // namespace engine::physics
