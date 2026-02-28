// Shared particle struct definition.
// Must match engine::particles::GpuParticle (32 bytes, std430).

struct Particle {
    vec2 pos;           // world pixel position
    vec2 vel;           // pixels/sec
    uint material;      // CA material ID
    float lifetime;     // seconds remaining
    uint flags;         // 0=dead, 1=alive, 2=reintegrate
    uint color;         // packed RGBA (r|g|b|a as bytes 0-3)
};
