#include "engine/asset/PixelGridFile.h"
#include "engine/asset/VFS.h"
#include "engine/core/Log.h"
#include <lz4.h>
#include <cstring>
#include <fstream>

namespace engine::asset {

bool pxg_save(const std::string& path,
              uint32_t width, uint32_t height,
              const PxgChannelDesc* channel_descs, uint32_t num_channels,
              const uint8_t* pixel_data,
              const std::string& metadata) {

    uint32_t uncompressed_size = width * height * num_channels;

    // Compress with LZ4
    int max_compressed = LZ4_compressBound(static_cast<int>(uncompressed_size));
    std::vector<char> compressed(max_compressed);

    int compressed_size = LZ4_compress_default(
        reinterpret_cast<const char*>(pixel_data),
        compressed.data(),
        static_cast<int>(uncompressed_size),
        max_compressed);

    if (compressed_size <= 0) {
        ENGINE_ERR("pxg_save: LZ4 compression failed for '%s'", path.c_str());
        return false;
    }

    // Build header
    PxgHeader header;
    header.version = 1;
    header.channels_per_pixel = static_cast<uint16_t>(num_channels);
    header.width = width;
    header.height = height;
    header.uncompressed_size = uncompressed_size;
    header.compressed_size = static_cast<uint32_t>(compressed_size);
    header.metadata_size = static_cast<uint32_t>(metadata.size());

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        ENGINE_ERR("pxg_save: Cannot open '%s' for writing", path.c_str());
        return false;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write channel descriptors
    for (uint32_t i = 0; i < num_channels; ++i) {
        file.write(reinterpret_cast<const char*>(&channel_descs[i]), sizeof(PxgChannelDesc));
    }

    // Write metadata
    if (!metadata.empty()) {
        file.write(metadata.data(), metadata.size());
    }

    // Write compressed pixel data
    file.write(compressed.data(), compressed_size);

    if (!file) {
        ENGINE_ERR("pxg_save: Write error for '%s'", path.c_str());
        return false;
    }

    ENGINE_LOG("pxg_save: Saved '%s' (%ux%u, %u channels, %d -> %d bytes)",
               path.c_str(), width, height, num_channels,
               uncompressed_size, compressed_size);
    return true;
}

static std::optional<PxgFile> pxg_load_from_bytes(const uint8_t* data, size_t size,
                                                    const char* source_name) {
    if (size < sizeof(PxgHeader)) {
        ENGINE_ERR("pxg_load: File too small (%zu bytes) [%s]", size, source_name);
        return std::nullopt;
    }

    PxgFile result;
    std::memcpy(&result.header, data, sizeof(PxgHeader));
    size_t offset = sizeof(PxgHeader);

    // Verify magic
    if (result.header.magic[0] != 'P' || result.header.magic[1] != 'X' ||
        result.header.magic[2] != 'G' || result.header.magic[3] != '\0') {
        ENGINE_ERR("pxg_load: Invalid magic number [%s]", source_name);
        return std::nullopt;
    }

    if (result.header.version != 1) {
        ENGINE_ERR("pxg_load: Unsupported version %u [%s]", result.header.version, source_name);
        return std::nullopt;
    }

    // Read channel descriptors
    uint32_t num_channels = result.header.channels_per_pixel;
    size_t channels_bytes = num_channels * sizeof(PxgChannelDesc);
    if (offset + channels_bytes > size) {
        ENGINE_ERR("pxg_load: Truncated channel descriptors [%s]", source_name);
        return std::nullopt;
    }
    result.channels.resize(num_channels);
    std::memcpy(result.channels.data(), data + offset, channels_bytes);
    offset += channels_bytes;

    // Read metadata
    if (result.header.metadata_size > 0) {
        if (offset + result.header.metadata_size > size) {
            ENGINE_ERR("pxg_load: Truncated metadata [%s]", source_name);
            return std::nullopt;
        }
        result.metadata.assign(data + offset, data + offset + result.header.metadata_size);
        offset += result.header.metadata_size;
    }

    // Read and decompress pixel data
    if (offset + result.header.compressed_size > size) {
        ENGINE_ERR("pxg_load: Truncated compressed data [%s]", source_name);
        return std::nullopt;
    }

    result.pixels.resize(result.header.uncompressed_size);
    int decompressed = LZ4_decompress_safe(
        reinterpret_cast<const char*>(data + offset),
        reinterpret_cast<char*>(result.pixels.data()),
        static_cast<int>(result.header.compressed_size),
        static_cast<int>(result.header.uncompressed_size));

    if (decompressed < 0 || static_cast<uint32_t>(decompressed) != result.header.uncompressed_size) {
        ENGINE_ERR("pxg_load: LZ4 decompression failed [%s]", source_name);
        return std::nullopt;
    }

    ENGINE_LOG("pxg_load: Loaded '%s' (%ux%u, %u channels)",
               source_name, result.header.width, result.header.height, num_channels);
    return result;
}

std::optional<PxgFile> pxg_load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ENGINE_ERR("pxg_load: Cannot open '%s'", path.c_str());
        return std::nullopt;
    }

    auto size = file.tellg();
    if (size < 0) return std::nullopt;

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return pxg_load_from_bytes(buffer.data(), buffer.size(), path.c_str());
}

std::optional<PxgFile> pxg_load(const VFS& vfs, const std::string& virtual_path) {
    auto file_data = vfs.read_file(virtual_path);
    if (file_data.is_err()) {
        ENGINE_ERR("pxg_load: Cannot read '%s' from VFS: %s",
                   virtual_path.c_str(), file_data.error().message.c_str());
        return std::nullopt;
    }
    return pxg_load_from_bytes(file_data.value().bytes.data(), file_data.value().bytes.size(),
                                virtual_path.c_str());
}

} // namespace engine::asset
