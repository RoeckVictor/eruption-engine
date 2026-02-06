#include "engine/physics/PixelBodyManager.h"
#include "engine/simulation/PixelGrid.h"
#include "engine/particles/ParticleBuffer.h"
#include "engine/core/Log.h"
#include <algorithm>

namespace engine::physics {

bool PixelBodyManager::init(PhysicsWorld& world, int terrain_chunk_size, int min_pixels) {
    // Validate that the PhysicsWorld has a valid underlying Box2D world
    if (!world.valid()) {
        ENGINE_ERR("PixelBodyManager::init() - Invalid PhysicsWorld provided");
        return false;
    }

    m_world = &world;
    min_body_pixels = min_pixels;

    // Initialize delegated components
    if (!m_terrain_colliders.init(world, terrain_chunk_size)) {
        ENGINE_ERR("PixelBodyManager::init() - Failed to init terrain colliders");
        return false;
    }

    return true;
}

void PixelBodyManager::shutdown() {
    // Check if init() was called successfully
    if (!m_world) {
        ENGINE_LOG("PixelBodyManager::shutdown() called before init() or after previous shutdown()");
        return;
    }

    // Shutdown delegated components
    m_terrain_colliders.shutdown();

    // Destroy all pixel bodies
    for (auto& body : m_bodies) {
        body->shutdown(*m_world);
    }
    m_bodies.clear();
    m_world = nullptr;
}

// --- Body lifecycle ---

PixelBody* PixelBodyManager::create_body(const uint8_t* materials, const uint8_t* categories,
                                          int w, int h, float world_px, float world_py,
                                          bool is_dynamic, bool indestructible) {
    if (!m_world) {
        ENGINE_ERR("PixelBodyManager::create_body() called before init()");
        return nullptr;
    }
    auto body = std::make_unique<PixelBody>();
    if (!body->init(*m_world, materials, categories, w, h, world_px, world_py, is_dynamic, indestructible)) {
        return nullptr;
    }
    PixelBody* ptr = body.get();
    m_bodies.push_back(std::move(body));
    return ptr;
}

void PixelBodyManager::destroy_body(PixelBody* body) {
    if (!body || !m_world) return;

    auto it = std::find_if(m_bodies.begin(), m_bodies.end(),
        [body](const std::unique_ptr<PixelBody>& b) { return b.get() == body; });

    if (it != m_bodies.end()) {
        (*it)->shutdown(*m_world);
        m_bodies.erase(it);
    }
}

// --- Per-frame pipeline ---

void PixelBodyManager::step_physics(float dt) {
    if (!m_world) {
        ENGINE_ERR("PixelBodyManager::step_physics() called before init()");
        return;
    }
    m_world->step(dt);
}

void PixelBodyManager::stamp_all(simulation::PixelGrid& grid, particles::ParticleBuffer* particle_buffer) {
    if (!m_world) {
        ENGINE_ERR("PixelBodyManager::stamp_all() called before init()");
        return;
    }
    // Convert m_bodies to raw pointer vector for stamper
    std::vector<PixelBody*> body_ptrs;
    body_ptrs.reserve(m_bodies.size());
    for (auto& body : m_bodies) {
        body_ptrs.push_back(body.get());
    }

    m_stamper.stamp_all(body_ptrs, *m_world, grid, particle_buffer);
}

void PixelBodyManager::clear_all(simulation::PixelGrid& grid) {
    m_stamper.clear_all(grid);
}

void PixelBodyManager::update_dirty_shapes() {
    if (!m_world) return;
    for (auto& body : m_bodies) {
        if (body->is_dirty()) {
            body->recompute_shapes(*m_world);
        }
    }
}

int PixelBodyManager::handle_splits() {
    if (!m_world) return 0;
    int new_bodies = 0;
    std::vector<PixelBody*> to_remove;

    // Collect bodies that need splitting (can't modify m_bodies while iterating)
    std::vector<std::pair<PixelBody*, std::vector<PixelBody::Component>>> splits;

    for (auto& body : m_bodies) {
        if (!body->is_dirty() && body->pixel_count() >= min_body_pixels) continue;

        // Check for too-small bodies
        if (body->pixel_count() < min_body_pixels) {
            to_remove.push_back(body.get());
            continue;
        }

        int components = body->count_components();
        if (components > 1) {
            auto extracted = body->extract_components();
            if (!extracted.empty()) {
                splits.push_back({body.get(), std::move(extracted)});
            }
        }
    }

    // Process splits
    for (auto& [original, components] : splits) {
        // Get the original body's velocity for transfer
        b2Vec2 lin_vel = m_world->get_body_linear_velocity(original->body_id());
        float ang_vel = m_world->get_body_angular_velocity(original->body_id());
        float orig_wx = original->world_x(*m_world);
        float orig_wy = original->world_y(*m_world);
        float orig_angle = original->rotation(*m_world);
        float orig_cx = original->local_center_x();
        float orig_cy = original->local_center_y();
        float cos_a = std::cos(orig_angle);
        float sin_a = std::sin(orig_angle);

        to_remove.push_back(original);

        for (auto& comp : components) {
            if (static_cast<int>(comp.materials.size()) < min_body_pixels) {
                // Too small, skip (caller can convert to particles)
                continue;
            }

            // Compute world position for the new component
            float dx = comp.center_x - orig_cx;
            float dy = comp.center_y - orig_cy;
            float new_wx = orig_wx + dx * cos_a - dy * sin_a;
            float new_wy = orig_wy + dx * sin_a + dy * cos_a;

            PixelBody* new_body = create_body(
                comp.materials.data(), comp.categories.data(),
                comp.width, comp.height, new_wx, new_wy, true);

            if (new_body) {
                // Transfer velocity
                m_world->set_body_linear_velocity(new_body->body_id(), lin_vel.x, lin_vel.y);
                b2Body_SetAngularVelocity(new_body->body_id(), ang_vel);
                new_bodies++;
            }
        }
    }

    // Remove originals
    for (auto* body : to_remove) {
        destroy_body(body);
    }

    return new_bodies;
}

// --- Terrain collider management ---

void PixelBodyManager::update_terrain_colliders(simulation::PixelGrid& grid) {
    m_terrain_colliders.update_terrain_colliders(grid);
}

void PixelBodyManager::mark_terrain_dirty_region(int x, int y, int w, int h) {
    m_terrain_colliders.mark_dirty_region(x, y, w, h);
}

void PixelBodyManager::mark_terrain_dirty_near_bodies(float margin) {
    if (!m_world) return;
    // Convert m_bodies to raw pointer vector for terrain colliders
    std::vector<PixelBody*> body_ptrs;
    body_ptrs.reserve(m_bodies.size());
    for (auto& body : m_bodies) {
        body_ptrs.push_back(body.get());
    }

    m_terrain_colliders.mark_dirty_near_bodies(body_ptrs, *m_world, margin);
}

} // namespace engine::physics
