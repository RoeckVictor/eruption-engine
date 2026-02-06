#pragma once

#include <memory>
#include <string>

namespace engine::asset {

class VFS;

/// Type-specific asset loading interface.
/// Specialize this template for each asset type (Shader, Texture, etc.).
/// The engine provides built-in specializations; games can add their own.
template<typename T>
struct AssetLoader {
    static std::unique_ptr<T> load(const VFS& vfs, const std::string& virtual_path);
    static bool reload(T& asset, const VFS& vfs, const std::string& virtual_path);
};

} // namespace engine::asset
