#pragma once

#include <cstdint>

namespace engine::graphics {

class Texture;

/// Asynchronous GPU texture readback using double-buffered Pixel Buffer Objects.
///
/// Overlaps GPU→CPU transfer with CPU processing by using two PBOs:
/// one being filled by the GPU while the other is read by the CPU.
///
/// Typical workflow:
///   Frame N:   begin() starts a non-blocking GPU→PBO transfer
///   Frame N+1: read() maps the completed PBO and copies data to CPU
///
/// The first call to read() after init returns false (no data yet).
/// Callers should handle this with a synchronous fallback on the first frame.
class PixelReadback {
public:
    PixelReadback() = default;
    ~PixelReadback();

    PixelReadback(const PixelReadback&) = delete;
    PixelReadback& operator=(const PixelReadback&) = delete;

    /// Allocate two PBOs of max_bytes each.
    void init(int max_bytes);
    void shutdown();

    /// Start an async readback of a sub-region from a 2D texture.
    /// If the region wraps (e.g. ring-buffer), pass two sub-regions.
    /// The data is laid out contiguously: region1 first, then region2.
    ///
    /// Single region:
    void begin(const Texture& tex, int x, int y, int w, int h);

    /// Split region (for ring-buffer wrap): reads two texture sub-regions
    /// into the same PBO contiguously.
    void begin_split(const Texture& tex,
                     int x1, int y1, int w1, int h1,
                     int x2, int y2, int w2, int h2);

    /// Copy the result of the most recently completed readback into dst.
    /// Returns true if data was available and copied, false if nothing ready.
    bool read(void* dst, int dst_size);

    /// True if a completed readback is available for reading.
    bool has_result() const { return m_has_data[1 - m_write_idx]; }

private:
    uint32_t m_pbos[2] = {};
    int m_write_idx = 0;        // PBO currently being written by GPU
    int m_max_bytes = 0;
    bool m_has_data[2] = {};    // whether each PBO has valid data
    int m_data_size[2] = {};    // bytes of valid data in each PBO
};

} // namespace engine::graphics
