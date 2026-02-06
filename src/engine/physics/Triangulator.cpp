#include "engine/physics/Triangulator.h"
#include <cmath>
#include <algorithm>

namespace engine::physics {

// Cross product of vectors (b-a) and (c-a)
static float cross2d(const Vec2f& a, const Vec2f& b, const Vec2f& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// Check if point p is strictly inside triangle (a, b, c) assuming CCW winding.
static bool point_in_triangle(const Vec2f& p, const Vec2f& a, const Vec2f& b, const Vec2f& c) {
    float d1 = cross2d(a, b, p);
    float d2 = cross2d(b, c, p);
    float d3 = cross2d(c, a, p);

    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

// Check if vertex at index i is a convex ear (triangle formed with prev/next
// is convex and contains no other polygon vertices).
static bool is_ear(const std::vector<Vec2f>& poly, const std::vector<int>& indices, int i) {
    int n = static_cast<int>(indices.size());
    int prev = (i - 1 + n) % n;
    int next = (i + 1) % n;

    const Vec2f& a = poly[indices[prev]];
    const Vec2f& b = poly[indices[i]];
    const Vec2f& c = poly[indices[next]];

    // Must be convex (positive cross product for CCW polygon)
    if (cross2d(a, b, c) <= 0.0f) return false;

    // Check that no other vertex lies inside this triangle
    for (int j = 0; j < n; j++) {
        if (j == prev || j == i || j == next) continue;
        if (point_in_triangle(poly[indices[j]], a, b, c)) {
            return false;
        }
    }

    return true;
}

std::vector<Triangle> Triangulator::ear_clip(const std::vector<Vec2f>& polygon) {
    int n = static_cast<int>(polygon.size());
    if (n < 3) return {};

    // Compute signed area to determine winding
    float area = ContourGenerator::signed_area(polygon);

    // We need CCW winding for the algorithm to work correctly.
    // If CW (negative area), reverse the polygon.
    std::vector<Vec2f> poly = polygon;
    if (area < 0.0f) {
        std::reverse(poly.begin(), poly.end());
    }

    // Index list — we'll remove vertices as we clip ears
    std::vector<int> indices(n);
    for (int i = 0; i < n; i++) indices[i] = i;

    std::vector<Triangle> triangles;
    triangles.reserve(n - 2);

    int fail_count = 0;
    int max_fails = static_cast<int>(indices.size()) * 2; // safety limit

    while (indices.size() > 2 && fail_count < max_fails) {
        bool found_ear = false;
        int count = static_cast<int>(indices.size());

        for (int i = 0; i < count; i++) {
            if (!is_ear(poly, indices, i)) continue;

            // Clip this ear
            int prev = (i - 1 + count) % count;
            int next = (i + 1) % count;

            Triangle tri;
            tri.a = poly[indices[prev]];
            tri.b = poly[indices[i]];
            tri.c = poly[indices[next]];
            triangles.push_back(tri);

            indices.erase(indices.begin() + i);
            found_ear = true;
            fail_count = 0;
            break;
        }

        if (!found_ear) {
            fail_count++;
            // Rotate the index list to try different starting points
            if (!indices.empty()) {
                int front = indices.front();
                indices.erase(indices.begin());
                indices.push_back(front);
            }
        }
    }

    return triangles;
}

} // namespace engine::physics
