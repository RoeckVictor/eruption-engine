#include "AssetPreviewPanel.h"
#include "engine/core/Logger.h"

#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cstdint>

#define STB_IMAGE_IMPLEMENTATION_INCLUDED
#include <stb_image.h>

namespace fs = std::filesystem;

namespace editor {

AssetPreviewPanel::AssetPreviewPanel()
    : Panel("Asset Preview")
{
}

AssetPreviewPanel::~AssetPreviewPanel() {
    unload_texture();
}

void AssetPreviewPanel::on_open() {
}

void AssetPreviewPanel::on_close() {
    unload_texture();
}

void AssetPreviewPanel::on_gui() {
    if (m_current_path.empty()) {
        ImGui::TextDisabled("No asset selected");
        ImGui::TextDisabled("Select a file in the File Browser");
        return;
    }

    // Show asset info
    ImGui::Text("Asset: %s", fs::path(m_current_path).filename().string().c_str());
    ImGui::TextDisabled("Type: %s", m_asset_type.c_str());
    ImGui::Separator();

    if (m_asset_type == "texture" && m_preview_texture.valid()) {
        // Show texture dimensions
        ImGui::Text("Size: %d x %d", m_preview_texture.width(), m_preview_texture.height());
        ImGui::Separator();

        // Calculate size to fit in panel while maintaining aspect ratio
        ImVec2 available = ImGui::GetContentRegionAvail();
        float aspect = static_cast<float>(m_preview_texture.width()) / static_cast<float>(m_preview_texture.height());

        float display_width = available.x;
        float display_height = display_width / aspect;

        if (display_height > available.y) {
            display_height = available.y;
            display_width = display_height * aspect;
        }

        // Center the image
        float offset_x = (available.x - display_width) * 0.5f;
        if (offset_x > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
        }

        // Show the texture using backend-independent imgui_texture_id()
        ImGui::Image(
            (ImTextureID)(uintptr_t)(m_preview_texture.imgui_texture_id()),
            ImVec2(display_width, display_height),
            ImVec2(0, 0),
            ImVec2(1, 1)
        );
    } else if (m_asset_type == "scene") {
        ImGui::TextWrapped("Scene file - double-click to open");

        // Show some basic info if we can parse it
        ImGui::Separator();
        ImGui::TextDisabled("Preview not available for scenes");
    } else if (m_asset_type == "prefab") {
        ImGui::TextWrapped("Prefab file - drag to hierarchy to instantiate");

        ImGui::Separator();
        ImGui::TextDisabled("Preview not available for prefabs");
    } else {
        ImGui::TextDisabled("Preview not available for this file type");
    }

    // Show full path at bottom
    ImGui::Separator();
    ImGui::TextDisabled("Path: %s", m_current_path.c_str());
}

void AssetPreviewPanel::set_asset(const std::string& path) {
    if (path == m_current_path) {
        return;
    }

    m_current_path = path;
    unload_texture();

    if (path.empty()) {
        m_asset_type = "";
        return;
    }

    // Determine asset type by extension
    fs::path file_path(path);
    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
        m_asset_type = "texture";
        load_texture(path);
    } else if (ext == ".scene") {
        m_asset_type = "scene";
    } else if (ext == ".prefab") {
        m_asset_type = "prefab";
    } else if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
        m_asset_type = "source";
    } else if (ext == ".json") {
        m_asset_type = "json";
    } else {
        m_asset_type = "unknown";
    }
}

void AssetPreviewPanel::clear_preview() {
    m_current_path.clear();
    m_asset_type.clear();
    unload_texture();
}

void AssetPreviewPanel::load_texture(const std::string& path) {
    // Load image using stb_image
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) {
        engine::Logger::instance().warning("AssetPreview", "Failed to load texture: %s", path.c_str());
        return;
    }

    // Create texture using RHI-backed graphics::Texture
    if (!m_preview_texture.create_2d(width, height,
                                     engine::graphics::TextureFormat::RGBA8,
                                     engine::graphics::TextureFilter::Linear,
                                     engine::graphics::TextureWrap::ClampToEdge,
                                     data)) {
        engine::Logger::instance().warning("AssetPreview", "Failed to create texture: %s", path.c_str());
        stbi_image_free(data);
        return;
    }

    stbi_image_free(data);

    engine::Logger::instance().info("AssetPreview", "Loaded texture: %s (%dx%d)", path.c_str(), width, height);
}

void AssetPreviewPanel::unload_texture() {
    m_preview_texture.destroy();
}

}
