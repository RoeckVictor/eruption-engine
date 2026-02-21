#include "SimulationPlayback.h"
#include "EditorComponents.h"
#include "engine/core/Logger.h"
#include "engine/core/Transform.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/simulation/SimSurface.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include <glad/gl.h>

namespace editor {

SimulationPlayback::SimulationPlayback(entt::registry& registry)
    : m_registry(registry) {}

SimulationPlayback::~SimulationPlayback() {
    shutdown();
}

void SimulationPlayback::init(engine::physics::PhysicsWorld* physics_world) {
    m_physics_world = physics_world;

    if (!m_color_shader.load_compute("shaders/ssbo_to_color.comp")) {
        engine::Logger::instance().error("Runtime",
            "Failed to load color conversion shader 'shaders/ssbo_to_color.comp' - "
            "pixel simulations will not render correctly");
        return;
    }

    auto view = m_registry.view<engine::simulation::SimSurface,
                                 engine::simulation::PixelGridComponent>();
    for (auto entity : view) {
        auto& sim_surface = view.get<engine::simulation::SimSurface>(entity);
        auto& grid_comp = view.get<engine::simulation::PixelGridComponent>(entity);

        if (!sim_surface.simulation_enabled) continue;
        if (grid_comp.pixel_grid_path.empty()) continue;

        auto pxg_file = engine::asset::pxg_load(grid_comp.pixel_grid_path);
        if (!pxg_file) {
            engine::Logger::instance().warning("Runtime", "Failed to load .pxg file: %s",
                grid_comp.pixel_grid_path.c_str());
            continue;
        }

        auto parsed = engine::asset::parse_pxg(*pxg_file);
        if (parsed.width <= 0 || parsed.height <= 0) {
            engine::Logger::instance().warning("Runtime", "Invalid .pxg dimensions: %dx%d",
                parsed.width, parsed.height);
            continue;
        }

        auto* lib = engine::simulation::MaterialLibraryRegistry::instance().get_library(sim_surface.material_set);
        if (!lib) {
            engine::Logger::instance().warning("Runtime", "Material library '%s' not found",
                sim_surface.material_set.c_str());
            continue;
        }

        auto state = std::make_unique<SimSurfaceState>();
        state->entity = entity;
        state->width = parsed.width;
        state->height = parsed.height;

        if (!state->pixel_grid.init(parsed.width, parsed.height, 4)) {
            engine::Logger::instance().error("Runtime", "Failed to init PixelGrid %dx%d",
                parsed.width, parsed.height);
            continue;
        }

        int pixel_count = parsed.width * parsed.height;
        std::vector<uint8_t> pixel_data(pixel_count * 4, 0);

        for (int i = 0; i < pixel_count; i++) {
            uint8_t mat_id = 0;
            if (!parsed.material_ids.empty() && i < static_cast<int>(parsed.material_ids.size())) {
                mat_id = parsed.material_ids[i];
            }

            uint8_t category = 0;
            uint8_t default_temp = 128;
            if (mat_id > 0) {
                auto* mat_def = lib->get_material(mat_id);
                if (mat_def) {
                    category = static_cast<uint8_t>(mat_def->category);
                    default_temp = mat_def->default_temp;
                }
            }

            pixel_data[i * 4 + 0] = mat_id;
            pixel_data[i * 4 + 1] = category;
            pixel_data[i * 4 + 2] = default_temp;
            pixel_data[i * 4 + 3] = 0;
        }

        int mat_counts[256] = {};
        for (int i = 0; i < pixel_count; i++) {
            mat_counts[pixel_data[i * 4 + 0]]++;
        }
        int non_air = pixel_count - mat_counts[0];
        int mobile = 0;
        for (int i = 0; i < pixel_count; i++) {
            uint8_t cat = pixel_data[i * 4 + 1];
            if (cat >= 2) mobile++; // powder=2, liquid=3, gas=4
        }
        engine::Logger::instance().info("Runtime",
            "Pixel data: %d total, %d non-air, %d mobile (powder/liquid/gas)",
            pixel_count, non_air, mobile);
        if (!parsed.has_material_layer) {
            engine::Logger::instance().warning("Runtime", ".pxg has NO material layer - all pixels are air");
        }

        state->pixel_grid.upload_both(0, 0, parsed.width, parsed.height, pixel_data.data());

        auto slots = lib->build_material_slots();

        // Resolve special material IDs for sim_step.comp uniforms
        uint8_t mat_water = 0, mat_lava = 0, mat_ice = 0, mat_steam = 0;
        if (auto* m = lib->get_material("water")) mat_water = m->id;
        if (auto* m = lib->get_material("lava"))  mat_lava  = m->id;
        if (auto* m = lib->get_material("ice"))   mat_ice   = m->id;
        if (auto* m = lib->get_material("steam")) mat_steam = m->id;

        // Init Margolus simulation with material uniform callback
        auto uniform_callback = [mat_water, mat_lava, mat_ice, mat_steam](engine::graphics::Shader& shader) {
            shader.set_uint("u_mat_water", mat_water);
            shader.set_uint("u_mat_lava", mat_lava);
            shader.set_uint("u_mat_ice", mat_ice);
            shader.set_uint("u_mat_steam", mat_steam);
        };

        // Pass chunk size to simulation for GPU dirty tracking
        int chunk_size_x = sim_surface.chunk_size_x;
        int chunk_size_y = sim_surface.chunk_size_y;

        if (!state->simulation.init(slots.data(), static_cast<int>(slots.size()),
                                     parsed.width, parsed.height,
                                     "shaders/sim_step.comp", uniform_callback,
                                     256, chunk_size_x, chunk_size_y)) {
            engine::Logger::instance().error("Runtime", "Failed to init MargolusSimulation");
            state->pixel_grid.shutdown();
            continue;
        }

        // Create RGBA8 color texture for viewport display
        if (!state->color_texture.create_2d(parsed.width, parsed.height,
                                             engine::graphics::TextureFormat::RGBA8)) {
            engine::Logger::instance().error("Runtime", "Failed to create color texture");
            state->simulation.shutdown();
            state->pixel_grid.shutdown();
            continue;
        }

        // Build palette SSBO from material library colors
        auto palette = lib->build_color_palette();
        palette.resize(256, 0x00000000);
        state->palette_ssbo.create(palette.size() * sizeof(uint32_t), palette.data(),
                                    engine::graphics::BufferUsage::StaticDraw);

        // Initialize terrain collider generation if requested
        if (sim_surface.generate_colliders && m_physics_world) {
            state->terrain_colliders = std::make_unique<engine::physics::TerrainColliderManager>();
            if (state->terrain_colliders->init(*m_physics_world,
                    sim_surface.chunk_size_x, sim_surface.chunk_size_y)) {
                engine::Logger::instance().info("Runtime",
                    "Terrain colliders enabled: chunk=%dx%d",
                    sim_surface.chunk_size_x, sim_surface.chunk_size_y);
            } else {
                engine::Logger::instance().error("Runtime", "Failed to init TerrainColliderManager");
                state->terrain_colliders.reset();
            }
        }

        engine::Logger::instance().info("Runtime", "Initialized pixel simulation: %dx%d, %zu materials",
            parsed.width, parsed.height, slots.size());

        m_surfaces.push_back(std::move(state));
    }

    if (!m_surfaces.empty()) {
        engine::Logger::instance().info("Runtime", "Started %zu pixel simulation(s)",
            m_surfaces.size());
    }
}

void SimulationPlayback::update(uint64_t frame_count) {
    if (m_surfaces.empty()) return;

    // Log diagnostic info on first simulation tick per play session
    bool first_update = (frame_count <= 1);

    for (auto& state : m_surfaces) {
        if (!m_registry.valid(state->entity)) {
            if (first_update) engine::Logger::instance().warning("Runtime", "Sim entity invalid");
            continue;
        }
        if (!m_registry.all_of<engine::simulation::SimSurface>(state->entity)) {
            if (first_update) engine::Logger::instance().warning("Runtime", "Entity missing SimSurface");
            continue;
        }

        auto& sim_surface = m_registry.get<engine::simulation::SimSurface>(state->entity);
        if (!sim_surface.simulation_enabled) {
            if (first_update) engine::Logger::instance().warning("Runtime", "Simulation disabled");
            continue;
        }

        if (first_update) {
            engine::Logger::instance().info("Runtime",
                "Running simulation tick: entity=%u, grid=%dx%d, color_tex=%u",
                static_cast<unsigned>(state->entity), state->width, state->height,
                state->color_texture.handle());
        }

        state->simulation.simulate(state->pixel_grid, m_render_context);

        // Convert SSBO -> RGBA8 color texture via palette lookup compute shader
        m_color_shader.use();
        state->pixel_grid.bind_read_ssbo(0);
        state->palette_ssbo.bind_base(1);
        state->color_texture.bind_as_image(0, engine::graphics::ImageAccess::WriteOnly);
        m_color_shader.set_int("u_grid_width", state->width);
        m_color_shader.set_int("u_grid_height", state->height);
        m_color_shader.set_uint("u_pixel_size", static_cast<uint32_t>(state->pixel_grid.pixel_size()));
        m_color_shader.set_uint("u_palette_size", 256u);

        int groups_x = (state->width + 15) / 16;
        int groups_y = (state->height + 15) / 16;
        // TEXTURE_FETCH barrier: ImGui reads this via texture sampler, not imageLoad
        m_render_context.dispatch_compute(groups_x, groups_y, 1, GL_TEXTURE_FETCH_BARRIER_BIT);

        // Update terrain colliders from settled pixels
        if (state->terrain_colliders && m_registry.valid(state->entity)) {
            engine::physics::TerrainColliderManager::EntityTransform et;
            if (m_registry.all_of<engine::Transform>(state->entity)) {
                auto& t = m_registry.get<engine::Transform>(state->entity);
                et.world_x = t.world_x;
                et.world_y = t.world_y;
                et.world_rotation_deg = t.world_rotation;
                et.scale_x = t.world_scale_x;
                et.scale_y = t.world_scale_y;
            }
            if (m_registry.all_of<engine::simulation::PixelGridComponent>(state->entity)) {
                auto& gc = m_registry.get<engine::simulation::PixelGridComponent>(state->entity);
                et.origin_x = gc.origin_x;
                et.origin_y = gc.origin_y;
            }

            // Use GPU dirty flags instead of marking entire grid dirty.
            // This only updates chunks where pixels actually moved, dramatically
            // reducing CPU readback and Box2D collider regeneration overhead.
            auto dirty_flags = state->simulation.read_and_clear_dirty_chunks();
            state->terrain_colliders->apply_gpu_dirty_flags(
                dirty_flags,
                state->simulation.num_chunks_x(),
                state->simulation.num_chunks_y());
            state->terrain_colliders->update_terrain_colliders(state->pixel_grid, et);
        }
    }
}

void SimulationPlayback::shutdown() {
    for (auto& state : m_surfaces) {
        if (state->terrain_colliders) {
            state->terrain_colliders->shutdown();
            state->terrain_colliders.reset();
        }
        state->simulation.shutdown();
        state->pixel_grid.shutdown();
        state->color_texture.destroy();
        state->palette_ssbo.destroy();
    }
    m_surfaces.clear();
    m_color_shader.destroy();
}

uint32_t SimulationPlayback::get_sim_texture(entt::entity entity) const {
    for (const auto& state : m_surfaces) {
        if (state->entity == entity) {
            return state->color_texture.handle();
        }
    }
    return 0;
}

}