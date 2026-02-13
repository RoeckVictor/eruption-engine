#include "engine/core/Engine.h"
#include "engine/core/Application.h"
#include "engine/core/Log.h"

namespace engine {

bool Engine::init(const char* title, int width, int height, const char* config_file_path) {
    // Load config if provided, otherwise use defaults
    if (config_file_path) {
        auto config_result = EngineConfig::load_from_json(config_file_path);
        if (config_result.is_ok()) {
            m_config = config_result.value();
        } else {
            ENGINE_ERR("Failed to load config from '%s': %s (using defaults)",
                       config_file_path, config_result.error().message.c_str());
            m_config = EngineConfig::defaults();
        }
    } else {
        m_config = EngineConfig::defaults();
    }
    if (!platform::Window::init_platform()) {
        ENGINE_ERR("Failed to initialize platform");
        return false;
    }

    if (!m_window.init(title, width, height)) {
        ENGINE_ERR("Failed to initialize window");
        platform::Window::shutdown_platform();
        return false;
    }

    m_timer.init(m_config.max_delta_time, m_config.fixed_timestep);

    if (!m_assets.init(m_config.asset_base_path)) {
        ENGINE_ERR("Failed to initialize asset database");
        m_window.shutdown();
        platform::Window::shutdown_platform();
        return false;
    }

    // Apply default clear color from config
    m_clear_r = m_config.clear_color_r;
    m_clear_g = m_config.clear_color_g;
    m_clear_b = m_config.clear_color_b;

    // Register built-in subsystems in the registry for extensible lookup
    m_subsystems.register_subsystem(m_window, "Window");
    m_subsystems.register_subsystem(m_input, "Input");
    m_subsystems.register_subsystem(m_timer, "Timer");
    m_subsystems.register_subsystem(m_render_context, "RenderContext");
    m_subsystems.register_subsystem(m_events, "EventBus");
    m_subsystems.register_subsystem(m_assets, "AssetDatabase");
    m_subsystems.register_subsystem(m_scenes, "SceneManager");

    ENGINE_LOG("Engine initialized");
    return true;
}

void Engine::run(Application& app) {
    // Application initializes and pushes initial scene
    if (!app.on_init(*this)) {
        ENGINE_ERR("Application initialization failed");
        app.on_shutdown(*this);
        return;
    }

    // Process any deferred scene operations from on_init()
    m_scenes.process_pending(*this);

    // Ensure at least one scene is active
    if (!m_scenes.has_active_scene()) {
        ENGINE_ERR("Application must push at least one scene in on_init()");
        app.on_shutdown(*this);
        return;
    }

    // Reset timer so the first frame delta doesn't include init time
    m_timer.init(m_config.max_delta_time, m_config.fixed_timestep);

    while (!m_window.should_close()) {
        m_timer.update();
        m_window.poll_events();
        m_input.update(m_window);

        float dt = static_cast<float>(m_timer.delta_time());

        // Poll asset hot-reload before update phase
        m_assets.poll_hot_reload();

        // Process deferred scene operations (push/pop/replace)
        m_scenes.process_pending(*this);

        // Per-frame update
        m_scenes.update(*this, dt);
        app.on_update(*this, dt);

        // Fixed timestep updates (physics, simulation).
        // Cap iterations to avoid spiral-of-death after long stalls.
        float fixed_dt = static_cast<float>(m_timer.fixed_dt());
        int steps = 0;
        while (m_timer.consume_fixed_step() && steps < m_config.max_fixed_steps) {
            m_scenes.fixed_update(*this, fixed_dt);
            app.on_fixed_update(*this, fixed_dt);
            ++steps;
        }

        // Render: clear, render scene, then application hook
        m_render_context.set_viewport(0, 0, m_window.width(), m_window.height());
        m_render_context.clear(m_clear_r, m_clear_g, m_clear_b);
        m_scenes.render(*this);
        app.on_render(*this);

        m_window.swap_buffers();
    }

    m_scenes.shutdown_all(*this);
    app.on_shutdown(*this);
}

void Engine::shutdown() {
    m_subsystems.clear();
    m_assets.shutdown();
    m_window.shutdown();
    platform::Window::shutdown_platform();
    ENGINE_LOG("Engine shut down");
}

} // namespace engine
