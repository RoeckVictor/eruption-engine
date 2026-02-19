#pragma once

#include "engine/asset/AssetLoader.h"
#include "engine/asset/VFS.h"
#include "engine/render/DynamicFont.h"
#include "engine/core/Log.h"
#include <stb_truetype.h>
#include <memory>
#include <string>

namespace engine::asset {

/// DynamicFont loader specialization.
/// Loads TTF/OTF files directly using stb_truetype.
template<>
struct AssetLoader<render::DynamicFont> {
    static std::unique_ptr<render::DynamicFont> load(const VFS& vfs,
                                                      const std::string& virtual_path) {
        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) {
            ENGINE_ERR("DynamicFontLoader: Cannot read '%s': %s",
                       virtual_path.c_str(), file_data.error().message.c_str());
            return nullptr;
        }

        auto font = std::make_unique<render::DynamicFont>();
        font->source_path = virtual_path;

        // Copy TTF data (stb_truetype requires persistent buffer)
        font->ttf_data = std::move(file_data.value().bytes);

        // Initialize stb_truetype
        font->font_info = std::make_unique<stbtt_fontinfo>();

        int offset = stbtt_GetFontOffsetForIndex(font->ttf_data.data(), 0);
        if (offset < 0) {
            ENGINE_ERR("DynamicFontLoader: Invalid font file '%s'", virtual_path.c_str());
            return nullptr;
        }

        if (!stbtt_InitFont(font->font_info.get(), font->ttf_data.data(), offset)) {
            ENGINE_ERR("DynamicFontLoader: Failed to init font '%s'", virtual_path.c_str());
            return nullptr;
        }

        ENGINE_LOG("DynamicFontLoader: Loaded '%s'", virtual_path.c_str());
        return font;
    }

    static bool reload(render::DynamicFont& font, const VFS& vfs,
                       const std::string& virtual_path) {
        // Clear all cached atlases on reload
        font.size_atlases.clear();

        auto file_data = vfs.read_file(virtual_path);
        if (file_data.is_err()) {
            return false;
        }

        font.ttf_data = std::move(file_data.value().bytes);

        int offset = stbtt_GetFontOffsetForIndex(font.ttf_data.data(), 0);
        if (offset < 0) {
            return false;
        }

        return stbtt_InitFont(font.font_info.get(), font.ttf_data.data(), offset);
    }
};

} // namespace engine::asset
