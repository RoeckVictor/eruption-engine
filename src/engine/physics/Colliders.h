#pragma once

#include "box2d/box2d.h"
#include <vector>

namespace engine::physics {

// Box collider for Box2D physics
struct BoxCollider {
    bool enabled = true;
    bool is_trigger = false;

    float width = 16.0f;
    float height = 16.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float rotation = 0.0f;

    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    b2ShapeId shape_id = b2_nullShapeId;
};

// Capsule collider for Box2D physics
struct CapsuleCollider {
    bool enabled = true;
    bool is_trigger = false;

    float length = 1.0f;
    float radius = 0.5f;
    float rotation = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    std::vector<b2ShapeId> shape_ids;
};

// Circle collider for Box2D physics
struct CircleCollider {
    bool enabled = true;
    bool is_trigger = false;

    float radius = 0.5f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;

    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    b2ShapeId shape_id = b2_nullShapeId;
};

// Dynamic collider generated from pixel grid boundaries
// This collider is automatically triangulated from the entity's PixelGridComponent
struct DynamicCollider {
    bool enabled = true;
    bool is_trigger = false;

    float simplification = 0.5f;
    float min_contour_area = 4.0f;

    float offset_x = 0.0f;
    float offset_y = 0.0f;

    float density = 1.0f;
    float friction = 0.3f;
    float restitution = 0.0f;

    bool generated = false;

    int triangle_count = 0;

    std::vector<b2ShapeId> shape_ids;
};

}
