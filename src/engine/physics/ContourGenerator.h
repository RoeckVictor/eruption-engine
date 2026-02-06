#pragma once

#include <cstdint>
#include <vector>

namespace engine::physics {

/// A 2D point used for contour generation (local pixel coordinates).
struct Vec2f {
    float x = 0.0f, y = 0.0f;
};

/// A closed contour extracted from a binary pixel grid.
struct Contour {
    std::vector<Vec2f> vertices;
    bool is_hole = false;
};

/// Generates simplified polygon contours from binary pixel grids.
///
/// The pipeline is:
///   1. Marching squares — extract edge segments from a binary grid
///   2. Segment joining — connect segments into closed polygon chains
///   3. RDP simplification — reduce vertex count while preserving shape
///   4. Winding classification — determine outer vs. hole contours
///
/// All coordinates are in local pixel space (0,0 is top-left of the grid).
class ContourGenerator {
public:
    /// Full pipeline: binary grid → simplified contours.
    /// @param solid_grid  Row-major boolean array (true = solid pixel).
    /// @param width       Grid width in pixels.
    /// @param height      Grid height in pixels.
    /// @param epsilon     RDP simplification tolerance (pixels). Higher = fewer vertices.
    static std::vector<Contour> generate(
        const bool* solid_grid, int width, int height,
        float epsilon = 1.5f);

    // --- Individual stages (exposed for testing) ---

    struct Segment {
        Vec2f a, b;
    };

    /// Extract edge segments via marching squares.
    static std::vector<Segment> marching_squares(const bool* grid, int w, int h);

    /// Join unordered segments into closed polygon chains.
    static std::vector<std::vector<Vec2f>> join_segments(const std::vector<Segment>& segs);

    /// Simplify a polygon using the Ramer-Douglas-Peucker algorithm.
    static std::vector<Vec2f> simplify_rdp(const std::vector<Vec2f>& poly, float epsilon);

    /// Compute the signed area of a polygon (positive = CCW, negative = CW).
    static float signed_area(const std::vector<Vec2f>& poly);
};

} // namespace engine::physics
