#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace engine::render { struct Camera2D; }

namespace game {

class CameraSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
    engine::render::Camera2D* m_camera = nullptr;
};

} // namespace game
