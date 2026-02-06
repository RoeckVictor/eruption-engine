#pragma once

#include "engine/core/System.h"
#include "engine/render/SpriteRenderer.h"
#include <entt/fwd.hpp>

namespace engine::render { struct Camera2D; }

namespace game {

class EntityRenderSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void shutdown() override;
    void render(engine::Engine& engine) override;

private:
    entt::registry* m_registry = nullptr;
    engine::render::Camera2D* m_camera = nullptr;
    engine::render::SpriteRenderer m_sprite_renderer;
};

} // namespace game
