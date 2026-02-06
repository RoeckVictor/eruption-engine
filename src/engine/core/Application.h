#pragma once

namespace engine {

class Engine;

/// Base class for game applications.
///
/// The engine drives the main loop and system dispatch automatically.
/// Games implement on_init() to push the initial scene(s) to engine.scenes() and
/// set up initial game state. Optional per-frame hooks (on_update, etc.)
/// are called AFTER the corresponding system phase, for game-specific
/// logic that doesn't fit into a system.
class Application {
public:
    virtual ~Application() = default;

    /// Called once before the main loop.  Register systems, create entities,
    /// load resources.  Return false to abort.
    virtual bool on_init(Engine& engine) = 0;

    /// Called once after the main loop ends (or if init fails).
    virtual void on_shutdown(Engine& engine) = 0;

    /// Optional hook called each frame after update systems run.
    virtual void on_update(Engine& /*engine*/, float /*dt*/) {}

    /// Optional hook called each fixed step after fixed-update systems run.
    virtual void on_fixed_update(Engine& /*engine*/, float /*fixed_dt*/) {}

    /// Optional hook called each frame after render systems run.
    virtual void on_render(Engine& /*engine*/) {}
};

} // namespace engine
