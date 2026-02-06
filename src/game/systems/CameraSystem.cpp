#include "game/systems/CameraSystem.h"
#include "engine/core/Engine.h"
#include "engine/render/Camera2D.h"
#include "game/GameContext.h"
#include "game/components/Components.h"
#include <entt/entt.hpp>
#include <algorithm>
#include <cmath>

namespace game {

bool CameraSystem::init(engine::Engine& engine) {
    auto& ctx = engine.app_context<GameContext>();
    m_registry = &ctx.registry;
    m_camera = &ctx.camera;
    return true;
}

void CameraSystem::update(engine::Engine& engine, float dt) {
    // Zoom from scroll input
    float scroll = engine.input().scroll_y();
    if (scroll != 0.0f) {
        static constexpr float ZOOM_FACTOR = 1.15f;
        if (scroll > 0)
            m_camera->zoom *= ZOOM_FACTOR;
        else
            m_camera->zoom *= (1.0f / ZOOM_FACTOR);

        m_camera->zoom = std::clamp(m_camera->zoom,
                                    m_camera->min_zoom, m_camera->max_zoom);
    }

    // Follow the first entity with a CameraTarget
    auto view = m_registry->view<const Transform, const CameraTarget>();
    if (view.begin() != view.end()) {
        auto entity = *view.begin();
        const auto& transform = view.get<const Transform>(entity);
        const auto& target = view.get<const CameraTarget>(entity);

        float screen_h = (float)engine.window().height();
        float vis_h = screen_h / m_camera->zoom;
        float goal_x = transform.x;
        float goal_y = transform.y - vis_h * target.offset_y_fraction;

        if (m_camera->smoothing <= 0.0f || dt <= 0.0f) {
            m_camera->x = goal_x;
            m_camera->y = goal_y;
        } else {
            float t = 1.0f - std::exp(-m_camera->smoothing * dt);
            m_camera->x += (goal_x - m_camera->x) * t;
            m_camera->y += (goal_y - m_camera->y) * t;
        }
    }
}

} // namespace game
