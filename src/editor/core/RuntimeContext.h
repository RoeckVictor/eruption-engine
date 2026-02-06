#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>

namespace engine {
class Engine;
}

namespace editor {

/// Editor/Runtime state machine.
enum class PlayState {
    Editing,    // Normal editor mode
    Playing,    // Game is running
    Paused      // Game is paused (can step frames)
};

/// Manages the runtime context for play mode.
/// Creates an isolated copy of the scene for gameplay,
/// preserving the original for when play mode stops.
class RuntimeContext {
public:
    RuntimeContext();
    ~RuntimeContext();

    /// Initialize with the editor's scene registry.
    void init(entt::registry* editor_registry);

    /// Get the current play state.
    PlayState state() const { return m_state; }

    /// Check if we're in any play mode (Playing or Paused).
    bool is_playing() const { return m_state != PlayState::Editing; }

    /// Check if currently paused.
    bool is_paused() const { return m_state == PlayState::Paused; }

    /// Enter play mode - snapshots scene and starts runtime.
    void play();

    /// Pause the runtime.
    void pause();

    /// Resume from pause.
    void resume();

    /// Stop play mode - restores original scene state.
    void stop();

    /// Step one frame while paused.
    void step_frame();

    /// Update the runtime (called each frame when playing).
    void update(float dt);

    /// Get the runtime registry (the copy used during play mode).
    entt::registry* runtime_registry() { return m_runtime_registry.get(); }
    const entt::registry* runtime_registry() const { return m_runtime_registry.get(); }

    /// Get time spent in current play session.
    float play_time() const { return m_play_time; }

    /// Get the frame count since play started.
    uint64_t frame_count() const { return m_frame_count; }

private:
    void snapshot_scene();
    void restore_scene();
    void create_runtime_copy();

    entt::registry* m_editor_registry = nullptr;
    std::unique_ptr<entt::registry> m_runtime_registry;

    // Scene snapshot stored as serialized data
    std::string m_scene_snapshot;

    PlayState m_state = PlayState::Editing;
    float m_play_time = 0.0f;
    uint64_t m_frame_count = 0;
    bool m_step_requested = false;
};

} // namespace editor
