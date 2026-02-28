#pragma once

#include "engine/core/Application.h"
#include "engine/core/EngineContext.h"
#include "engine/systems/PixelGridRenderSystem.h"
#include "engine/systems/PixelGridLoaderSystem.h"
#include "engine/systems/ImageRenderSystem.h"
#include "engine/systems/TextRenderSystem.h"
#include "engine/simulation/CategoryLibrary.h"
#include "engine/render/Camera2D.h"

#include <entt/entt.hpp>
#include <memory>
#include <string>

namespace editor {
class RuntimeContext;
class ScriptManager;
}

class GameApplication : public engine::Application {
public:
    GameApplication(const std::string& scene_path, const std::string& product_name);
    ~GameApplication();

    bool on_init(engine::Engine& engine) override;
    void on_shutdown(engine::Engine& engine) override;
    void on_update(engine::Engine& engine, float dt) override;

private:
    std::string m_scene_path;
    std::string m_product_name;

    entt::registry m_registry;
    std::unique_ptr<editor::RuntimeContext> m_runtime;
    std::unique_ptr<editor::ScriptManager> m_script_manager;

    engine::PixelGridLoaderSystem m_loader_system;
    engine::PixelGridRenderSystem m_render_system;
    engine::ImageRenderSystem m_image_render_system;
    engine::TextRenderSystem m_text_render_system;

    engine::render::Camera2D m_camera;

    engine::simulation::CategoryLibrary m_category_library;

    std::unique_ptr<engine::EngineContext> m_engine_ctx;
};
