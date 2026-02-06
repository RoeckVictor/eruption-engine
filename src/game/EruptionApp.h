#pragma once

#include "engine/core/Application.h"

namespace game {

/// Top-level application. Pushes the active scene onto the engine's
/// SceneManager; all game state and systems live inside the scene.
class EruptionApp : public engine::Application {
public:
    bool on_init(engine::Engine& engine) override;
    void on_shutdown(engine::Engine& engine) override;
};

} // namespace game
