#include "PixelGridTriangulation.h"
#include "engine/core/Logger.h"

namespace engine::physics {

PixelGridMesh PixelGridTriangulation::triangulate(
    const simulation::LoadedPixelGridData* loaded_grid,
    float simplification,
    float min_contour_area) {

    PixelGridMesh mesh;

    if (!loaded_grid || loaded_grid->material_ids.empty()) {
        return mesh;
    }

    int width = loaded_grid->width;
    int height = loaded_grid->height;

    if (width <= 0 || height <= 0) {
        return mesh;
    }

    mesh.width = width;
    mesh.height = height;

    auto solid_grid = std::make_unique<bool[]>(width * height);
    for (int i = 0; i < width * height; ++i) {
        solid_grid[i] = (loaded_grid->material_ids[i] != 0);
    }

    float epsilon = simplification * 2.0f;
    auto contours = ContourGenerator::generate(solid_grid.get(), width, height, epsilon);

    if (contours.empty()) {
        return mesh;
    }

    for (const auto& contour : contours) {
        if (contour.is_hole) {
            continue;
        }

        float area = std::abs(ContourGenerator::signed_area(contour.vertices));
        if (area < min_contour_area) {
            continue;
        }

        auto triangles = Triangulator::ear_clip(contour.vertices);
        mesh.triangles.insert(mesh.triangles.end(), triangles.begin(), triangles.end());
    }

    return mesh;
}

}