#pragma once

#include "engine/core/System.h"
#include "engine/graphics/Shader.h"
#include "engine/graphics/Texture.h"
#include <entt/fwd.hpp>
#include <unordered_map>
#include <cstdint>

namespace engine {

class PixelGridLoaderSystem;

namespace render { struct Camera2D; }
namespace simulation { struct PixelGridComponent; }
namespace render { struct PixelGridRenderer; }

/// Renders entities with PixelGridRenderer + PixelGridComponent.
///
/// This system renders pixel grids at entity Transform positions, supporting:
/// - Layer ordering
/// - Opacity and tint
/// - Pixel-perfect rendering
/// - Camera transform
///
/// Execution Order: Runs in render phase, after GridRenderSystem.
class PixelGridRenderSystem : public System {
public:
    bool init(Engine& engine) override;
    void shutdown() override;
    void render(Engine& engine) override;

    /// Set the loader system reference (must be called after both systems are initialized)
    void set_loader(PixelGridLoaderSystem* loader) { m_loader = loader; }

private:
    entt::registry* m_registry = nullptr;
    render::Camera2D* m_camera = nullptr;
    PixelGridLoaderSystem* m_loader = nullptr;

    graphics::Shader m_sprite_shader;
    uint32_t m_quad_vao = 0;
    uint32_t m_quad_vbo = 0;

    // Texture cache (entity → texture)
    std::unordered_map<entt::entity, graphics::Texture> m_cached_textures;

    void ensure_texture_for_entity(entt::entity entity, const simulation::PixelGridComponent& grid_comp);
};

} // namespace engine
