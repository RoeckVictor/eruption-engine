#pragma once

#include "engine/asset/AssetLoader.h"
#include "engine/asset/VFS.h"
#include "engine/graphics/Shader.h"
#include "engine/core/Log.h"
#include <memory>
#include <string>

namespace engine::asset {

/// Shader loader specialization.
///
/// Virtual path conventions:
///   "shaders/sprite"          → loads "shaders/sprite.vert" + "shaders/sprite.frag"
///   "shaders/sim_step.comp"   → loads a compute shader
template<>
struct AssetLoader<graphics::Shader> {
    static std::unique_ptr<graphics::Shader> load(const VFS& vfs, const std::string& virtual_path) {
        auto shader = std::make_unique<graphics::Shader>();

        // Check if this is a compute shader
        if (virtual_path.size() > 5 &&
            virtual_path.substr(virtual_path.size() - 5) == ".comp") {
            auto physical = vfs.resolve(virtual_path);
            if (physical.is_err()) {
                ENGINE_ERR("ShaderLoader: Cannot resolve compute shader '%s': %s",
                           virtual_path.c_str(), physical.error().message.c_str());
                return nullptr;
            }
            if (!shader->load_compute(physical.value().c_str())) {
                return nullptr;
            }
            return shader;
        }

        // Graphics shader: append .vert and .frag
        std::string vert_path = virtual_path + ".vert";
        std::string frag_path = virtual_path + ".frag";

        auto vert_physical = vfs.resolve(vert_path);
        auto frag_physical = vfs.resolve(frag_path);
        if (vert_physical.is_err() || frag_physical.is_err()) {
            ENGINE_ERR("ShaderLoader: Cannot resolve shader pair '%s' (.vert/.frag)",
                       virtual_path.c_str());
            return nullptr;
        }

        if (!shader->load_graphics(vert_physical.value().c_str(), frag_physical.value().c_str())) {
            return nullptr;
        }

        return shader;
    }

    static bool reload(graphics::Shader& shader, const VFS& /*vfs*/,
                       const std::string& /*virtual_path*/) {
        // Shader already has built-in hot-reload via try_reload()
        return shader.try_reload();
    }
};

} // namespace engine::asset
