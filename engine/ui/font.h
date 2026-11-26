// ROXBOX - procedural 6x8 bitmap font + glyph atlas texture
#pragma once
#include <cstdint>
#include <vector>

namespace rb {

inline constexpr int FONT_GLYPH_W = 6;
inline constexpr int FONT_GLYPH_H = 8;
inline constexpr int FONT_ATLAS_SIZE = 128; // 16x16 glyphs

// returns RGBA atlas (white glyphs, alpha mask) and glyph count
std::vector<uint8_t> generateFontAtlas();

// glyph index for a char (32..126), 0 for space
int fontGlyphIndex(char c);

} // namespace rb
