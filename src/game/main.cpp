#include "engine/core/Engine.h"
#include "game/EruptionApp.h"
#include "game/GameLog.h"

int main() {
    GAME_LOG("=== Eruption Engine ===");

    engine::Engine engine;
    if (!engine.init("Eruption", 1024, 768)) return 1;

    game::EruptionApp app;
    engine.run(app);

    engine.shutdown();
    GAME_LOG("Eruption shut down cleanly");
    return 0;
}
