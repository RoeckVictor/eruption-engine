#pragma once

#include "engine/platform/Window.h"
#include "engine/platform/Input.h"
#include "engine/platform/Timer.h"
#include "engine/graphics/RenderContext.h"
#include "engine/core/EventBus.h"
#include "engine/core/EngineConfig.h"
#include "engine/core/SubsystemRegistry.h"
#include "engine/asset/AssetDatabase.h"
#include "engine/scene/SceneManager.h"
#include "engine/core/Logger.h"
#include <any>
#include <cstdlib>

namespace engine {

class Application;

class Engine {
public:
    Engine() = default;
    explicit Engine(const EngineConfig& cfg) : m_config(cfg) {}

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// Initialize engine with window and subsystems.
    /// If config_file_path is provided, loads config from JSON (falls back to defaults on error).
    bool init(const char* title, int width, int height, const char* config_file_path = nullptr);
    void run(Application& app);
    void shutdown();

    // --- Subsystem accessors ---

    platform::Window& window() { return m_window; }
    const platform::Window& window() const { return m_window; }
    platform::Input& input() { return m_input; }
    const platform::Input& input() const { return m_input; }
    platform::Timer& timer() { return m_timer; }
    const platform::Timer& timer() const { return m_timer; }
    graphics::RenderContext& render_context() { return m_render_context; }
    const graphics::RenderContext& render_context() const { return m_render_context; }

    EventBus& events() { return m_events; }
    asset::AssetDatabase& assets() { return m_assets; }
    const asset::AssetDatabase& assets() const { return m_assets; }
    scene::SceneManager& scenes() { return m_scenes; }
    const scene::SceneManager& scenes() const { return m_scenes; }

    /// Subsystem registry for extensible subsystem lookup.
    /// Built-in subsystems are pre-registered; custom ones can be added.
    SubsystemRegistry& subsystems() { return m_subsystems; }
    const SubsystemRegistry& subsystems() const { return m_subsystems; }

    /// Access engine configuration.
    const EngineConfig& config() const { return m_config; }

    /// Store a game-specific context object on the engine.
    /// Systems retrieve it in their init() to resolve dependencies.
    /// Type-safe: app_context<T>() will throw if T doesn't match what was set.
    template<typename T>
    void set_app_context(T& ctx) { m_app_ctx = &ctx; }

    template<typename T>
    T& app_context() const {
        auto* ptr = std::any_cast<T*>(&m_app_ctx);
        if (!ptr || !*ptr) {
            Logger::instance().error("Engine", "App context not set or type mismatch — aborting");
            std::abort();
        }
        return **ptr;
    }

    /// Set the framebuffer clear color (called before render systems each frame).
    void set_clear_color(float r, float g, float b) {
        m_clear_r = r; m_clear_g = g; m_clear_b = b;
    }

private:
    EngineConfig m_config;
    platform::Window m_window;
    platform::Input m_input;
    platform::Timer m_timer;
    graphics::RenderContext m_render_context;
    EventBus m_events;
    asset::AssetDatabase m_assets;
    scene::SceneManager m_scenes;
    SubsystemRegistry m_subsystems;

    std::any m_app_ctx;

    float m_clear_r = 0.0f;
    float m_clear_g = 0.0f;
    float m_clear_b = 0.0f;
};

} // namespace engine
