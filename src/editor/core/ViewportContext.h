#pragma once

#include <cmath>

namespace editor {

// Manages viewport camera state and grid/snap settings
// This is a value-type context (no callbacks or overrides)
struct ViewportContext {
    struct Camera {
        float x = 0.0f;
        float y = 0.0f;
        float zoom = 1.0f;

        void reset() {
            x = 0.0f;
            y = 0.0f;
            zoom = 1.0f;
        }
    };

    Camera camera;

    bool grid_visible = true;
    bool snap_enabled = false;
    float grid_size = 32.0f;
    bool local_space = false;

    float snap_to_grid(float value) const {
        if (!snap_enabled || grid_size <= 0.0f) {
            return value;
        }
        return std::round(value / grid_size) * grid_size;
    }

    void reset() {
        camera.reset();
        grid_visible = true;
        snap_enabled = false;
        grid_size = 32.0f;
        local_space = false;
    }
};

}
