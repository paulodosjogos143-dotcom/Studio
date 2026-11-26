// ROXBOX - procedural texture atlas (256x256, 16x16 tiles) + color palette
// Zero external assets: every block face is generated at startup.
#pragma once
#include <cstdint>
#include <vector>

namespace rb {

inline constexpr int ATLAS_SIZE = 256;
inline constexpr int TILE_SIZE = 16;

// generate the block texture atlas (RGBA, 256x256)
std::vector<uint8_t> generateAtlas();
// generate the 24x1 RGBA palette texture
std::vector<uint8_t> generatePaletteTexture();

} // namespace rb
