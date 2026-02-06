#pragma once

#include <cstdint>
#include <string>
#include <filesystem>
#include <functional>

namespace engine::asset {

/// Lightweight value-type handle to a loaded asset.
/// 32-bit index + 32-bit generation for safe invalidation.
template<typename T>
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0;

    bool valid() const { return generation != 0; }
    bool operator==(const Handle& o) const { return index == o.index && generation == o.generation; }
    bool operator!=(const Handle& o) const { return !(*this == o); }
};

/// Metadata stored per asset slot in the database.
struct AssetMeta {
    std::string virtual_path;
    std::string physical_path;
    std::filesystem::file_time_type last_modified{};
    uint32_t generation = 0;
};

} // namespace engine::asset

namespace std {
template<typename T>
struct hash<engine::asset::Handle<T>> {
    size_t operator()(const engine::asset::Handle<T>& h) const {
        return hash<uint64_t>()((uint64_t(h.generation) << 32) | h.index);
    }
};
} // namespace std
