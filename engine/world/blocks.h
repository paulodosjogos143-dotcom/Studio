// ROXBOX - block definitions
// A tiny block table (Roblox-style materials mapped to voxel blocks).
#pragma once
#include <cstdint>
#include <array>

namespace rb {

// block ids (uint8 per voxel -> 4 KB per 16^3 chunk)
enum Block : uint8_t {
    BLK_AIR = 0,
    BLK_GRASS = 1,
    BLK_DIRT = 2,
    BLK_STONE = 3,
    BLK_COBBLE = 4,
    BLK_WOOD = 5,
    BLK_PLANKS = 6,
    BLK_LEAVES = 7,
    BLK_SAND = 8,
    BLK_WATER = 9,
    BLK_GLASS = 10,
    BLK_BRICK = 11,
    BLK_LAMP = 12,
    BLK_METAL = 13,
    BLK_GOLD = 14,
    BLK_COAL_ORE = 15,
    BLK_IRON_ORE = 16,
    BLK_DIAMOND_ORE = 17,
    BLK_BEDROCK = 18,
    BLK_REDSTONE = 19,
    BLK_COUNT = 20,
};

// Roblox-style material -> block id
enum class Material : int {
    Plastic = 0, NeonPlastic, Wood, Metal, Neon, Brick, Glass, Stone, Sand,
    Leaves, Water, Gold, Diamond, Redstone, Lamp, Count
};

// global 24-entry RGB palette used for part tints (8-bit color, tiny memory)
struct RGB8 { uint8_t r, g, b; };
inline constexpr int PALETTE_SIZE = 24;
inline constexpr RGB8 palette[PALETTE_SIZE] = {
    {255,255,255}, {40,40,40},  {120,120,120}, {255,0,0},    {255,140,0}, {255,220,0},
    {60,200,60},   {40,90,255}, {160,40,220},  {240,90,180}, {130,80,40}, {0,180,180},
    {220,60,220},  {120,220,60}, {150,30,20},  {30,40,120},  {0,140,130}, {100,100,30},
    {200,200,200}, {255,210,80}, {80,255,80},  {80,140,255}, {255,80,80},  {180,80,255},
};

struct BlockDef {
    const char* name;
    uint8_t solid;       // collides
    uint8_t transparent; // rendered in transparent pass (glass/water)
    uint8_t emissive;    // self-lit (lamp)
    uint8_t wave;        // animated uv (water)
    uint8_t color[6][3]; // px,nx,py,ny,pz,nz
};

// px,nx,py,ny,pz,nz
inline const BlockDef blockDefs[BLK_COUNT] = {
    { "Air",     0,0,0,0, {{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}} },
    { "Grass",   1,0,0,0, {{90,140,60},{130,95,55},{95,165,70},{130,95,55},{90,140,60},{130,95,55}} },
    { "Dirt",    1,0,0,0, {{130,95,55},{130,95,55},{130,95,55},{130,95,55},{130,95,55},{130,95,55}} },
    { "Stone",   1,0,0,0, {{120,120,125},{120,120,125},{125,125,130},{115,115,120},{120,120,125},{120,120,125}} },
    { "Cobble",  1,0,0,0, {{95,95,100},{95,95,100},{100,100,105},{90,90,95},{95,95,100},{95,95,100}} },
    { "Wood",    1,0,0,0, {{110,80,40},{110,80,40},{150,110,60},{150,110,60},{110,80,40},{110,80,40}} },
    { "Planks",  1,0,0,0, {{150,110,60},{150,110,60},{160,120,70},{140,100,50},{150,110,60},{150,110,60}} },
    { "Leaves",  1,0,0,0, {{60,110,40},{60,110,40},{70,130,50},{60,110,40},{60,110,40},{60,110,40}} },
    { "Sand",    1,0,0,0, {{220,200,130},{220,200,130},{230,215,140},{215,195,125},{220,200,130},{220,200,130}} },
    { "Water",   0,1,0,1, {{50,90,200},{50,90,200},{60,110,210},{40,80,180},{50,90,200},{50,90,200}} },
    { "Glass",   0,1,0,0, {{200,220,240},{200,220,240},{210,230,245},{195,215,235},{200,220,240},{200,220,240}} },
    { "Brick",   1,0,0,0, {{150,70,55},{150,70,55},{160,75,60},{140,65,50},{150,70,55},{150,70,55}} },
    { "Lamp",    1,0,1,0, {{255,230,140},{255,230,140},{255,240,170},{255,220,120},{255,230,140},{255,230,140}} },
    { "Metal",   1,0,0,0, {{180,185,195},{180,185,195},{190,195,205},{170,175,185},{180,185,195},{180,185,195}} },
    { "Gold",    1,0,0,0, {{240,200,70},{240,200,70},{250,215,90},{235,190,60},{240,200,70},{240,200,70}} },
    { "CoalOre", 1,0,0,0, {{80,80,85},{80,80,85},{95,95,100},{75,75,80},{80,80,85},{80,80,85}} },
    { "IronOre", 1,0,0,0, {{170,150,130},{170,150,130},{185,165,145},{160,140,120},{170,150,130},{170,150,130}} },
    { "DiamondOre", 1,0,0,0, {{120,200,220},{120,200,220},{140,220,235},{110,190,210},{120,200,220},{120,200,220}} },
    { "Bedrock", 1,0,0,0, {{60,60,65},{60,60,65},{70,70,75},{55,55,60},{60,60,65},{60,60,65}} },
    { "Redstone",1,0,1,0, {{190,40,40},{190,40,40},{210,60,60},{180,30,30},{190,40,40},{190,40,40}} },
};

inline bool blockIsSolid(uint8_t b) { return blockDefs[b].solid != 0; }
inline bool blockIsTransparent(uint8_t b) { return blockDefs[b].transparent != 0; }
inline bool blockIsEmissive(uint8_t b) { return blockDefs[b].emissive != 0; }
inline bool blockIsLiquid(uint8_t b) { return b == BLK_WATER; }
inline bool blockHasWave(uint8_t b) { return blockDefs[b].wave != 0; }
inline const BlockDef& blockDef(uint8_t b) { return blockDefs[b]; }

// Roblox material -> block id mapping
inline uint8_t materialToBlock(Material m) {
    switch (m) {
        case Material::Plastic:     return BLK_PLANKS;
        case Material::NeonPlastic: return BLK_PLANKS;
        case Material::Wood:        return BLK_WOOD;
        case Material::Metal:       return BLK_METAL;
        case Material::Neon:        return BLK_LAMP;
        case Material::Brick:       return BLK_BRICK;
        case Material::Glass:       return BLK_GLASS;
        case Material::Stone:       return BLK_STONE;
        case Material::Sand:        return BLK_SAND;
        case Material::Leaves:      return BLK_LEAVES;
        case Material::Water:       return BLK_WATER;
        case Material::Gold:        return BLK_GOLD;
        case Material::Diamond:     return BLK_DIAMOND_ORE;
        case Material::Redstone:    return BLK_REDSTONE;
        case Material::Lamp:        return BLK_LAMP;
        case Material::Count: break;
    }
    return BLK_STONE;
}

// nearest palette index for a color (quantize for tiny per-voxel tint storage)
inline int paletteNearest(uint8_t r, uint8_t g, uint8_t b) {
    int best = 0;
    int bestD = 1 << 30;
    for (int i = 0; i < PALETTE_SIZE; ++i) {
        int dr = (int)r - (int)palette[i].r, dg = (int)g - (int)palette[i].g, db = (int)b - (int)palette[i].b;
        int d = dr * dr + dg * dg + db * db;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// map material block tint: return palette index for part color if >0 else 0
inline int blockTint(uint8_t block, int paletteIdx) {
    if (paletteIdx <= 0) return 0;
    return paletteIdx < PALETTE_SIZE ? paletteIdx : 0;
}

} // namespace rb
