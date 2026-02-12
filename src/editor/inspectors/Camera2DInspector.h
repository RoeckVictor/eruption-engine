#pragma once

namespace engine::render {
struct Camera2D;
}

namespace editor {

/// Custom inspector for Camera2D component.
/// Provides specialized UI for camera position, zoom, and limits.
class Camera2DInspector {
public:
    /// Draw inspector UI for Camera2D component.
    /// Returns true if any value was modified.
    static bool draw(engine::render::Camera2D& camera);
};

} // namespace editor
