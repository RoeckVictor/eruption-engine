#pragma once

#include <string>
#include <cstdint>
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>

namespace engine::asset {

// A 128-bit GUID for uniquely identifying assets
// Stored as two 64-bit integers for efficient comparison and hashing
struct AssetGUID {
    uint64_t high = 0;
    uint64_t low = 0;

    bool valid() const { return high != 0 || low != 0; }
    bool operator==(const AssetGUID& o) const { return high == o.high && low == o.low; }
    bool operator!=(const AssetGUID& o) const { return !(*this == o); }
    bool operator<(const AssetGUID& o) const {
        return high < o.high || (high == o.high && low < o.low);
    }

    static AssetGUID generate() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;

        AssetGUID guid;
        guid.high = dist(gen);
        guid.low = dist(gen);

        // Set version 4 (random) bits: version in bits 12-15 of high
        guid.high = (guid.high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
        // Set variant bits: variant 1 (10xx) in bits 62-63 of low
        guid.low = (guid.low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

        return guid;
    }

    std::string to_string() const {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');

        // high: xxxxxxxx-xxxx-xxxx
        ss << std::setw(8) << ((high >> 32) & 0xFFFFFFFF) << '-';
        ss << std::setw(4) << ((high >> 16) & 0xFFFF) << '-';
        ss << std::setw(4) << (high & 0xFFFF) << '-';

        // low: xxxx-xxxxxxxxxxxx
        ss << std::setw(4) << ((low >> 48) & 0xFFFF) << '-';
        ss << std::setw(12) << (low & 0xFFFFFFFFFFFFULL);

        return ss.str();
    }

    static AssetGUID from_string(const std::string& str) {
        AssetGUID guid;
        if (str.length() != 36) return guid;

        try {
            // Remove dashes and parse
            std::string clean;
            for (char c : str) {
                if (c != '-') clean += c;
            }
            if (clean.length() != 32) return guid;

            // Parse high (first 16 hex chars)
            guid.high = std::stoull(clean.substr(0, 16), nullptr, 16);
            // Parse low (last 16 hex chars)
            guid.low = std::stoull(clean.substr(16, 16), nullptr, 16);
        } catch (...) {
            guid.high = guid.low = 0;
        }

        return guid;
    }
};

}

namespace std {
template<>
struct hash<engine::asset::AssetGUID> {
    size_t operator()(const engine::asset::AssetGUID& guid) const {
        // Combine high and low with XOR and bit mixing
        size_t h = hash<uint64_t>()(guid.high);
        size_t l = hash<uint64_t>()(guid.low);
        return h ^ (l + 0x9e3779b9 + (h << 6) + (h >> 2));
    }
};
}
