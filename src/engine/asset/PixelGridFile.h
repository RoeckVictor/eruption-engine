#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace engine::asset {

class VFS;

/// Channel descriptor for .pxg files. Describes what a single channel stores.
struct PxgChannelDesc {
    char name[12] = {};      // e.g. "material", "temperature", "flags"
    uint32_t bits = 8;       // bits per channel (currently always 8)
};
static_assert(sizeof(PxgChannelDesc) == 16, "PxgChannelDesc must be 16 bytes");

/// Fixed-size header for .pxg files.
struct PxgHeader {
    uint8_t magic[4] = {'P','X','G','\0'};
    uint16_t version = 1;
    uint16_t channels_per_pixel = 4;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t uncompressed_size = 0;
    uint32_t compressed_size = 0;
    uint32_t metadata_size = 0;
    uint8_t reserved[28] = {};
};
static_assert(sizeof(PxgHeader) == 56, "PxgHeader size check");

/// A loaded .pxg file.
struct PxgFile {
    PxgHeader header;
    std::vector<PxgChannelDesc> channels;
    std::vector<uint8_t> metadata;
    std::vector<uint8_t> pixels;
};

/// Save pixel data to a .pxg file.
bool pxg_save(const std::string& path,
              uint32_t width, uint32_t height,
              const PxgChannelDesc* channel_descs, uint32_t num_channels,
              const uint8_t* pixel_data,
              const std::string& metadata = "");

/// Load a .pxg file from a physical path.
std::optional<PxgFile> pxg_load(const std::string& path);

/// Load a .pxg file through the VFS.
std::optional<PxgFile> pxg_load(const VFS& vfs, const std::string& virtual_path);

} // namespace engine::asset
