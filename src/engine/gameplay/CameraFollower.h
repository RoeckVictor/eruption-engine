#pragma once

namespace engine::gameplay {

/// Camera follow target component.
///
/// Marks an entity as a camera target. The CameraFollowSystem
/// will smoothly move the camera to follow this entity.
struct CameraFollower {
    /// Vertical offset as fraction of screen height.
    /// 0.0 = bottom of screen, 0.5 = center, 1.0 = top
    float offset_y_fraction = 0.33f;

    /// Whether this target is currently active.
    /// Useful for switching between multiple targets.
    bool active = true;
};

} // namespace engine::gameplay
