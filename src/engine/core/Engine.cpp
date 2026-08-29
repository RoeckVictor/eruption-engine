#include "engine/core/Engine.h"
#include "engine/core/Application.h"
#include "engine/core/Log.h"
#include "engine/rhi/RHI.h"
#include "engine/profiler/Profiler.h"
#include "engine/save/SaveSystem.h"

namespace engine {

Engine::~Engine() = default;

rhi::RHIContext* Engine::rhi_context() {
    return m_rhi_device ? m_rhi_device->context() : nullptr;
}

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

    // Select graphics API for window based on configured backend
    platform::GraphicsAPI window_api = platform::GraphicsAPI::OpenGL;
    if (m_config.graphics_backend == rhi::Backend::Vulkan) {
        window_api = platform::GraphicsAPI::Vulkan;
    } else if (m_config.graphics_backend != rhi::Backend::OpenGL) {
        ENGINE_ERR("Unsupported graphics backend, falling back to OpenGL");
        m_config.graphics_backend = rhi::Backend::OpenGL;
    }

    if (!m_window.init(title, width, height, window_api)) {
        ENGINE_ERR("Failed to initialize window");
        platform::Window::shutdown_platform();
        return false;
    }

    // Initialize RHI device
    rhi::RHIDeviceCreateInfo device_info;
    device_info.gl_proc_address = platform::Window::get_gl_proc_address;
    device_info.window_handle = m_window.glfw_handle();
    m_rhi_device = rhi::create_rhi_device(m_config.graphics_backend, device_info);
    if (!m_rhi_device) {
        ENGINE_ERR("Failed to initialize RHI device");
        m_window.shutdown();
        platform::Window::shutdown_platform();
        return false;
    }
    ENGINE_LOG("RHI initialized: %s (%s)", m_rhi_device->backend_name(), m_rhi_device->renderer_name());

    // Initialize GPU profiler via RHI device
    m_gpu_profiler = m_rhi_device->create_gpu_profiler();
    if (m_gpu_profiler) {
        ENGINE_LOG("GPU profiler initialized");
    } else {
        ENGINE_LOG_WARN("GPU profiler not supported on this system");
    }

    // Set global RHI device for graphics classes to access
    rhi::set_current_device(m_rhi_device.get());

    // Wire up legacy RenderContext to use RHI
    m_render_context.set_rhi_context(m_rhi_device->context());

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

    // Initialize audio engine
    m_audio_engine = std::make_unique<audio::AudioEngine>();
    if (m_audio_engine->init(m_config.audio_sample_rate)) {
        m_audio_engine->set_master_volume(m_config.master_volume);
        m_subsystems.register_subsystem(*m_audio_engine, "AudioEngine");
    } else {
        ENGINE_LOG_WARN("Audio engine failed to initialize -- audio will be disabled");
        m_audio_engine.reset();
    }

    // Initialize save system
    m_save_system = std::make_unique<save::SaveSystem>();
    m_save_system->init(title ? title : "EruptionGame");
    m_subsystems.register_subsystem(*m_save_system, "SaveSystem");

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
        profiler::Profiler::instance().begin_frame();

        {
            PROFILE_SCOPE("FrameStart");
            m_timer.update();
            m_window.poll_events();
            m_input.update(m_window);
            m_action_map.evaluate(m_input);
        }

        float dt = static_cast<float>(m_timer.delta_time());

        {
            PROFILE_SCOPE("Assets");
            m_assets.poll_hot_reload();
        }

        // Begin the GPU frame early so compute dispatches during Update/FixedUpdate
        // can record into the command buffer. In Vulkan, all GPU commands must be
        // recorded between begin_frame() and end_frame().
        auto* ctx = m_rhi_device->context();
        if (m_gpu_profiler) m_gpu_profiler->begin_frame();
        {
            PROFILE_SCOPE("RHI::begin_frame");
            ctx->begin_frame();
        }

        // Process deferred scene operations (push/pop/replace)
        m_scenes.process_pending(*this);

        {
            PROFILE_SCOPE("Update");
            m_scenes.update(*this, dt);
            app.on_update(*this, dt);
        }

        // Fixed timestep updates (physics, simulation).
        // Cap iterations to avoid spiral-of-death after long stalls.
        {
            PROFILE_SCOPE("FixedUpdate");
            float fixed_dt = static_cast<float>(m_timer.fixed_dt());
            int steps = 0;
            while (m_timer.consume_fixed_step() && steps < m_config.max_fixed_steps) {
                m_scenes.fixed_update(*this, fixed_dt);
                app.on_fixed_update(*this, fixed_dt);
                ++steps;
            }
        }

        // Render: set up viewport, clear, render scene, then application hook
        {
            PROFILE_SCOPE("Render");
            {
                PROFILE_SCOPE("RHI::setup");
                ctx->set_viewport(0, 0, m_window.width(), m_window.height());
                ctx->clear(m_clear_r, m_clear_g, m_clear_b);
            }
            {
                PROFILE_SCOPE("Scene::render");
                m_scenes.render(*this);
            }
            {
                PROFILE_SCOPE("App::on_render");
                app.on_render(*this);
            }
            {
                PROFILE_SCOPE("RHI::end_frame");
                ctx->end_frame();
            }

            if (m_gpu_profiler) m_gpu_profiler->end_frame();
        }

        {
            PROFILE_SCOPE("SwapBuffers");
            m_window.swap_buffers();
        }

        profiler::Profiler::instance().end_frame();
    }

    m_scenes.shutdown_all(*this);
    app.on_shutdown(*this);
}

void Engine::shutdown() {
    m_subsystems.clear();
    if (m_save_system) {
        m_save_system->shutdown();
        m_save_system.reset();
    }
    if (m_audio_engine) {
        m_audio_engine->shutdown();
        m_audio_engine.reset();
    }
    m_assets.shutdown();
    if (m_gpu_profiler) {
        m_gpu_profiler->shutdown();
        m_gpu_profiler.reset();
    }
    rhi::set_current_device(nullptr);  // Clear global before destroying
    m_rhi_device.reset();  // Destroy RHI device before window
    m_window.shutdown();
    platform::Window::shutdown_platform();
    ENGINE_LOG("Engine shut down");
}

} // namespace engine
