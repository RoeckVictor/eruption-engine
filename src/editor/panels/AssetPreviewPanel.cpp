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

    if (m_asset_type == "texture" && m_preview_texture != 0) {
        // Show texture dimensions
        ImGui::Text("Size: %d x %d", m_texture_width, m_texture_height);
        ImGui::Separator();

        // Calculate size to fit in panel while maintaining aspect ratio
        ImVec2 available = ImGui::GetContentRegionAvail();
        float aspect = static_cast<float>(m_texture_width) / static_cast<float>(m_texture_height);

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

        // Show the texture
        ImGui::Image(
            (ImTextureID)(uintptr_t)m_preview_texture,
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
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

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

    // Create OpenGL texture
    glGenTextures(1, &m_preview_texture);
    glBindTexture(GL_TEXTURE_2D, m_preview_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    m_texture_width = width;
    m_texture_height = height;

    stbi_image_free(data);

    engine::Logger::instance().info("AssetPreview", "Loaded texture: %s (%dx%d)", path.c_str(), width, height);
}

void AssetPreviewPanel::unload_texture() {
    if (m_preview_texture != 0) {
        glDeleteTextures(1, &m_preview_texture);
        m_preview_texture = 0;
    }
    m_texture_width = 0;
    m_texture_height = 0;
}

} // namespace editor
