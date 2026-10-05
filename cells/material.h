// material.h
#pragma once
#include <raylib.h>
#include <cstdint>

enum class CellMaterial : uint8_t {
    Empty = 0,
    Bedrock,
    Sand,
    WetSand,
    Water,
    Stone,
    Lava,
    Steam,
    Glass,
    Dirt,
    Grass,
    Gravel,
    Wood,
    Leaves,
    Brick,
    Bone,
    Snow,
    Ice,
    Acid,
    Oil,
    Gunpowder,
    Fire,
    Smoke,
    Miasma,
    Blood,
    Coal,
    CopperOre,
    IronOre,
    GoldOre,
    Firestone,
    Frostite,
    Stormite,
    Venomite,
    Adamantite,
    Obsidian,
    Basalt,
    Moss,
    Keg,
    Metal,
    Spikes,
    Platform, // one-way planks: stand on them, jump up through them, S to drop through
    Thatch,   // bundled straw on Norse roofs: dry, and it goes up fast
    Masonry,  // grey dressed stone in mortared courses: castle and crypt walls
    Glowmoss, // the damp caves' faint light
    BloodEarth, // the battlefield before Dunmoor: ground soaked dark with old blood
    Sandstone,  // the desert east of Dunmoor: banded rock that blasts to sand
    Count
};

// Collectable resources. Ore cells map onto these.
enum Res { R_COPPER, R_IRON, R_COAL, R_GOLD, R_FIRESTONE, R_FROSTITE, R_STORMITE, R_VENOMITE, R_ADAMANTITE, RES_COUNT };

enum class Kind : uint8_t { Air, Solid, Powder, Liquid, Gas, Fire };

// MaterialProps::tags
enum MatTag : uint8_t {
    T_EXTINGUISH = 1, // puts out burning neighbours
    T_ACIDPROOF  = 2, // acid can't eat it
    T_GEM        = 4, // sparkles
};

struct MaterialProps {
    const char* name;
    Color a, b;      // each cell picks a colour between a and b from its shade byte
    Kind kind;
    int density;
    int hardness;    // what an explosion / weapon needs to break it, 255 = never
    int flammable;   // % chance to catch per exposure
    int burnTime;
    int ore;         // Res + 1, 0 = not an ore
    int dispersion;  // liquids: how far they spread sideways per tick
    // optional trailing columns: rows that leave them out get zero
    uint8_t tags = 0;                           // MatTag bits
    CellMaterial crumble = CellMaterial::Empty; // what a blast leaves of it when it isn't blown away
    Color glow = {0, 0, 0, 0};                  // light it gives off (rgb, before the lighting pass's gain)
};

inline const MaterialProps& props(CellMaterial m) {
    static const MaterialProps table[] = {
        { "Air",        {0,0,0,0},         {0,0,0,0},         Kind::Air,     0,   0,   0,   0, 0, 0 },
        { "Bedrock",    {22,20,26,255},    {46,42,52,255},    Kind::Solid, 100, 255,   0,   0, 0, 0, T_ACIDPROOF },
        { "Sand",       {196,164,96,255},  {236,208,138,255}, Kind::Powder, 20,   0,   0,   0, 0, 0 },
        { "Wet Sand",   {132,108,66,255},  {168,140,88,255},  Kind::Powder, 21,   0,   0,   0, 0, 0, T_EXTINGUISH },
        { "Water",      {30,84,180,190},   {56,120,214,205},  Kind::Liquid, 10,   0,   0,   0, 0, 5, T_EXTINGUISH },
        { "Stone",      {74,74,82,255},    {134,132,142,255}, Kind::Solid,  50,   3,   0,   0, 0, 0, 0, CellMaterial::Gravel },
        { "Lava",       {220,70,16,255},   {255,160,40,255},  Kind::Liquid, 15,   0,   0,   0, 0, 1, 0, CellMaterial::Empty, {255,107,31,255} },
        { "Steam",      {190,190,200,120}, {230,230,240,90},  Kind::Gas,    -2,   0,   0,   0, 0, 0 },
        { "Glass",      {150,195,212,150}, {200,235,245,185}, Kind::Solid,  50,   2,   0,   0, 0, 0, T_ACIDPROOF },
        { "Dirt",       {74,48,30,255},    {126,88,56,255},   Kind::Solid,  40,   1,   0,   0, 0, 0 },
        { "Grass",      {42,104,34,255},   {96,168,62,255},   Kind::Solid,  40,   1,   8,  30, 0, 0 },
        { "Gravel",     {88,84,80,255},    {150,144,136,255}, Kind::Powder, 22,   0,   0,   0, 0, 0 },
        { "Wood",       {88,56,32,255},    {146,104,60,255},  Kind::Solid,  40,   2,   5, 160, 0, 0 },
        { "Leaves",     {28,80,28,255},    {74,138,48,255},   Kind::Solid,  30,   1,  25,  25, 0, 0 },
        { "Brick",      {94,52,44,255},    {152,92,74,255},   Kind::Solid,  50,   4,   0,   0, 0, 0, 0, CellMaterial::Gravel },
        { "Bone",       {186,182,162,255}, {234,230,210,255}, Kind::Powder, 18,   0,   0,   0, 0, 0 },
        { "Snow",       {212,220,236,255}, {250,252,255,255}, Kind::Powder, 15,   0,   0,   0, 0, 0, T_EXTINGUISH },
        { "Ice",        {136,186,228,230}, {198,230,252,240}, Kind::Solid,  45,   2,   0,   0, 0, 0, 0, CellMaterial::Snow },
        { "Acid",       {96,216,36,220},   {164,255,96,230},  Kind::Liquid, 11,   0,   0,   0, 0, 3, T_ACIDPROOF, CellMaterial::Empty, {31,77,15,255} },
        { "Oil",        {36,28,24,235},    {74,60,44,235},    Kind::Liquid,  8,   0,  60,  50, 0, 3 },
        { "Gunpowder",  {38,38,42,255},    {86,84,88,255},    Kind::Powder, 19,   0, 100,   4, 0, 0 },
        { "Fire",       {255,200,60,255},  {255,90,20,255},   Kind::Fire,   -1,   0,   0,   0, 0, 0, 0, CellMaterial::Empty, {255,140,51,255} },
        { "Smoke",      {50,50,54,140},    {92,92,98,110},    Kind::Gas,    -1,   0,   0,   0, 0, 0 },
        { "Miasma",     {106,148,48,100},  {152,192,82,90},   Kind::Gas,    -3,   0,  90,   2, 0, 0 },
        { "Blood",      {116,8,14,235},    {172,22,28,235},   Kind::Liquid, 10,   0,   0,   0, 0, 2, T_EXTINGUISH },
        { "Coal",       {20,20,22,255},    {60,58,60,255},    Kind::Solid,  50,   3,   3, 240, R_COAL + 1, 0 },
        { "Copper Ore", {146,80,46,255},   {236,156,94,255},  Kind::Solid,  50,   3,   0,   0, R_COPPER + 1, 0 },
        { "Iron Ore",   {116,80,70,255},   {200,174,164,255}, Kind::Solid,  50,   4,   0,   0, R_IRON + 1, 0 },
        { "Gold Ore",   {176,136,28,255},  {255,228,104,255}, Kind::Solid,  50,   3,   0,   0, R_GOLD + 1, 0, T_GEM },
        { "Firestone",  {176,38,14,255},   {255,134,42,255},  Kind::Solid,  50,   5,   0,   0, R_FIRESTONE + 1, 0, T_GEM },
        { "Frostite",   {66,156,228,255},  {184,238,255,255}, Kind::Solid,  50,   5,   0,   0, R_FROSTITE + 1, 0, T_GEM },
        { "Stormite",   {106,66,206,255},  {204,174,255,255}, Kind::Solid,  50,   5,   0,   0, R_STORMITE + 1, 0, T_GEM },
        { "Venomite",   {36,146,52,255},   {124,238,112,255}, Kind::Solid,  50,   5,   0,   0, R_VENOMITE + 1, 0, T_GEM },
        { "Adamantite", {28,126,116,255},  {94,234,214,255},  Kind::Solid,  50,   6,   0,   0, R_ADAMANTITE + 1, 0, T_GEM },
        { "Obsidian",   {18,12,30,255},    {58,42,80,255},    Kind::Solid,  50,   6,   0,   0, 0, 0, 0, CellMaterial::Gravel },
        { "Basalt",     {40,36,38,255},    {82,74,76,255},    Kind::Solid,  50,   5,   0,   0, 0, 0, 0, CellMaterial::Gravel },
        { "Moss",       {30,76,34,255},    {74,132,60,255},   Kind::Solid,  40,   1,  10,  40, 0, 0 },
        { "Powder Keg", {106,60,26,255},   {162,104,54,255},  Kind::Solid,  40,   2, 100,   6, 0, 0 },
        { "Iron Plate", {78,84,94,255},    {132,140,150,255}, Kind::Solid,  60,   7,   0,   0, 0, 0, T_ACIDPROOF },
        { "Spikes",     {136,136,146,255}, {214,214,224,255}, Kind::Solid,  50,   4,   0,   0, 0, 0 },
        { "Platform",   {104,70,38,255},   {212,164,100,255},  Kind::Solid,  40,   2,   5, 120, 0, 0 },
        { "Thatch",     {122,96,48,255},   {204,170,96,255},  Kind::Solid,  30,   1,  40,  50, 0, 0 },
        { "Masonry",    {58,58,66,255},    {138,136,146,255}, Kind::Solid,  50,   4,   0,   0, 0, 0, 0, CellMaterial::Gravel },
        { "Glowmoss",   {34,96,84,255},    {96,196,160,255},  Kind::Solid,  40,   1,  10,  40, 0, 0, 0, CellMaterial::Empty, {40,130,110,255} },
        { "Bloodied Earth", {62,22,20,255}, {112,44,34,255},  Kind::Solid,  40,   1,   0,   0, 0, 0 },
        { "Sandstone",  {150,98,56,255},   {222,170,108,255}, Kind::Solid,  50,   3,   0,   0, 0, 0, 0, CellMaterial::Sand },
    };
    static_assert(sizeof(table) / sizeof(table[0]) == (int)CellMaterial::Count, "material table out of sync");
    return table[static_cast<unsigned char>(m)];
}

// How much a surface grips what slides on it: 1 = ordinary rock (rigid bodies' mu 0.55, corpses' Tune::FRICTION)
inline float grip(CellMaterial m) {
    switch (m) {
    case CellMaterial::Ice:      return 0.08f;
    case CellMaterial::Glass:    return 0.3f;
    case CellMaterial::Obsidian: return 0.45f;
    case CellMaterial::Metal:    return 0.5f;
    case CellMaterial::Moss: case CellMaterial::Glowmoss: return 0.7f;
    case CellMaterial::Sand: case CellMaterial::WetSand: case CellMaterial::Snow:
    case CellMaterial::Dirt: case CellMaterial::Grass: case CellMaterial::BloodEarth: return 1.4f;
    case CellMaterial::Gravel: return 1.6f;
    default: return 1.0f;
    }
}
