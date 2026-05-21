#pragma once

#include "engine/platform/Window.h"
#include "engine/platform/Input.h"
#include "engine/platform/Timer.h"
#include "engine/graphics/RenderContext.h"
#include "engine/core/EventBus.h"
#include "engine/core/EngineConfig.h"
#include "engine/core/SubsystemRegistry.h"
#include "engine/asset/AssetDatabase.h"
#include "engine/audio/AudioEngine.h"
#include "engine/platform/InputAction.h"
#include "engine/save/SaveSystem.h"
#include "engine/scene/SceneManager.h"
#include "engine/core/Logger.h"
#include "engine/rhi/RHIDevice.h"
#include "engine/profiler/GPUProfiler.h"
#include <any>
#include <optional>
#include <stdexcept>
#include <memory>

namespace engine {

class Application;

class Engine {
public:
    Engine() = default;
    explicit Engine(const EngineConfig& cfg) : m_config(cfg) {}
    ~Engine();  // Defined in .cpp where RHIDevice is complete

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool init(const char* title, int width, int height, const char* config_file_path = nullptr);
    void run(Application& app);
    void shutdown();

    platform::Window& window() { return m_window; }
    const platform::Window& window() const { return m_window; }
    platform::Input& input() { return m_input; }
    const platform::Input& input() const { return m_input; }
    platform::Timer& timer() { return m_timer; }
    const platform::Timer& timer() const { return m_timer; }
    graphics::RenderContext& render_context() { return m_render_context; }
    const graphics::RenderContext& render_context() const { return m_render_context; }

    rhi::RHIDevice* rhi_device() { return m_rhi_device.get(); }
    const rhi::RHIDevice* rhi_device() const { return m_rhi_device.get(); }

    rhi::RHIContext* rhi_context();

    profiler::GPUProfiler* gpu_profiler() { return m_gpu_profiler.get(); }
    const profiler::GPUProfiler* gpu_profiler() const { return m_gpu_profiler.get(); }

    EventBus& events() { return m_events; }
    asset::AssetDatabase& assets() { return m_assets; }
    const asset::AssetDatabase& assets() const { return m_assets; }
    scene::SceneManager& scenes() { return m_scenes; }
    const scene::SceneManager& scenes() const { return m_scenes; }

    audio::AudioEngine* audio_engine() { return m_audio_engine.get(); }
    const audio::AudioEngine* audio_engine() const { return m_audio_engine.get(); }

    platform::InputActionMap& action_map() { return m_action_map; }
    const platform::InputActionMap& action_map() const { return m_action_map; }

    save::SaveSystem* save_system() { return m_save_system.get(); }
    const save::SaveSystem* save_system() const { return m_save_system.get(); }

    SubsystemRegistry& subsystems() { return m_subsystems; }
    const SubsystemRegistry& subsystems() const { return m_subsystems; }

    const EngineConfig& config() const { return m_config; }

    template<typename T>
    void set_app_context(T& ctx) { m_app_ctx = &ctx; }

    template<typename T>
    T& app_context() const {
        auto* ptr = std::any_cast<T*>(&m_app_ctx);
        if (!ptr || !*ptr) {
            throw std::runtime_error("Engine::app_context<T>() - context not set or type mismatch");
        }
        return **ptr;
    }

    template<typename T>
    T* try_app_context() const {
        auto* ptr = std::any_cast<T*>(&m_app_ctx);
        return (ptr && *ptr) ? *ptr : nullptr;
    }

    void set_clear_color(float r, float g, float b) {
        m_clear_r = r; m_clear_g = g; m_clear_b = b;
    }

private:
    EngineConfig m_config;
    platform::Window m_window;
    platform::Input m_input;
    platform::Timer m_timer;
    std::unique_ptr<rhi::RHIDevice> m_rhi_device;
    std::unique_ptr<profiler::GPUProfiler> m_gpu_profiler;
    graphics::RenderContext m_render_context;
    EventBus m_events;
    asset::AssetDatabase m_assets;
    scene::SceneManager m_scenes;
    SubsystemRegistry m_subsystems;
    std::unique_ptr<audio::AudioEngine> m_audio_engine;
    platform::InputActionMap m_action_map;
    std::unique_ptr<save::SaveSystem> m_save_system;

    std::any m_app_ctx;

    float m_clear_r = 0.0f;
    float m_clear_g = 0.0f;
    float m_clear_b = 0.0f;
};

}
