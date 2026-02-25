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
        // Box2D uses Y-up, so lower Y = below the body center.
        for (int j = 0; j < manifold.pointCount; j++) {
            float point_y = manifold.points[j].point.y;
            if (point_y < body_pos.y + tolerance_m) {
                return true;
            }
        }
    }

    return false;
}

bool PhysicsWorld::valid() const {
    return b2World_IsValid(m_world_id);
}

void PhysicsWorld::for_each_contact(const ContactCallback& callback) const {
    if (!b2World_IsValid(m_world_id)) return;

    // Iterate through contact events from the world
    b2ContactEvents contact_events = b2World_GetContactEvents(m_world_id);

    // Process begin contacts
    for (int i = 0; i < contact_events.beginCount; ++i) {
        const b2ContactBeginTouchEvent& event = contact_events.beginEvents[i];

        b2BodyId body_a = b2Shape_GetBody(event.shapeIdA);
        b2BodyId body_b = b2Shape_GetBody(event.shapeIdB);

        bool is_sensor_a = b2Shape_IsSensor(event.shapeIdA);
        bool is_sensor_b = b2Shape_IsSensor(event.shapeIdB);

        ContactInfo info;
        info.body_a = body_a;
        info.body_b = body_b;
        info.shape_a = event.shapeIdA;
        info.shape_b = event.shapeIdB;
        info.is_touching = true;
        info.is_sensor = is_sensor_a || is_sensor_b;
        info.normal_x = 0.0f;
        info.normal_y = 0.0f;
        info.point_x = 0.0f;
        info.point_y = 0.0f;
        info.impulse = 0.0f;

        // Get contact manifold for detailed info if available
        // Note: Begin events don't have manifold data in Box2D 3.x

        callback(info);
    }

    // Process end contacts (for exit detection)
    for (int i = 0; i < contact_events.endCount; ++i) {
        const b2ContactEndTouchEvent& event = contact_events.endEvents[i];

        // Only process if both shapes are still valid
        if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB)) {
            continue;
        }

        b2BodyId body_a = b2Shape_GetBody(event.shapeIdA);
        b2BodyId body_b = b2Shape_GetBody(event.shapeIdB);

        // Validity already checked above
        bool is_sensor_a = b2Shape_IsSensor(event.shapeIdA);
        bool is_sensor_b = b2Shape_IsSensor(event.shapeIdB);

        ContactInfo info;
        info.body_a = body_a;
        info.body_b = body_b;
        info.shape_a = event.shapeIdA;
        info.shape_b = event.shapeIdB;
        info.is_touching = false;  // No longer touching
        info.is_sensor = is_sensor_a || is_sensor_b;
        info.normal_x = 0.0f;
        info.normal_y = 0.0f;
        info.point_x = 0.0f;
        info.point_y = 0.0f;
        info.impulse = 0.0f;

        callback(info);
    }

    // Process hit events for impulse data
    for (int i = 0; i < contact_events.hitCount; ++i) {
        const b2ContactHitEvent& event = contact_events.hitEvents[i];

        b2BodyId body_a = b2Shape_GetBody(event.shapeIdA);
        b2BodyId body_b = b2Shape_GetBody(event.shapeIdB);

        bool is_sensor_a = b2Shape_IsSensor(event.shapeIdA);
        bool is_sensor_b = b2Shape_IsSensor(event.shapeIdB);

        ContactInfo info;
        info.body_a = body_a;
        info.body_b = body_b;
        info.shape_a = event.shapeIdA;
        info.shape_b = event.shapeIdB;
        info.is_touching = true;
        info.is_sensor = is_sensor_a || is_sensor_b;
        info.normal_x = event.normal.x;
        info.normal_y = event.normal.y;
        info.point_x = event.point.x * m_pixels_per_meter;
        info.point_y = event.point.y * m_pixels_per_meter;
        info.impulse = event.approachSpeed;  // Use approach speed as impulse indicator

        callback(info);
    }
}

PhysicsWorld::RaycastHit PhysicsWorld::raycast(float origin_x, float origin_y,
                                                float dir_x, float dir_y,
                                                float max_distance) const {
    RaycastHit result;
    if (!b2World_IsValid(m_world_id)) return result;

    // Convert to meters
    b2Vec2 origin = pixels_to_meters(origin_x, origin_y);

    // Normalize direction and scale by max distance in meters
    float dir_len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    if (dir_len < 0.0001f) return result;

    float max_dist_m = pixels_to_meters(max_distance);
    b2Vec2 translation = {
        (dir_x / dir_len) * max_dist_m,
        (dir_y / dir_len) * max_dist_m
    };

    b2QueryFilter filter = b2DefaultQueryFilter();
    b2RayResult ray_result = b2World_CastRayClosest(m_world_id, origin, translation, filter);

    if (ray_result.hit) {
        result.hit = true;
        result.shape = ray_result.shapeId;
        result.body = b2Shape_GetBody(ray_result.shapeId);
        result.point_x = ray_result.point.x * m_pixels_per_meter;
        result.point_y = ray_result.point.y * m_pixels_per_meter;
        result.normal_x = ray_result.normal.x;
        result.normal_y = ray_result.normal.y;
        result.fraction = ray_result.fraction;
    }

    return result;
}

void PhysicsWorld::for_each_body(const BodyCallback& callback) const {
    if (!b2World_IsValid(m_world_id)) return;

    // Get body count and iterate
    b2BodyEvents body_events = b2World_GetBodyEvents(m_world_id);

    // Box2D 3.x doesn't have a direct "iterate all bodies" API.
    // We need to use body move events to track bodies, or query world AABB.
    // For now, use a world AABB query to find all bodies.

    // Query a very large AABB to capture all bodies
    b2AABB query_aabb;
    query_aabb.lowerBound = {-1e6f, -1e6f};
    query_aabb.upperBound = {1e6f, 1e6f};

    // Use b2World_OverlapAABB to find all shapes, then collect unique bodies
    struct QueryContext {
        const BodyCallback* callback;
        std::vector<uint64_t> seen_bodies;  // Track seen body IDs
    };

    QueryContext ctx;
    ctx.callback = &callback;

    auto overlap_callback = [](b2ShapeId shape_id, void* user_data) -> bool {
        auto* ctx = static_cast<QueryContext*>(user_data);

        b2BodyId body_id = b2Shape_GetBody(shape_id);
        if (!b2Body_IsValid(body_id)) return true;

        // Create unique key from body ID
        uint64_t body_key = static_cast<uint64_t>(body_id.index1) |
                           (static_cast<uint64_t>(body_id.world0) << 32);

        // Check if we've already processed this body
        bool seen = false;
        for (uint64_t key : ctx->seen_bodies) {
            if (key == body_key) {
                seen = true;
                break;
            }
        }

        if (!seen) {
            ctx->seen_bodies.push_back(body_key);
            (*ctx->callback)(body_id);
        }

        return true;  // Continue iteration
    };

    b2QueryFilter filter = b2DefaultQueryFilter();
    b2World_OverlapAABB(m_world_id, query_aabb, filter, overlap_callback, &ctx);
}

} // namespace engine::physics
