#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace game {

class GameInputSystem : public engine::System {
public:
    bool init(engine::Engine& engine) override;
    void update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
};

} // namespace game
