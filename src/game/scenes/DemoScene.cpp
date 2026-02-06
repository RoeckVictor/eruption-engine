#include "game/scenes/DemoScene.h"
#include "engine/core/Engine.h"
#include "game/GameEvents.h"
#include "game/GameLog.h"
#include "game/GameComponentRegistry.h"
#include "game/components/Components.h"
#include "game/components/RigidBodyComponent.h"
#include "game/world/MaterialData.h"
#include <nlohmann/json.hpp>
#include <vector>

namespace game {

engine::Result<void, engine::ErrorInfo> DemoScene::on_enter(engine::Engine& engine) {
    using engine::Err;
    using engine::Ok;
    using engine::EngineError;

    // Load game-specific configuration
    m_game_config = GameConfig::load(engine.assets().vfs(), "game/game_config.json");

    // Register singleton context for game input
    m_registry.ctx().emplace<GameInputState>();

    // Set up prefab system
    register_game_components(m_component_registry);
    m_prefab_manager.set_registry(m_component_registry);
    if (!m_prefab_manager.load_prefab(engine.assets().vfs(), "game/player.prefab")) {
        GAME_ERR("DemoScene: Failed to load player.prefab");
    }

    // Init world (fixed-size grid, no streaming)
    const auto& config = engine.config();
    if (!m_world.init(m_game_config.grid_width, m_game_config.grid_height, config.max_material_slots)) {
        return Err<void>(EngineError::InitFailed, "Failed to init world");
    }

    // Init physics
    auto physics_result = m_physics_world.init(
        config.default_gravity_x,
        config.default_gravity_y,
        config.pixels_per_meter
    );
    if (physics_result.is_err()) {
        return Err<void>(EngineError::InitFailed, "Failed to init physics",
                         physics_result.error().message);
    }
    if (!m_body_manager.init(m_physics_world, config.terrain_chunk_size, config.min_body_pixels)) {
        return Err<void>(EngineError::InitFailed, "Failed to init body manager");
    }

    // Init particle systems
    if (!m_particle_buffer.init(65536)) {
        return Err<void>(EngineError::InitFailed, "Failed to init particle buffer");
    }
    if (!m_particle_sim.init(m_game_config.grid_width, m_game_config.grid_height)) {
        return Err<void>(EngineError::InitFailed, "Failed to init particle sim");
    }
    if (!m_particle_renderer.init()) {
        return Err<void>(EngineError::InitFailed, "Failed to init particle renderer");
    }

    // Build game context and publish on the engine for system dependency resolution
    m_context.emplace(GameContext{
        m_registry, m_world, m_camera,
        m_physics_world, m_body_manager,
        m_particle_buffer, m_particle_sim, m_particle_renderer
    });
    engine.set_app_context(*m_context);

    // Fill the bottom rows with stone to form a flat floor
    fill_floor();

    // Spawn player centered on top of the floor
    float spawn_x = (float)(m_game_config.grid_width / 2);
    float spawn_y = (float)(m_game_config.grid_height - m_game_config.floor_thickness
                            - m_game_config.player_spawn_offset_y);
    spawn_player(spawn_x, spawn_y);
    GAME_LOG("Player spawned at (%.0f, %.0f)", spawn_x, spawn_y);

    // Init camera on player position
    m_camera.x = spawn_x;
    m_camera.y = spawn_y;
    m_camera.zoom = m_game_config.camera_initial_zoom;
    m_camera.smoothing = m_game_config.camera_smoothing;

    // Register systems into the scene's SystemManager.
    // Execution order within each phase matches registration order.
    auto& sys = systems();

    // Update phase (per-frame)
    sys.add_update_system(m_game_input);
    sys.add_update_system(m_tool_system);
    sys.add_update_system(m_input_system);
    sys.add_update_system(m_camera_system);

    // Fixed-update phase (fixed timestep)
    sys.add_fixed_update_system(m_player_system);
    sys.add_fixed_update_system(m_sim_system);
    sys.add_fixed_update_system(m_rigidbody_sync_system);  // Sync Transform after physics

    // Render phase
    sys.add_render_system(m_render_system);
    sys.add_render_system(m_particle_render_system);
    sys.add_render_system(m_entity_render_system);
    sys.add_render_system(m_debug_render_system);

    // Subscribe to respawn event
    engine.events().subscribe<RespawnRequestedEvent>(
        [this](const RespawnRequestedEvent&) {
            respawn_player();
        });

    engine.set_clear_color(0.05f, 0.05f, 0.08f);

    GAME_LOG("--- DemoScene Controls ---");
    GAME_LOG("A/D or Left/Right: Move player");
    GAME_LOG("W/Up/Space:        Jump");
    GAME_LOG("Left click:        Spawn material");
    GAME_LOG("Right click:       Erase / dig");
    GAME_LOG("1-7:               Select material (1=Rock 2=Dirt 3=Sand 4=Water 5=Lava 6=Ice 7=Steam)");
    GAME_LOG("[/]:               Brush size");
    GAME_LOG("8:                 Paste sprite (object.pxg)");
    GAME_LOG("9:                 Spawn rigidbody (box.pxg)");
    GAME_LOG("Scroll wheel:      Zoom in/out");
    GAME_LOG("F3:                Toggle debug draw");
    GAME_LOG("P:                 Pause/resume simulation");
    GAME_LOG("R:                 Respawn player");
    GAME_LOG("Escape:            Quit");

    return Ok();
}

void DemoScene::on_exit(engine::Engine& /*engine*/) {
    m_particle_renderer.shutdown();
    m_particle_sim.shutdown();
    m_particle_buffer.shutdown();
    m_body_manager.shutdown();
    m_physics_world.shutdown();
    m_world.shutdown();
}

void DemoScene::spawn_player(float x, float y) {
    // Destroy old player entity if it exists (including its rigid body)
    if (m_player_entity != entt::null) {
        if (auto* rb = m_registry.try_get<RigidBodyComponent>(m_player_entity)) {
            if (rb->body) {
                m_body_manager.destroy_body(rb->body);
            }
        }
        m_registry.destroy(m_player_entity);
    }

    // Create player pixel buffer (filled with rock material)
    int player_w = m_game_config.player_body_width;
    int player_h = m_game_config.player_body_height;
    std::vector<uint8_t> materials(player_w * player_h, MAT_ROCK);
    std::vector<uint8_t> categories(player_w * player_h, CAT_STATIC);

    // Create PixelBody for player (indestructible, so tools can't damage it)
    auto* player_body = m_body_manager.create_body(
        materials.data(), categories.data(),
        player_w, player_h,
        x - player_w / 2.0f, y - player_h / 2.0f,  // Center on spawn position
        true,  // is_dynamic
        true   // indestructible
    );

    if (!player_body) {
        GAME_ERR("Failed to create player PixelBody!");
        return;
    }

    // Lock rotation so player stays upright
    m_physics_world.set_fixed_rotation(player_body->body_id(), true);

    // Instantiate player entity from prefab with position override
    nlohmann::json overrides;
    overrides["Transform"] = { {"x", x}, {"y", y} };
    m_player_entity = m_prefab_manager.instantiate("player", m_registry, overrides);

    if (m_player_entity == entt::null) {
        GAME_ERR("Failed to instantiate player from prefab!");
        m_body_manager.destroy_body(player_body);
        return;
    }

    // Add the RigidBodyComponent manually (physics bodies can't be serialized to prefab)
    m_registry.emplace<RigidBodyComponent>(m_player_entity).body = player_body;
}

void DemoScene::respawn_player() {
    float spawn_x = (float)(m_game_config.grid_width / 2);
    float spawn_y = (float)(m_game_config.grid_height - m_game_config.floor_thickness
                            - m_game_config.player_spawn_offset_y);
    spawn_player(spawn_x, spawn_y);
    GAME_LOG("Player respawned at (%.0f, %.0f)", spawn_x, spawn_y);
}

void DemoScene::fill_floor() {
    int floor_y = m_game_config.grid_height - m_game_config.floor_thickness;
    const auto& rock_props = MATERIAL_TABLE[MAT_ROCK];

    int floor_pixel_count = m_game_config.grid_width * m_game_config.floor_thickness;
    std::vector<uint8_t> pixels(floor_pixel_count * 4);
    for (int i = 0; i < floor_pixel_count; i++) {
        pixels[i * 4 + 0] = MAT_ROCK;              // R = material ID
        pixels[i * 4 + 1] = rock_props.category;  // G = category (engine physics)
        pixels[i * 4 + 2] = rock_props.default_temp; // B = temperature
        pixels[i * 4 + 3] = 0;                    // A = reserved
    }

    m_world.grid().upload_both(0, floor_y, m_game_config.grid_width,
                               m_game_config.floor_thickness, pixels.data());
}

} // namespace game
