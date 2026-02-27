#pragma once

#include "engine/physics/ContourGenerator.h"
#include "engine/physics/Triangulator.h"
#include "engine/simulation/IPixelGridLoader.h"
#include <vector>
#include <memory>
#include <cmath>
#include <utility>

namespace engine::physics {

struct PixelGridMesh {
    std::vector<Triangle> triangles;
    int width = 0;
    int height = 0;
};

struct GridToLocalParams {
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    float grid_height = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

// Utility for triangulating pixel grids into collision meshes
// Used by both editor PhysicsPlayback and engine Box2DPhysicsSystem
class PixelGridTriangulation {
public:
    static PixelGridMesh triangulate(
        const simulation::LoadedPixelGridData* loaded_grid,
        float simplification,
        float min_contour_area);

    static inline std::pair<float, float> grid_to_local(const Vec2f& p, const GridToLocalParams& params) {
        float lx = (p.x - params.origin_x) * params.scale_x + params.offset_x;
        float ly = (params.grid_height - p.y - params.origin_y) * params.scale_y + params.offset_y;
        return {lx, ly};
    }

    static inline bool is_clockwise(float ax, float ay, float bx, float by, float cx, float cy) {
        float cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        return cross < 0.0f;
    }

    static inline float triangle_area(float ax, float ay, float bx, float by, float cx, float cy) {
        float cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        return std::abs(cross) * 0.5f;
    }
};

}