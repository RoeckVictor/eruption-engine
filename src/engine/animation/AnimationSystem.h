#pragma once

#include "engine/core/System.h"
#include <entt/fwd.hpp>

namespace engine::animation {

/// Ticks all Animator components each frame, advancing frames based on elapsed time.
/// Game registers this in the update phase.
class AnimationSystem : public engine::System {
public:
    void set_registry(entt::registry* reg) { m_registry = reg; }

    void update(engine::Engine& engine, float dt) override;

private:
    entt::registry* m_registry = nullptr;
};

} // namespace engine::animation
