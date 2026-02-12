#pragma once

#include "box2d/box2d.h"
#include <vector>

namespace engine::physics {

// --- Base Collider Properties (common to all collider types) ---

/// Box collider for Box2D physics.
/// A rectangular collision shape.
struct BoxCollider {
    bool enabled = true;

    /// Is this a trigger? (no collision response, only overlap detection)
    bool is_trigger = false;

    /// Width of the box (in pixels)
    float width = 16.0f;

    /// Height of the box (in pixels)
    float height = 16.0f;

    /// Local offset X (relative to entity position)
    float offset_x = 0.0f;

    /// Local offset Y (relative to entity position)
    float offset_y = 0.0f;

    /// Rotation offset (in degrees, relative to entity rotation)
    float rotation = 0.0f;

    // --- Physics Material ---

    /// Density (kg/m²) - affects mass calculation
    float density = 1.0f;

    /// Friction coefficient (0 = ice, 1 = rubber)
    float friction = 0.3f;

    /// Restitution/bounciness (0 = no bounce, 1 = perfect bounce)
    float restitution = 0.0f;

    // --- Runtime State (managed by physics system, not serialized) ---

    /// Box2D shape handle
    b2ShapeId shape_id = b2_nullShapeId;
};

/// Capsule collider for Box2D physics.
/// A pill-shaped collision shape (rectangle with rounded ends).
struct CapsuleCollider {
    bool enabled = true;

    bool is_trigger = false;

    /// Length of the capsule (excluding rounded ends)
    float length = 1.0f;

    /// Radius of the rounded ends
    float radius = 0.5f;

    /// Capsule orientation (0 = vertical, 90 = horizontal, in degrees)
    float rotation = 0.0f;

    float offset_x = 0.0f;
    float offset_y = 0.0f;

    // Physics material
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    // --- Runtime State (managed by physics system, not serialized) ---

    /// Box2D shape handles (capsule is composed of box + 2 circles = 3 shapes)
    std::vector<b2ShapeId> shape_ids;
};

/// Circle collider for Box2D physics.
/// A circular collision shape.
struct CircleCollider {
    bool enabled = true;

    bool is_trigger = false;

    /// Radius of the circle (in world units)
    float radius = 0.5f;

    float offset_x = 0.0f;
    float offset_y = 0.0f;

    // Physics material
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    // --- Runtime State (managed by physics system, not serialized) ---

    /// Box2D shape handle
    b2ShapeId shape_id = b2_nullShapeId;
};

/// Dynamic collider generated from pixel grid boundaries.
/// This collider is automatically triangulated from the entity's PixelGridComponent.
struct DynamicCollider {
    bool enabled = true;

    bool is_trigger = false;

    /// Simplification tolerance (higher = fewer triangles, less accurate)
    /// 0 = no simplification, 1.0 = aggressive simplification
    float simplification = 0.5f;

    /// Minimum contour area to include (ignore small details)
    float min_contour_area = 4.0f;

    float offset_x = 0.0f;
    float offset_y = 0.0f;

    // Physics material
    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    // --- Runtime State ---

    /// Whether the collider mesh has been generated
    bool generated = false;

    /// Number of triangles in the generated mesh
    int triangle_count = 0;

    // --- Runtime State (managed by physics system, not serialized) ---

    /// Box2D shape handles (one per convex polygon, multiple due to 8-vertex limit)
    std::vector<b2ShapeId> shape_ids;
};

} // namespace engine::physics
