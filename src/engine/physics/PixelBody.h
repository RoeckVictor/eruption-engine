#pragma once

#include "engine/physics/PhysicsWorld.h"
#include <cstdint>
#include <vector>

namespace engine::physics {

/// A rigid body composed of pixels.
///
/// Owns a local pixel buffer (material IDs + categories in local body space) and
/// a Box2D body. When pixels are destroyed, the body is marked dirty and its
/// collision shapes can be recomputed from the current pixel state.
///
/// Coordinates: local pixel (0,0) is the top-left of the pixel buffer.
/// The body's center of mass in local space is computed from the pixel distribution.
class PixelBody {
public:
    /// Initialize from pixel buffers. Creates a Box2D body and computes collision shapes.
    /// @param world          The physics world to create the body in.
    /// @param materials      Row-major material IDs (0 = empty). Copied internally.
    /// @param categories     Row-major physics categories (from MaterialDefs.h). Copied internally.
    /// @param width          Buffer width in pixels.
    /// @param height         Buffer height in pixels.
    /// @param world_px       World X position (pixels) for the body center.
    /// @param world_py       World Y position (pixels) for the body center.
    /// @param is_dynamic     True for dynamic body, false for static.
    /// @param indestructible If true, pixels cannot be destroyed by tools/damage.
    bool init(PhysicsWorld& world, const uint8_t* materials, const uint8_t* categories,
              int width, int height, float world_px, float world_py, bool is_dynamic = true,
              bool indestructible = false);
    void shutdown(PhysicsWorld& world);

    // --- Pixel manipulation ---

    uint8_t get_pixel(int lx, int ly) const;
    void set_pixel(int lx, int ly, uint8_t material);
    void destroy_pixel(int lx, int ly);

    // --- Collision shape management ---

    bool is_dirty() const { return m_dirty; }
    void mark_dirty() { m_dirty = true; }

    /// Recompute collision shapes from current pixel state.
    /// Destroys all existing shapes and creates new ones.
    void recompute_shapes(PhysicsWorld& world, float simplify_epsilon = 0.25f);

    // --- Connected component analysis ---

    /// Count disconnected pixel groups. Returns > 1 if the body has split.
    int count_components() const;

    /// Data for a single connected component extracted from the body.
    struct Component {
        std::vector<uint8_t> materials;   // material IDs
        std::vector<uint8_t> categories;  // physics categories
        int width, height;
        int offset_x, offset_y;  // offset relative to original body's local space
        float center_x, center_y; // center of mass in original body's local space
    };

    /// Extract each connected component as a separate pixel buffer.
    std::vector<Component> extract_components() const;

    // --- Transform queries (pixel space) ---

    b2BodyId body_id() const { return m_body_id; }

    /// Get world position of the body center in pixels.
    float world_x(const PhysicsWorld& world) const;
    float world_y(const PhysicsWorld& world) const;
    float rotation(const PhysicsWorld& world) const;

    // --- Buffer access ---

    const uint8_t* materials() const { return m_materials.data(); }
    const uint8_t* categories() const { return m_categories.data(); }
    int width() const { return m_width; }
    int height() const { return m_height; }

    /// Number of non-empty pixels.
    int pixel_count() const { return m_pixel_count; }

    /// Center of mass in local pixel coordinates.
    float local_center_x() const { return m_local_center_x; }
    float local_center_y() const { return m_local_center_y; }

    /// Returns true if this body's pixels cannot be destroyed.
    bool is_indestructible() const { return m_indestructible; }

private:
    void compute_center_of_mass();
    void build_solid_grid(bool* solid) const;

    std::vector<uint8_t> m_materials;   // material IDs (game-defined)
    std::vector<uint8_t> m_categories;  // physics categories (engine-defined)
    int m_width = 0, m_height = 0;
    int m_pixel_count = 0;
    b2BodyId m_body_id = b2_nullBodyId;
    bool m_dirty = false;
    bool m_indestructible = false;
    float m_local_center_x = 0.0f;
    float m_local_center_y = 0.0f;
};

} // namespace engine::physics
