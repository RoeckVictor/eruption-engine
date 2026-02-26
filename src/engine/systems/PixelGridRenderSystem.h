#pragma once

#include "engine/core/System.h"
#include "engine/graphics/Shader.h"
#include "engine/graphics/Texture.h"
#include "engine/rhi/RHIPipeline.h"
#include "engine/rhi/RHIBuffer.h"
#include <entt/fwd.hpp>
#include <unordered_map>
#include <cstdint>
#include <memory>

namespace engine {

class PixelGridLoaderSystem;

namespace render { struct Camera2D; }
namespace simulation { struct PixelGridComponent; class MaterialLibrary; }
namespace render { struct PixelGridRenderer; }
namespace rhi { class RHITexture; }

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
    const char* name() const override { return "PixelGridRenderSystem"; }
    bool init(Engine& engine) override;
    void shutdown() override;
    void render(Engine& engine) override;

    /// Set the loader system reference (must be called after both systems are initialized)
    void set_loader(PixelGridLoaderSystem* loader) { m_loader = loader; }

    /// Set a texture override for an entity (e.g. from a live simulation).
    /// The override is an RHI texture — caller retains ownership.
    /// Pass nullptr to clear the override.
    void set_texture_override(entt::entity entity, const rhi::RHITexture* texture) {
        if (texture == nullptr)
            m_texture_overrides.erase(entity);
        else
            m_texture_overrides[entity] = texture;
    }

private:
    entt::registry* m_registry = nullptr;
    render::Camera2D* m_camera = nullptr;
    PixelGridLoaderSystem* m_loader = nullptr;
    simulation::MaterialLibrary* m_material_lib = nullptr;  // Cached for legacy palette fallback

    graphics::Shader m_sprite_shader;
    std::unique_ptr<rhi::RHIPipeline> m_pipeline;
    std::unique_ptr<rhi::RHIBuffer> m_quad_vbo;

    // Texture cache (entity → texture)
    std::unordered_map<entt::entity, graphics::Texture> m_cached_textures;

    // External texture overrides (e.g. simulation textures) — not owned
    std::unordered_map<entt::entity, const rhi::RHITexture*> m_texture_overrides;

    void ensure_texture_for_entity(entt::entity entity, const simulation::PixelGridComponent& grid_comp);

    /// Remove cached textures for entities that no longer exist in the registry.
    void purge_stale_textures();
};

} // namespace engine
