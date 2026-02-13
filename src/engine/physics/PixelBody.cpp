#include "engine/physics/PixelBody.h"
#include "engine/physics/ContourGenerator.h"
#include "engine/physics/Triangulator.h"
#include "engine/core/Log.h"
#include <cstring>
#include <queue>
#include <cmath>
#include <memory>

namespace engine::physics {

bool PixelBody::init(PhysicsWorld& world, const uint8_t* materials, const uint8_t* categories,
                     int width, int height, float world_px, float world_py, bool is_dynamic,
                     bool indestructible) {
    m_width = width;
    m_height = height;
    int total = width * height;
    m_materials.assign(materials, materials + total);
    m_categories.assign(categories, categories + total);
    m_dirty = false;
    m_indestructible = indestructible;

    compute_center_of_mass();

    if (m_pixel_count == 0) {
        ENGINE_ERR("PixelBody: no solid pixels in buffer");
        return false;
    }

    // Create the Box2D body at the given world position
    if (is_dynamic) {
        m_body_id = world.create_dynamic_body(world_px, world_py);
    } else {
        m_body_id = world.create_static_body(world_px, world_py);
    }

    if (!b2Body_IsValid(m_body_id)) {
        ENGINE_ERR("PixelBody: failed to create Box2D body");
        return false;
    }

    // Build initial collision shapes
    recompute_shapes(world);

    return true;
}

void PixelBody::shutdown(PhysicsWorld& world) {
    if (b2Body_IsValid(m_body_id)) {
        world.destroy_body(m_body_id);
        m_body_id = b2_nullBodyId;
    }
    m_materials.clear();
    m_categories.clear();
    m_width = m_height = 0;
    m_pixel_count = 0;
}

// --- Pixel manipulation ---

uint8_t PixelBody::get_pixel(int lx, int ly) const {
    if (lx < 0 || lx >= m_width || ly < 0 || ly >= m_height) return 0;
    return m_materials[ly * m_width + lx];
}

void PixelBody::set_pixel(int lx, int ly, uint8_t material) {
    if (lx < 0 || lx >= m_width || ly < 0 || ly >= m_height) return;
    int idx = ly * m_width + lx;
    if (m_materials[idx] != material) {
        if (m_materials[idx] == 0 && material != 0) m_pixel_count++;
        else if (m_materials[idx] != 0 && material == 0) m_pixel_count--;
        m_materials[idx] = material;
        m_dirty = true;
    }
}

void PixelBody::destroy_pixel(int lx, int ly) {
    if (lx < 0 || lx >= m_width || ly < 0 || ly >= m_height) return;
    int idx = ly * m_width + lx;
    if (m_materials[idx] != 0) {
        m_materials[idx] = 0;
        m_categories[idx] = 0; // CAT_EMPTY
        m_pixel_count--;
        m_dirty = true;
    }
}

// --- Collision shape management ---

void PixelBody::recompute_shapes(PhysicsWorld& world, float simplify_epsilon) {
    if (!b2Body_IsValid(m_body_id)) return;

    // Remove existing shapes
    world.destroy_all_shapes(m_body_id);

    // Recompute center of mass
    compute_center_of_mass();

    if (m_pixel_count == 0) {
        m_dirty = false;
        return;
    }

    // Build solid grid (can't use std::vector<bool> — it's bit-packed and has no .data())
    auto solid = std::make_unique<bool[]>(m_width * m_height);
    build_solid_grid(solid.get());

    // Generate contours
    auto contours = ContourGenerator::generate(solid.get(), m_width, m_height, simplify_epsilon);

    // Triangulate each outer contour and create Box2D fixtures.
    // Vertices are in local pixel space; we need to convert them to local meter space
    // relative to the body center.
    float scale = 1.0f / world.pixels_per_meter();

    for (auto& contour : contours) {
        if (contour.is_hole) continue; // Skip holes for now
        if (contour.vertices.size() < 3) continue;

        auto triangles = Triangulator::ear_clip(contour.vertices);

        for (auto& tri : triangles) {
            // Skip degenerate triangles (area < 0.5 pixel²)
            float cross = (tri.b.x - tri.a.x) * (tri.c.y - tri.a.y) -
                          (tri.c.x - tri.a.x) * (tri.b.y - tri.a.y);
            if (std::abs(cross) < 1.0f) continue;

            // Convert from local pixel coords to local meter coords relative to body center
            b2Vec2 verts[3];
            verts[0] = {(tri.a.x - m_local_center_x) * scale,
                        (tri.a.y - m_local_center_y) * scale};
            verts[1] = {(tri.b.x - m_local_center_x) * scale,
                        (tri.b.y - m_local_center_y) * scale};
            verts[2] = {(tri.c.x - m_local_center_x) * scale,
                        (tri.c.y - m_local_center_y) * scale};

            world.add_polygon_shape(m_body_id, verts, 3);
        }
    }

    m_dirty = false;
}

// --- Connected component analysis ---

std::pair<std::vector<int>, int> PixelBody::label_components() const {
    std::vector<int> labels(m_width * m_height, 0);
    int num_components = 0;
    std::queue<int> bfs;

    for (int i = 0; i < m_width * m_height; i++) {
        if (m_materials[i] == 0 || labels[i] != 0) continue;

        num_components++;
        labels[i] = num_components;
        bfs.push(i);

        while (!bfs.empty()) {
            int idx = bfs.front();
            bfs.pop();

            int x = idx % m_width;
            int y = idx / m_width;

            // 4-connected neighbors
            int neighbors[4] = {-1, -1, -1, -1};
            if (x > 0)            neighbors[0] = idx - 1;
            if (x < m_width - 1)  neighbors[1] = idx + 1;
            if (y > 0)            neighbors[2] = idx - m_width;
            if (y < m_height - 1) neighbors[3] = idx + m_width;

            for (int ni = 0; ni < 4; ni++) {
                int n = neighbors[ni];
                if (n >= 0 && m_materials[n] != 0 && labels[n] == 0) {
                    labels[n] = num_components;
                    bfs.push(n);
                }
            }
        }
    }

    return {std::move(labels), num_components};
}

int PixelBody::count_components() const {
    if (m_pixel_count <= 1) return m_pixel_count;
    auto [labels, count] = label_components();
    return count;
}

std::vector<PixelBody::Component> PixelBody::extract_components() const {
    if (m_pixel_count == 0) return {};

    auto [labels, num_components] = label_components();

    if (num_components <= 1) return {}; // No split occurred

    // Extract each component
    std::vector<Component> components(num_components);

    // First pass: compute bounding boxes
    struct AABB { int min_x, min_y, max_x, max_y; };
    std::vector<AABB> bounds(num_components, {m_width, m_height, 0, 0});

    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            int label = labels[y * m_width + x];
            if (label == 0) continue;
            auto& bb = bounds[label - 1];
            bb.min_x = std::min(bb.min_x, x);
            bb.min_y = std::min(bb.min_y, y);
            bb.max_x = std::max(bb.max_x, x);
            bb.max_y = std::max(bb.max_y, y);
        }
    }

    // Second pass: extract materials, categories, and compute center of mass
    for (int ci = 0; ci < num_components; ci++) {
        auto& comp = components[ci];
        auto& bb = bounds[ci];
        comp.offset_x = bb.min_x;
        comp.offset_y = bb.min_y;
        comp.width = bb.max_x - bb.min_x + 1;
        comp.height = bb.max_y - bb.min_y + 1;
        int comp_size = comp.width * comp.height;
        comp.materials.resize(comp_size, 0);
        comp.categories.resize(comp_size, 0);

        float sum_x = 0.0f, sum_y = 0.0f;
        int count = 0;

        for (int y = bb.min_y; y <= bb.max_y; y++) {
            for (int x = bb.min_x; x <= bb.max_x; x++) {
                if (labels[y * m_width + x] != ci + 1) continue;
                int local_x = x - bb.min_x;
                int local_y = y - bb.min_y;
                int src_idx = y * m_width + x;
                int dst_idx = local_y * comp.width + local_x;
                comp.materials[dst_idx] = m_materials[src_idx];
                comp.categories[dst_idx] = m_categories[src_idx];
                sum_x += static_cast<float>(x) + 0.5f;
                sum_y += static_cast<float>(y) + 0.5f;
                count++;
            }
        }

        if (count > 0) {
            comp.center_x = sum_x / static_cast<float>(count);
            comp.center_y = sum_y / static_cast<float>(count);
        }
    }

    return components;
}

// --- Transform queries ---

float PixelBody::world_x(const PhysicsWorld& world) const {
    return world.get_body_position(m_body_id).x;
}

float PixelBody::world_y(const PhysicsWorld& world) const {
    return world.get_body_position(m_body_id).y;
}

float PixelBody::rotation(const PhysicsWorld& world) const {
    return world.get_body_angle(m_body_id);
}

// --- Private helpers ---

void PixelBody::compute_center_of_mass() {
    float sum_x = 0.0f, sum_y = 0.0f;
    m_pixel_count = 0;

    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            if (m_materials[y * m_width + x] != 0) {
                sum_x += static_cast<float>(x) + 0.5f;
                sum_y += static_cast<float>(y) + 0.5f;
                m_pixel_count++;
            }
        }
    }

    if (m_pixel_count > 0) {
        m_local_center_x = sum_x / static_cast<float>(m_pixel_count);
        m_local_center_y = sum_y / static_cast<float>(m_pixel_count);
    } else {
        m_local_center_x = static_cast<float>(m_width) * 0.5f;
        m_local_center_y = static_cast<float>(m_height) * 0.5f;
    }
}

void PixelBody::build_solid_grid(bool* solid) const {
    for (int i = 0; i < m_width * m_height; i++) {
        solid[i] = (m_materials[i] != 0);
    }
}

} // namespace engine::physics
