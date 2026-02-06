#pragma once

#include "engine/asset/AssetLoader.h"
#include "engine/asset/VFS.h"
#include "engine/graphics/Texture.h"
#include "engine/core/Log.h"
#include <stb_image.h>
#include <memory>
#include <string>

namespace engine::asset {

/// Texture loader specialization.
/// Loads image files (PNG, JPG, BMP) via stb_image as RGBA8 textures.
template<>
struct AssetLoader<graphics::Texture> {
    static std::unique_ptr<graphics::Texture> load(const VFS& vfs, const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) {
            ENGINE_ERR("TextureLoader: Cannot read '%s': %s",
                       virtual_path.c_str(), file_data.error().message.c_str());
            return nullptr;
        }

        int width, height, channels;
        unsigned char* pixels = stbi_load_from_memory(
            file_data.value().bytes.data(),
            static_cast<int>(file_data.value().bytes.size()),
            &width, &height, &channels, 4);  // Force RGBA

        if (!pixels) {
            ENGINE_ERR("TextureLoader: stbi_load failed for '%s': %s",
                       virtual_path.c_str(), stbi_failure_reason());
            return nullptr;
        }

        auto texture = std::make_unique<graphics::Texture>();
        bool ok = texture->create_2d(width, height,
                                      graphics::TextureFormat::RGBA8,
                                      graphics::TextureFilter::Nearest,
                                      graphics::TextureWrap::ClampToEdge,
                                      pixels);
        stbi_image_free(pixels);

        if (!ok) {
            ENGINE_ERR("TextureLoader: Failed to create GPU texture for '%s'", virtual_path.c_str());
            return nullptr;
        }

        ENGINE_LOG("TextureLoader: Loaded '%s' (%dx%d)", virtual_path.c_str(), width, height);
        return texture;
    }

    static bool reload(graphics::Texture& texture, const VFS& vfs,
                       const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) return false;

        int width, height, channels;
        unsigned char* pixels = stbi_load_from_memory(
            file_data.value().bytes.data(),
            static_cast<int>(file_data.value().bytes.size()),
            &width, &height, &channels, 4);

        if (!pixels) return false;

        // Only reload if dimensions match (can't resize GPU texture easily)
        if (width != texture.width() || height != texture.height()) {
            ENGINE_ERR("TextureLoader: Reload dimension mismatch for '%s'", virtual_path.c_str());
            stbi_image_free(pixels);
            return false;
        }

        texture.upload_sub_2d(0, 0, width, height, pixels);
        stbi_image_free(pixels);
        return true;
    }
};

} // namespace engine::asset
