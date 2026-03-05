#pragma once

#include <cstdint>
#include <vector>

namespace engine::physics {

struct Vec2f {
    float x = 0.0f, y = 0.0f;
};

struct Contour {
    std::vector<Vec2f> vertices;
    bool is_hole = false;
};

// Information about solid pixels at chunk edges from neighboring chunks
// Used to prevent false diagonal edges at chunk boundaries where terrain continues
struct NeighborEdges {
    const bool* left = nullptr;
    const bool* right = nullptr;
    const bool* top = nullptr;
    const bool* bottom = nullptr;

    bool top_left = false;
    bool top_right = false;
    bool bottom_left = false;
    bool bottom_right = false;
};

// Generates simplified polygon contours from binary pixel grids
class ContourGenerator {
public:
    static std::vector<Contour> generate(
        const bool* solid_grid, int width, int height,
        float epsilon = 1.5f,
        const NeighborEdges* neighbors = nullptr);

    struct Segment {
        Vec2f a, b;
    };

    static std::vector<Segment> marching_squares(const bool* grid, int w, int h,
                                                  const NeighborEdges* neighbors = nullptr);

    static std::vector<std::vector<Vec2f>> join_segments(const std::vector<Segment>& segs);

    static std::vector<Vec2f> simplify_rdp(const std::vector<Vec2f>& poly, float epsilon);

    static float signed_area(const std::vector<Vec2f>& poly);
};

}
