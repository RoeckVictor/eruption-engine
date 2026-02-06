#pragma once

namespace engine {

class Engine;

/// Base class for engine and game systems.
/// Systems follow the same lifecycle as Application:
///   init -> [update / fixed_update / render] -> shutdown
class System {
public:
    virtual ~System() = default;

    virtual bool init(Engine&) { return true; }
    virtual void shutdown() {}

    virtual void update(Engine&, float) {}
    virtual void fixed_update(Engine&, float) {}
    virtual void render(Engine&) {}
};

} // namespace engine
