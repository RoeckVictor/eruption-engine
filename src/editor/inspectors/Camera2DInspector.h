#pragma once

namespace engine::render {
struct Camera2D;
}

namespace editor {

// Custom inspector for Camera2D component
// Provides specialized UI for camera position, zoom, and limits
class Camera2DInspector {
public:
    static bool draw(engine::render::Camera2D& camera);
};

}
