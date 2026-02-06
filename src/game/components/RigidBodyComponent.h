#pragma once

namespace engine::physics { class PixelBody; }

namespace game {

/// Marks an entity as having a pixel-based rigid body managed by PixelBodyManager.
struct RigidBodyComponent {
    engine::physics::PixelBody* body = nullptr;
};

} // namespace game
