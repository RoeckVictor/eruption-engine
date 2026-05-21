#pragma once

namespace engine::audio {

// ECS component marking an entity as the audio listener.
// Position is taken from the entity's Transform.
// Only the first active AudioListener in the scene is used.
struct AudioListener {
    bool enabled = true;
};

}
