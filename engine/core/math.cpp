// ROXBOX - value noise implementation
#include "math.h"
#include <cstdint>

namespace rb {

// tiny deterministic hash used to build the value-noise lattice
inline uint32_t hash3(int x, int y, int z, uint32_t seed) {
    uint32_t h = seed ^ (uint32_t)(x * 374761393u) ^ (uint32_t)(y * 668265263u) ^ (uint32_t)(z * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return h;
}

float valueNoise3(const vec3& p, uint32_t seed) {
    int ix = (int)std::floor(p.x), iy = (int)std::floor(p.y), iz = (int)std::floor(p.z);
    // hash lattice value -> [-1, 1]
    float v = (float)((int)(hash3(ix, iy, iz, seed) % 20001) - 10000) / 10000.f;
    return v;
}

} // namespace rb
