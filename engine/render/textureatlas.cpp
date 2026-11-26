#include "textureatlas.h"
#include "../core/math.h"
#include "../world/blocks.h"

namespace rb {

namespace {

struct Pixel { uint8_t r, g, b, a; };

inline uint32_t hash2(int x, int y, uint32_t seed) {
    uint32_t h = seed ^ (uint32_t)(x * 73856093u) ^ (uint32_t)(y * 19349663u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
inline float noise01(int x, int y, uint32_t seed) {
    return (float)((hash2(x, y, seed) >> 8) * (1.0 / 16777216.0));
}
inline Pixel tint(Pixel p, float f) {
    p.r = (uint8_t)std::min(255.f, p.r * f);
    p.g = (uint8_t)std::min(255.f, p.g * f);
    p.b = (uint8_t)std::min(255.f, p.b * f);
    return p;
}

// draw one 16x16 tile for a block face. face: 0=+X 1=-X 2=+Y 3=-Y 4=+Z 5=-Z
void drawTile(std::vector<uint8_t>& atlas, int tileIdx, uint8_t block, int face) {
    uint32_t seed = (uint32_t)(tileIdx * 131u + 7u);
    int ox = (tileIdx % 16) * TILE_SIZE, oy = (tileIdx / 16) * TILE_SIZE;
    for (int y = 0; y < TILE_SIZE; ++y) {
        for (int x = 0; x < TILE_SIZE; ++x) {
            Pixel p{255, 255, 255, 255};
            float n = noise01(x, y, seed);
            switch (block) {
                case BLK_GRASS:
                    if (face == 2) { // top: green noise
                        p = {(uint8_t)(95 + n * 70), (uint8_t)(165 + n * 60), (uint8_t)(70 + n * 40), 255};
                    } else { // side: dirt + green fringe on top
                        p = {(uint8_t)(130 + n * 40), (uint8_t)(95 + n * 30), (uint8_t)(55 + n * 25), 255};
                        if (y < 3) {
                            p = tint(p, 0.55f);
                            p.g = (uint8_t)std::min(255.f, (float)p.g * 1.6f);
                        }
                    }
                    break;
                case BLK_DIRT:
                    p = {(uint8_t)(130 + n * 45), (uint8_t)(95 + n * 35), (uint8_t)(55 + n * 30), 255};
                    break;
                case BLK_STONE:
                    p = {(uint8_t)(120 + n * 30), (uint8_t)(120 + n * 30), (uint8_t)(125 + n * 30), 255};
                    if (n > 0.85f) p = tint(p, 0.8f);
                    break;
                case BLK_COBBLE: {
                    int cx = x / 4, cy = y / 4;
                    float v = 0.85f + noise01(cx * 7 + 1, cy * 7 + 3, seed) * 0.3f;
                    p = {(uint8_t)(95 * v), (uint8_t)(95 * v), (uint8_t)(100 * v), 255};
                    if ((x + cy) % 4 == 0 || (y + cx) % 4 == 0) p = tint(p, 0.7f);
                    break;
                }
                case BLK_WOOD: {
                    float ring = std::sin((float)y * 1.4f + seed) * 0.5f + 0.5f;
                    p = {(uint8_t)(110 + ring * 50), (uint8_t)(80 + ring * 30), (uint8_t)(40 + ring * 20), 255};
                    if (face == 2 || face == 3) { // end grain rings
                        float r2 = std::sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f));
                        p = tint(p, 0.85f + ((int)(r2) % 2 == 0 ? 0.25f : 0.f));
                    }
                    break;
                }
                case BLK_PLANKS:
                    p = {(uint8_t)(150 + n * 30), (uint8_t)(110 + n * 25), (uint8_t)(60 + n * 20), 255};
                    if (x % 4 == 0) p = tint(p, 0.78f);
                    if (y % 8 == 0) p = tint(p, 0.88f);
                    break;
                case BLK_LEAVES:
                    p = {(uint8_t)(50 + n * 60), (uint8_t)(105 + n * 70), (uint8_t)(35 + n * 40), 255};
                    if (n > 0.9f) p = tint(p, 0.75f);
                    break;
                case BLK_SAND:
                    p = {(uint8_t)(220 + n * 20), (uint8_t)(200 + n * 25), (uint8_t)(130 + n * 30), 255};
                    if (n > 0.92f) p = tint(p, 0.85f);
                    break;
                case BLK_WATER:
                    p = {(uint8_t)(50 + n * 30), (uint8_t)(90 + n * 40), (uint8_t)(200 + n * 30), 130};
                    if ((int)(y * 2 + seed) % 3 == 0) {
                        p = tint(p, 1.15f);
                        p.a = 150;
                    }
                    break;
                case BLK_GLASS:
                    p = {(uint8_t)(200 + n * 20), (uint8_t)(220 + n * 15), (uint8_t)(240 + n * 10), 255};
                    if (x == 0 || y == 0 || x == TILE_SIZE - 1 || y == TILE_SIZE - 1) {
                        p = {150, 170, 190, 255};
                    } else if (n > 0.8f) {
                        p = {220, 235, 250, 90};
                    } else {
                        p.a = 80;
                    }
                    break;
                case BLK_BRICK:
                    p = {(uint8_t)(150 + n * 30), (uint8_t)(70 + n * 20), (uint8_t)(55 + n * 20), 255};
                    if (y % 4 == 0 || (y % 4 > 2 && x % 8 == 0) || (y % 4 <= 1 && x % 8 == 4))
                        p = {190, 180, 170, 255}; // mortar
                    break;
                case BLK_LAMP: {
                    float glow = 1.f - std::sqrt((x - 7.5f) * (x - 7.5f) + (y - 7.5f) * (y - 7.5f)) / 9.f;
                    p = {(uint8_t)(255), (uint8_t)(220 + glow * 30), (uint8_t)(120 + glow * 90), 255};
                    p = tint(p, 0.7f + glow * 0.5f);
                    break;
                }
                case BLK_METAL:
                    p = {(uint8_t)(180 + n * 25), (uint8_t)(185 + n * 20), (uint8_t)(195 + n * 15), 255};
                    if (y % 6 == 0) p = tint(p, 0.8f);
                    break;
                case BLK_GOLD:
                    p = {(uint8_t)(240 + n * 15), (uint8_t)(200 + n * 30), (uint8_t)(70 + n * 30), 255};
                    if (y % 5 == 0) p = tint(p, 0.82f);
                    break;
                case BLK_COAL_ORE:
                    p = {(uint8_t)(80 + n * 20), (uint8_t)(80 + n * 20), (uint8_t)(85 + n * 20), 255};
                    if ((x + y * 3) % 7 == 0) p = {40, 40, 42, 255};
                    break;
                case BLK_IRON_ORE:
                    p = {(uint8_t)(170 + n * 20), (uint8_t)(150 + n * 20), (uint8_t)(130 + n * 20), 255};
                    if ((x * 2 + y) % 6 == 0) p = {200, 180, 160, 255};
                    break;
                case BLK_DIAMOND_ORE:
                    p = {(uint8_t)(120 + n * 20), (uint8_t)(200 + n * 20), (uint8_t)(220 + n * 20), 255};
                    if ((x * 3 + y * 2) % 9 == 0) p = {200, 255, 255, 255};
                    break;
                case BLK_BEDROCK:
                    p = {(uint8_t)(60 + n * 25), (uint8_t)(60 + n * 25), (uint8_t)(65 + n * 25), 255};
                    break;
                case BLK_REDSTONE:
                    p = {(uint8_t)(190 + n * 40), (uint8_t)(40 + n * 20), (uint8_t)(40 + n * 20), 255};
                    if (n > 0.88f) p = tint(p, 0.7f);
                    break;
                default:
                    p = {(uint8_t)(128 + n * 30), (uint8_t)(128 + n * 30), (uint8_t)(128 + n * 30), 255};
                    break;
            }
            size_t idx = ((size_t)(oy + y) * ATLAS_SIZE + (ox + x)) * 4;
            atlas[idx] = p.r; atlas[idx + 1] = p.g; atlas[idx + 2] = p.b; atlas[idx + 3] = p.a;
        }
    }
}

} // namespace

std::vector<uint8_t> generateAtlas() {
    std::vector<uint8_t> atlas((size_t)ATLAS_SIZE * ATLAS_SIZE * 4, 0);
    int tiles = 0;
    for (uint8_t b = 1; b < BLK_COUNT; ++b) {
        for (int f = 0; f < 6; ++f) {
            if (tiles >= 256) break;
            drawTile(atlas, tiles, b, f);
            ++tiles;
        }
    }
    return atlas;
}

std::vector<uint8_t> generatePaletteTexture() {
    std::vector<uint8_t> tex((size_t)PALETTE_SIZE * 4, 0);
    for (int i = 0; i < PALETTE_SIZE; ++i) {
        tex[(size_t)i * 4 + 0] = palette[i].r;
        tex[(size_t)i * 4 + 1] = palette[i].g;
        tex[(size_t)i * 4 + 2] = palette[i].b;
        tex[(size_t)i * 4 + 3] = 255;
    }
    return tex;
}

} // namespace rb
