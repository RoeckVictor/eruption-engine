#pragma once

#include "engine/core/System.h"
#include "engine/graphics/Shader.h"
#include "engine/graphics/Texture.h"
#include "engine/asset/AssetHandle.h"
#include <entt/fwd.hpp>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace engine {

namespace render { struct Camera2D; struct Image; }
namespace asset { class AssetDatabase; }

/// Renders entities with Image components.
///
/// Handles both world-space (Transform) and screen-space (ScreenRect) entities.
/// If sprite_path is empty, renders a solid color quad (white texture * color).
/// If sprite_path is set, renders the texture multiplied by color for tinting.
class ImageRenderSystem : public System {
public:
    bool init(Engine& engine) override;
    void shutdown() override;
    void render(Engine& engine) override;

private:
    entt::registry* m_registry = nullptr;
    render::Camera2D* m_camera = nullptr;
    asset::AssetDatabase* m_assets = nullptr;

    graphics::Shader m_shader;
    graphics::Texture m_white_texture;  // 1x1 white pixel for solid color quads

    uint32_t m_quad_vao = 0;
    uint32_t m_quad_vbo = 0;

    // Texture cache: sprite_path -> handle (safe across hot-reloads)
    std::unordered_map<std::string, asset::Handle<graphics::Texture>> m_texture_cache;

    /// Get or load a texture from the asset database.
    /// Returns the white texture if path is empty.
    graphics::Texture* get_texture(const std::string& sprite_path);

    /// Create the 1x1 white texture for solid color rendering.
    bool create_white_texture();
};

} // namespace engine
