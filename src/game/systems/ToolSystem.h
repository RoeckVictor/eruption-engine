#pragma once

#include "engine/core/System.h"
#include "game/asset/PxgSprite.h"
#include <entt/fwd.hpp>

namespace engine::render { struct Camera2D; }
namespace engine::simulation { class PixelGrid; }
namespace engine::physics { class PixelBodyManager; }

namespace game {

class ToolSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void update(engine::Engine& engine, float dt) override;

private:
    void erase_body_pixels(float world_x, float world_y, int radius);

    entt::registry* m_registry = nullptr;
    engine::simulation::PixelGrid* m_grid = nullptr;
    engine::render::Camera2D* m_camera = nullptr;
    engine::physics::PixelBodyManager* m_body_manager = nullptr;

    PxgSprite m_paste_sprite;
    BodyPrefab m_body_prefab;
    bool m_paste_loaded = false;
    bool m_body_loaded = false;
};

} // namespace game
