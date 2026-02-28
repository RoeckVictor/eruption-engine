#include "GameApplication.h"

#include "engine/core/Engine.h"
#include "engine/core/Logger.h"
#include "engine/core/ScreenRectSystem.h"
#include "engine/scene/Scene.h"
#include "engine/reflection/ReflectionInit.h"
#include "engine/platform/KeyCode.h"
#include "engine/platform/PlatformUtils.h"

#include "editor/core/EditorComponents.h"
#include "editor/core/RuntimeContext.h"
#include "editor/core/SimulationPlayback.h"
#include "editor/scripting/ScriptManager.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/simulation/CategoryLibrary.h"
#include "engine/simulation/PixelGridComponent.h"

#include <imgui.h>
#include <filesystem>

namespace fs = std::filesystem;

class GameScene : public engine::scene::Scene {
public:
    const char* name() const override { return "GameScene"; }

    engine::Result<void, engine::ErrorInfo> on_enter(engine::Engine& /*engine*/) override {
        return engine::Ok();
    }

    void on_exit(engine::Engine& /*engine*/) override {}
};

GameApplication::GameApplication(const std::string& scene_path, const std::string& product_name)
    : m_scene_path(scene_path)
    , m_product_name(product_name)
{
}

GameApplication::~GameApplication() = default;

bool GameApplication::on_init(engine::Engine& engine) {
    auto& log = engine::Logger::instance();
    log.info("Game", "Initializing game: %s", m_product_name.c_str());

    engine::reflection::init_engine_reflections();
    editor::init_component_type_registry();

    // Get executable directory for resolving asset paths
    std::string exe_dir = engine::platform::executable_directory();

    // Load categories first (materials need them to resolve category names)
    std::string categories_path = exe_dir + "/assets/categories";
    m_category_library.ensure_empty_category();
    m_category_library.load_from_directory(categories_path, true);

    // Load default material library (required for pixel simulations)
    auto& mat_registry = engine::simulation::MaterialLibraryRegistry::instance();
    auto* mat_lib = mat_registry.get_or_create_library("default");
    mat_lib->set_category_library(&m_category_library);

    std::string materials_path = exe_dir + "/assets/materials";
    if (!mat_lib->load_from_directory(materials_path)) {
        log.warning("Game", "Failed to load default material library from: %s — pixel simulations may not work", materials_path.c_str());
    }

    editor::SceneSerializer serializer(m_registry);
    bool scene_loaded = false;

    if (!fs::exists(m_scene_path)) {
        log.error("Game", "Scene file not found: %s", m_scene_path.c_str());
        log.error("Game", "Working directory: %s", fs::current_path().string().c_str());
    } else if (!serializer.load(fs::path(m_scene_path))) {
        log.error("Game", "Failed to load scene: %s — %s",
                  m_scene_path.c_str(), serializer.last_error().c_str());
    } else {
        log.info("Game", "Loaded scene: %s", m_scene_path.c_str());
        scene_loaded = true;

        // Reset loaded flag on all PixelGridComponents — scene serializer may have
        // saved loaded=true, but no actual pixel data is in memory yet. This lets
        // PixelGridLoaderSystem pick them up for static grids.
        auto pgc_view = m_registry.view<engine::simulation::PixelGridComponent>();
        for (auto e : pgc_view) {
            pgc_view.get<engine::simulation::PixelGridComponent>(e).loaded = false;
        }
    }

    // Read scene settings (gravity, bg color, etc.)
    const auto& settings = serializer.settings();

    // Set clear color from scene background
    engine.set_clear_color(settings.bg_color[0], settings.bg_color[1], settings.bg_color[2]);

    m_script_manager = std::make_unique<editor::ScriptManager>();

    std::string lib_name = engine::platform::shared_library_name("GameScripts");
    fs::path lib_path = lib_name;

    if (fs::exists(lib_path)) {
        if (m_script_manager->dll_manager().load(lib_path.string())) {
            log.info("Game", "Loaded scripts library (%zu types)",
                     m_script_manager->dll_manager().script_types().size());
        } else {
            log.warning("Game", "Failed to load scripts library: %s",
                        m_script_manager->dll_manager().last_error().c_str());
        }
    } else {
        log.info("Game", "No %s found — running without scripts", lib_name.c_str());
    }

    m_runtime = std::make_unique<editor::RuntimeContext>();
    m_runtime->init(&m_registry, m_script_manager.get());
    m_runtime->set_engine(&engine);

    // Set project assets path (relative to exe for built game)
    m_runtime->set_project_assets_path("Assets");

    // Compute world transforms before play — hierarchy positions must be
    // correct for physics body creation and initial script callbacks.
    if (scene_loaded) {
        editor::update_world_transforms(m_registry);

        // Compute screen-space positions for first frame rendering
        auto& window = engine.window();
        engine::ScreenRectSystem::update(m_registry,
                                         static_cast<float>(window.width()),
                                         static_cast<float>(window.height()));
    }

    if (scene_loaded) {
        // Enter play mode — this initializes physics, scripts, pixel simulations
        m_runtime->play(settings);

        log.info("Game", "Play mode started (physics gravity: %.1f, %.1f)",
                 settings.gravity_x, settings.gravity_y);
    }

    {
        auto view = m_registry.view<engine::render::Camera2D>();
        if (auto it = view.begin(); it != view.end()) {
            m_camera = view.get<engine::render::Camera2D>(*it);
        }
    }

    m_engine_ctx = std::make_unique<engine::EngineContext>(
        engine::EngineContext{m_registry, m_runtime->physics_world(), m_camera}
    );
    engine.set_app_context(*m_engine_ctx);

    auto scene = std::make_unique<GameScene>();

    // Register render systems into the scene
    scene->systems().add_update_system(m_loader_system);
    scene->systems().add_render_system(m_render_system);
    scene->systems().add_render_system(m_image_render_system);
    scene->systems().add_render_system(m_text_render_system);

    // Wire loader into renderer
    m_render_system.set_loader(&m_loader_system);

    engine.scenes().push(std::move(scene));

    if (scene_loaded) {
        log.info("Game", "Game initialized successfully");
    } else {
        log.warning("Game", "Game started without a scene — window will remain open for debugging");
    }
    return true;
}

void GameApplication::on_update(engine::Engine& engine, float dt) {
    if (m_runtime && m_runtime->is_playing()) {
        m_runtime->update(dt);

        for (const auto& sim : m_runtime->sim_surfaces()) {
            auto* tex = sim->color_texture.rhi_texture();
            if (tex) {
                m_render_system.set_texture_override(sim->entity, tex);
            }
        }
    }

    // Update screen-space positions (ScreenRect computed_x/y)
    auto& window = engine.window();
    engine::ScreenRectSystem::update(m_registry,
                                     static_cast<float>(window.width()),
                                     static_cast<float>(window.height()));

    // Sync camera from entity to our stable reference (for rendering)
    {
        auto view = m_registry.view<engine::render::Camera2D>();
        if (auto it = view.begin(); it != view.end()) {
            auto& cam = view.get<engine::render::Camera2D>(*it);
            m_camera.x = cam.x;
            m_camera.y = cam.y;
            m_camera.zoom = cam.zoom;
        }
    }

    // ESC to quit
    if (engine.input().is_pressed(engine::platform::KeyCode::Escape)) {
        engine.window().set_should_close(true);
    }
}

void GameApplication::on_shutdown(engine::Engine& engine) {
    auto& log = engine::Logger::instance();
    log.info("Game", "Shutting down game...");

    if (m_runtime && m_runtime->is_playing()) {
        m_runtime->stop();
    }

    m_engine_ctx.reset();
    m_runtime.reset();
    m_script_manager.reset();

    log.info("Game", "Game shut down cleanly");
}
