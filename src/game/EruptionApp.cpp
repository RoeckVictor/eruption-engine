#include "game/EruptionApp.h"
#include "engine/core/Engine.h"
#include "game/scenes/DemoScene.h"
#include "game/GameEvents.h"
#include "game/GameLog.h"
#include <memory>

namespace game {

bool EruptionApp::on_init(engine::Engine& engine) {
    // App-level event: Escape → close window
    engine.events().subscribe<QuitRequestedEvent>(
        [&engine](const QuitRequestedEvent&) {
            engine.window().set_should_close(true);
        });

    // Push the demo scene (all game state and systems live inside it)
    engine.scenes().push(std::make_unique<DemoScene>());

    return true;
}

void EruptionApp::on_shutdown(engine::Engine& /*engine*/) {
    // Scene cleanup is handled by SceneManager::shutdown_all()
}

} // namespace game
