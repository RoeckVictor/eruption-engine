#pragma once

#include "engine/physics/ContourGenerator.h"
#include <vector>

namespace engine::physics {

struct Triangle {
    Vec2f a, b, c;
};

/// Ear clipping triangulation for simple polygons.
///
/// Converts a simple polygon (no self-intersections) into a set of triangles.
/// The polygon should have CCW winding for correct results. CW polygons are
/// automatically reversed.
class Triangulator {
public:
    /// Triangulate a simple polygon using ear clipping.
    /// Returns empty vector if the polygon has fewer than 3 vertices.
    static std::vector<Triangle> ear_clip(const std::vector<Vec2f>& polygon);
};

} // namespace engine::physics
