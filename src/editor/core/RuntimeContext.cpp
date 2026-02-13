#include "RuntimeContext.h"
#include "EditorComponents.h"
#include "editor/serialization/SceneSerializer.h"
#include "engine/core/MathConstants.h"
#include "engine/core/Logger.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/animation/AnimationSystem.h"
#include "engine/simulation/SimSurface.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/MaterialLibrary.h"
#include "engine/asset/PixelGridFile.h"
#include "engine/asset/PxgDataParser.h"
#include <glad/gl.h>
#include <cmath>

namespace editor {

RuntimeContext::RuntimeContext() = default;
RuntimeContext::~RuntimeContext() = default;

void RuntimeContext::init(entt::registry* editor_registry) {
    m_editor_registry = editor_registry;
}

void RuntimeContext::play(const SceneSettings& settings) {
    if (m_state != PlayState::Editing) {
        return; // Already playing
    }

    if (!m_editor_registry) {
        engine::Logger::instance().error("Runtime", "Cannot enter play mode: no registry set");
        return;
    }

    // Snapshot the current scene state (for restoring on stop)
    snapshot_scene();

    // Initialize physics world from scene settings.
    // SceneSettings gravity is in pixels/s² with positive Y = down convention.
    // Editor uses Y-up, so negate Y. PhysicsWorld expects m/s², so divide by ppm.
    float ppm = settings.pixels_per_meter;
    float gravity_x_m = settings.gravity_x / ppm;
    float gravity_y_m = -settings.gravity_y / ppm;  // negate: Y-down scene setting → Y-up Box2D
    m_physics_world = std::make_unique<engine::physics::PhysicsWorld>();
    m_physics_world->init(gravity_x_m, gravity_y_m, ppm);

    // Create Box2D bodies from Rigidbody + Collider components
    init_physics_bodies();

    // Initialize pixel simulations for entities with SimSurface + PixelGridComponent
    init_pixel_simulations();

    m_state = PlayState::Playing;
    m_play_time = 0.0f;
    m_frame_count = 0;

    engine::Logger::instance().info("Runtime", "Entered play mode with physics system");
}

void RuntimeContext::pause() {
    if (m_state != PlayState::Playing) {
        return;
    }

    m_state = PlayState::Paused;
    engine::Logger::instance().info("Runtime", "Paused (frame %llu, time %.2fs)", m_frame_count, m_play_time);
}

void RuntimeContext::resume() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_state = PlayState::Playing;
    engine::Logger::instance().info("Runtime", "Resumed");
}

void RuntimeContext::stop() {
    if (m_state == PlayState::Editing) {
        return; // Not playing
    }

    // Shutdown engine systems before restoring scene
    shutdown_pixel_simulations();
    m_physics_world.reset();

    // Restore the original scene state
    restore_scene();

    m_state = PlayState::Editing;
    engine::Logger::instance().info("Runtime", "Stopped play mode (ran for %.2fs, %llu frames)", m_play_time, m_frame_count);
}

void RuntimeContext::step_frame() {
    if (m_state != PlayState::Paused) {
        return;
    }

    m_step_requested = true;
}

void RuntimeContext::update(float dt) {
    if (m_state == PlayState::Editing) {
        return;
    }

    // If paused and no step requested, don't update
    if (m_state == PlayState::Paused && !m_step_requested) {
        return;
    }

    // Clear step request
    m_step_requested = false;

    // Update play time and frame count
    m_play_time += dt;
    m_frame_count++;

    // Run engine systems on the editor registry
    if (!m_editor_registry) return;

    // Update physics (Box2D simulation)
    if (m_physics_world) {
        m_physics_world->step(dt, 4); // 4 substeps for stability

        // Sync physics results back to transforms
        sync_physics_to_transforms();
    }

    // Update world transforms for hierarchy
    update_world_transforms(*m_editor_registry);

    // Update pixel simulations (falling sand, liquids, gases)
    update_pixel_simulations(dt);
}

void RuntimeContext::snapshot_scene() {
    if (!m_editor_registry) {
        return;
    }

    // Serialize the scene to a string (JSON)
    SceneSerializer serializer(*m_editor_registry);
    m_scene_snapshot = serializer.save_to_string();

    engine::Logger::instance().info("Runtime", "Scene snapshot created (%zu bytes)", m_scene_snapshot.size());
}

void RuntimeContext::restore_scene() {
    if (!m_editor_registry || m_scene_snapshot.empty()) {
        return;
    }

    // Clear the editor registry
    m_editor_registry->clear();

    // Restore from snapshot
    SceneSerializer serializer(*m_editor_registry);
    if (serializer.load_from_string(m_scene_snapshot)) {
        engine::Logger::instance().info("Runtime", "Scene restored from snapshot");
    } else {
        engine::Logger::instance().error("Runtime", "Failed to restore scene from snapshot");
    }

    m_scene_snapshot.clear();
}

void RuntimeContext::init_physics_bodies() {
    if (!m_editor_registry || !m_physics_world) return;

    int body_count = 0;

    // 1) Create bodies for entities with explicit Rigidbody components
    auto rb_view = m_editor_registry->view<engine::physics::Rigidbody, engine::Transform>();
    for (auto entity : rb_view) {
        auto& rb = rb_view.get<engine::physics::Rigidbody>(entity);
        if (!rb.enabled) continue;
        if (b2Body_IsValid(rb.body_id)) continue;

        create_body_for_entity(entity);
        body_count++;
    }

    // 2) Create implicit static bodies for collider-only entities (no Rigidbody).
    //    Box2D requires every shape to be attached to a body.
    int static_count = 0;
    auto create_static_for_collider = [&](entt::entity entity) {
        if (!m_editor_registry->all_of<engine::Transform>(entity)) return;
        if (m_editor_registry->all_of<engine::physics::Rigidbody>(entity)) return; // already handled above

        // Add an implicit Rigidbody as static
        auto& rb = m_editor_registry->emplace<engine::physics::Rigidbody>(entity);
        rb.body_type = engine::physics::BodyType::Static;
        rb.mass = 0.0f;
        rb.enabled = true;

        auto& transform = m_editor_registry->get<engine::Transform>(entity);
        float angle_rad = transform.world_rotation * engine::DEG_TO_RAD;
        rb.body_id = m_physics_world->create_static_body(
            transform.world_x, transform.world_y, angle_rad);

        // Attach collider shapes
        attach_collider_shapes(entity, rb);
        static_count++;
    };

    m_editor_registry->view<engine::physics::BoxCollider>().each(
        [&](entt::entity e, auto&) { create_static_for_collider(e); });
    m_editor_registry->view<engine::physics::CircleCollider>().each(
        [&](entt::entity e, auto&) { create_static_for_collider(e); });
    m_editor_registry->view<engine::physics::CapsuleCollider>().each(
        [&](entt::entity e, auto&) { create_static_for_collider(e); });

    engine::Logger::instance().info("Runtime", "Created %d physics bodies (%d implicit static)", body_count + static_count, static_count);
}

void RuntimeContext::create_body_for_entity(entt::entity entity) {
    if (!m_editor_registry || !m_physics_world) return;
    if (!m_editor_registry->all_of<engine::physics::Rigidbody, engine::Transform>(entity)) return;

    auto& rb = m_editor_registry->get<engine::physics::Rigidbody>(entity);
    auto& transform = m_editor_registry->get<engine::Transform>(entity);

    // Create body based on type using world position
    float angle_rad = transform.world_rotation * engine::DEG_TO_RAD;

    switch (rb.body_type) {
        case engine::physics::BodyType::Dynamic:
            rb.body_id = m_physics_world->create_dynamic_body(
                transform.world_x, transform.world_y, angle_rad);
            break;
        case engine::physics::BodyType::Static:
            rb.body_id = m_physics_world->create_static_body(
                transform.world_x, transform.world_y, angle_rad);
            break;
        case engine::physics::BodyType::Kinematic:
            rb.body_id = m_physics_world->create_kinematic_body(
                transform.world_x, transform.world_y, angle_rad);
            break;
    }

    // Apply body properties
    m_physics_world->set_gravity_scale(rb.body_id, rb.gravity_scale);
    m_physics_world->set_fixed_rotation(rb.body_id, rb.lock_rotation);

    // Apply initial velocity for dynamic bodies
    if (rb.body_type == engine::physics::BodyType::Dynamic) {
        if (rb.initial_velocity_x != 0.0f || rb.initial_velocity_y != 0.0f) {
            m_physics_world->set_body_linear_velocity(rb.body_id,
                rb.initial_velocity_x, rb.initial_velocity_y);
        }
        if (rb.initial_angular_velocity != 0.0f) {
            m_physics_world->set_body_angular_velocity(rb.body_id,
                rb.initial_angular_velocity);
        }
    }

    // Attach collider shapes to the body
    attach_collider_shapes(entity, rb);

    // For dynamic bodies: ensure mass is set even without collider shapes.
    // Box2D bodies with no shapes have zero mass and won't respond to gravity.
    // Use Rigidbody.mass as override (like Unity behavior).
    if (rb.body_type == engine::physics::BodyType::Dynamic && rb.mass > 0.0f) {
        b2MassData mass_data = b2Body_GetMassData(rb.body_id);
        if (mass_data.mass <= 0.0f) {
            mass_data.mass = rb.mass;
            mass_data.center = {0.0f, 0.0f};
            mass_data.rotationalInertia = rb.mass * 0.01f;
            b2Body_SetMassData(rb.body_id, mass_data);
        }
    }
}

void RuntimeContext::attach_collider_shapes(entt::entity entity, engine::physics::Rigidbody& rb) {
    if (!m_editor_registry || !m_physics_world) return;
    if (!m_editor_registry->all_of<engine::Transform>(entity)) return;

    auto& transform = m_editor_registry->get<engine::Transform>(entity);
    float scale_x = transform.world_scale_x;
    float scale_y = transform.world_scale_y;
    float avg_scale = (std::abs(scale_x) + std::abs(scale_y)) * 0.5f;

    // BoxCollider
    if (m_editor_registry->all_of<engine::physics::BoxCollider>(entity)) {
        auto& box = m_editor_registry->get<engine::physics::BoxCollider>(entity);
        if (box.enabled) {
            // Apply entity scale to dimensions and offset
            float hw = m_physics_world->pixels_to_meters(box.width * 0.5f * std::abs(scale_x));
            float hh = m_physics_world->pixels_to_meters(box.height * 0.5f * std::abs(scale_y));
            float ox = m_physics_world->pixels_to_meters(box.offset_x * scale_x);
            float oy = m_physics_world->pixels_to_meters(box.offset_y * scale_y);

            float rot_rad = box.rotation * engine::DEG_TO_RAD;
            float cos_r = std::cos(rot_rad);
            float sin_r = std::sin(rot_rad);

            float corners[4][2] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };
            b2Vec2 verts[4];
            for (int i = 0; i < 4; i++) {
                verts[i].x = corners[i][0] * cos_r - corners[i][1] * sin_r + ox;
                verts[i].y = corners[i][0] * sin_r + corners[i][1] * cos_r + oy;
            }

            box.shape_id = m_physics_world->add_polygon_shape(
                rb.body_id, verts, 4, box.density, box.friction, box.restitution);
        }
    }

    // CircleCollider
    if (m_editor_registry->all_of<engine::physics::CircleCollider>(entity)) {
        auto& circle = m_editor_registry->get<engine::physics::CircleCollider>(entity);
        if (circle.enabled) {
            b2Circle c;
            c.center.x = m_physics_world->pixels_to_meters(circle.offset_x * scale_x);
            c.center.y = m_physics_world->pixels_to_meters(circle.offset_y * scale_y);
            c.radius = m_physics_world->pixels_to_meters(circle.radius * avg_scale);

            b2ShapeDef shape_def = b2DefaultShapeDef();
            shape_def.density = circle.density;
            shape_def.material.friction = circle.friction;
            shape_def.material.restitution = circle.restitution;
            shape_def.isSensor = circle.is_trigger;

            circle.shape_id = b2CreateCircleShape(rb.body_id, &shape_def, &c);
        }
    }

    // CapsuleCollider
    if (m_editor_registry->all_of<engine::physics::CapsuleCollider>(entity)) {
        auto& cap = m_editor_registry->get<engine::physics::CapsuleCollider>(entity);
        if (cap.enabled) {
            // Apply entity scale to dimensions and offset
            float half_len = m_physics_world->pixels_to_meters(cap.length * 0.5f * avg_scale);
            float rad = m_physics_world->pixels_to_meters(cap.radius * avg_scale);
            float ox = m_physics_world->pixels_to_meters(cap.offset_x * scale_x);
            float oy = m_physics_world->pixels_to_meters(cap.offset_y * scale_y);
            float cap_rot = cap.rotation * engine::DEG_TO_RAD;

            float ax = -std::sin(cap_rot);
            float ay = std::cos(cap_rot);

            float bw = rad;
            float bh = half_len;
            float cos_c = std::cos(cap_rot);
            float sin_c = std::sin(cap_rot);

            float box_corners[4][2] = {
                {-bw, -bh}, {bw, -bh}, {bw, bh}, {-bw, bh}
            };
            b2Vec2 box_verts[4];
            for (int i = 0; i < 4; i++) {
                box_verts[i].x = box_corners[i][0] * cos_c - box_corners[i][1] * sin_c + ox;
                box_verts[i].y = box_corners[i][0] * sin_c + box_corners[i][1] * cos_c + oy;
            }
            cap.shape_ids.push_back(m_physics_world->add_polygon_shape(
                rb.body_id, box_verts, 4, cap.density, cap.friction, cap.restitution));

            b2ShapeDef shape_def = b2DefaultShapeDef();
            shape_def.density = cap.density;
            shape_def.material.friction = cap.friction;
            shape_def.material.restitution = cap.restitution;
            shape_def.isSensor = cap.is_trigger;

            b2Circle top_circle;
            top_circle.center.x = ox + ax * half_len;
            top_circle.center.y = oy + ay * half_len;
            top_circle.radius = rad;
            cap.shape_ids.push_back(b2CreateCircleShape(rb.body_id, &shape_def, &top_circle));

            b2Circle bottom_circle;
            bottom_circle.center.x = ox - ax * half_len;
            bottom_circle.center.y = oy - ay * half_len;
            bottom_circle.radius = rad;
            cap.shape_ids.push_back(b2CreateCircleShape(rb.body_id, &shape_def, &bottom_circle));
        }
    }
}

void RuntimeContext::sync_physics_to_transforms() {
    if (!m_editor_registry || !m_physics_world) return;

    auto view = m_editor_registry->view<engine::physics::Rigidbody, engine::Transform>();

    for (auto entity : view) {
        auto& rb = view.get<engine::physics::Rigidbody>(entity);
        auto& transform = view.get<engine::Transform>(entity);

        if (!rb.enabled) continue;

        // Auto-create body for dynamically spawned entities
        if (!b2Body_IsValid(rb.body_id)) {
            create_body_for_entity(entity);
            if (!b2Body_IsValid(rb.body_id)) continue; // Creation failed
        }

        if (rb.body_type == engine::physics::BodyType::Dynamic) {
            // Read physics position (in pixels) and angle (in radians)
            b2Vec2 pos = m_physics_world->get_body_position(rb.body_id);
            float angle = m_physics_world->get_body_angle(rb.body_id);

            // Write to local transform (works correctly for root entities)
            transform.x = pos.x;
            transform.y = pos.y;
            transform.rotation = angle * engine::RAD_TO_DEG;

            // Handle position locks by zeroing velocity on locked axes
            if (rb.lock_position_x || rb.lock_position_y) {
                b2Vec2 vel = m_physics_world->get_body_linear_velocity(rb.body_id);
                if (rb.lock_position_x) vel.x = 0.0f;
                if (rb.lock_position_y) vel.y = 0.0f;
                m_physics_world->set_body_linear_velocity(rb.body_id, vel.x, vel.y);
            }
        }
        else if (rb.body_type == engine::physics::BodyType::Kinematic) {
            // Sync transform -> physics for kinematic bodies
            m_physics_world->set_body_transform(rb.body_id,
                transform.world_x, transform.world_y,
                transform.world_rotation * engine::DEG_TO_RAD);
        }
    }
}

void RuntimeContext::init_pixel_simulations() {
    if (!m_editor_registry) return;

    // Load shared color conversion shader (SSBO material IDs → RGBA8 via palette)
    if (!m_color_shader.load_compute("shaders/ssbo_to_color.comp")) {
        engine::Logger::instance().error("Runtime", "Failed to load ssbo_to_color compute shader");
        return;
    }

    auto view = m_editor_registry->view<engine::simulation::SimSurface,
                                         engine::simulation::PixelGridComponent>();
    for (auto entity : view) {
        auto& sim_surface = view.get<engine::simulation::SimSurface>(entity);
        auto& grid_comp = view.get<engine::simulation::PixelGridComponent>(entity);

        if (!sim_surface.simulation_enabled) continue;
        if (grid_comp.pixel_grid_path.empty()) continue;

        // Load .pxg file
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

        // Get material library
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

        // Initialize PixelGrid (4 bytes per pixel: material, category, temperature, flags)
        if (!state->pixel_grid.init(parsed.width, parsed.height, 4)) {
            engine::Logger::instance().error("Runtime", "Failed to init PixelGrid %dx%d",
                parsed.width, parsed.height);
            continue;
        }

        // Build initial pixel data from parsed material IDs
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
            pixel_data[i * 4 + 3] = 0; // flags
        }

        // Diagnostic: count material distribution
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

        // Build material slots for simulation
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

        if (!state->simulation.init(slots.data(), static_cast<int>(slots.size()),
                                     parsed.width, parsed.height,
                                     "shaders/sim_step.comp", uniform_callback)) {
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
        // Ensure 256 entries
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

        m_sim_surfaces.push_back(std::move(state));
    }

    if (!m_sim_surfaces.empty()) {
        engine::Logger::instance().info("Runtime", "Started %zu pixel simulation(s)",
            m_sim_surfaces.size());
    }
}

void RuntimeContext::update_pixel_simulations(float /*dt*/) {
    if (m_sim_surfaces.empty()) return;
    if (!m_editor_registry) return;

    // Log diagnostic info on first simulation tick per play session
    bool first_update = (m_frame_count <= 1);

    for (auto& state : m_sim_surfaces) {
        if (!m_editor_registry->valid(state->entity)) {
            if (first_update) engine::Logger::instance().warning("Runtime", "Sim entity invalid");
            continue;
        }
        if (!m_editor_registry->all_of<engine::simulation::SimSurface>(state->entity)) {
            if (first_update) engine::Logger::instance().warning("Runtime", "Entity missing SimSurface");
            continue;
        }

        auto& sim_surface = m_editor_registry->get<engine::simulation::SimSurface>(state->entity);
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

        // Run falling-sand simulation (4 Margolus phases)
        state->simulation.simulate(state->pixel_grid, m_render_context);

        // Convert SSBO → RGBA8 color texture via palette lookup compute shader
        m_color_shader.use();
        state->pixel_grid.bind_read_ssbo(0);
        state->palette_ssbo.bind_base(1);
        state->color_texture.bind_as_image(0, engine::graphics::ImageAccess::WriteOnly);
        m_color_shader.set_int("u_grid_width", state->width);
        m_color_shader.set_int("u_grid_height", state->height);
        m_color_shader.set_uint("u_pixel_size", static_cast<uint32_t>(state->pixel_grid.pixel_size()));

        int groups_x = (state->width + 15) / 16;
        int groups_y = (state->height + 15) / 16;
        // TEXTURE_FETCH barrier: ImGui reads this via texture sampler, not imageLoad
        m_render_context.dispatch_compute(groups_x, groups_y, 1, GL_TEXTURE_FETCH_BARRIER_BIT);

        // Update terrain colliders from settled pixels
        if (state->terrain_colliders && m_editor_registry->valid(state->entity)) {
            // Build entity transform for collider positioning
            engine::physics::TerrainColliderManager::EntityTransform et;
            if (m_editor_registry->all_of<engine::Transform>(state->entity)) {
                auto& t = m_editor_registry->get<engine::Transform>(state->entity);
                et.world_x = t.world_x;
                et.world_y = t.world_y;
                et.world_rotation_deg = t.world_rotation;
                et.scale_x = t.world_scale_x;
                et.scale_y = t.world_scale_y;
            }
            if (m_editor_registry->all_of<engine::simulation::PixelGridComponent>(state->entity)) {
                auto& gc = m_editor_registry->get<engine::simulation::PixelGridComponent>(state->entity);
                et.origin_x = gc.origin_x;
                et.origin_y = gc.origin_y;
            }

            // Mark entire grid dirty (simulation changes every pixel every frame)
            state->terrain_colliders->mark_dirty_region(0, 0, state->width, state->height);
            state->terrain_colliders->update_terrain_colliders(state->pixel_grid, et);
        }
    }

    first_update = false;
}

void RuntimeContext::shutdown_pixel_simulations() {
    for (auto& state : m_sim_surfaces) {
        if (state->terrain_colliders) {
            state->terrain_colliders->shutdown();
            state->terrain_colliders.reset();
        }
        state->simulation.shutdown();
        state->pixel_grid.shutdown();
        state->color_texture.destroy();
        state->palette_ssbo.destroy();
    }
    m_sim_surfaces.clear();
    m_color_shader.destroy();
}

uint32_t RuntimeContext::get_sim_texture(entt::entity entity) const {
    for (const auto& state : m_sim_surfaces) {
        if (state->entity == entity) {
            return state->color_texture.handle();
        }
    }
    return 0;
}

} // namespace editor
