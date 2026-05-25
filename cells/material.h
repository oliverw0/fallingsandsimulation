// material.h
#pragma once
#include <raylib.h>

enum class CellMaterial : unsigned char {
    Empty = 0,
    Sand,
    WetSand,
    Water,
    Stone,
    Lava,
    Steam,
    Glass,
};

struct MaterialProps {
    Color color;
    bool falls;
    bool flows;
    bool blocks;
    int density;
};

inline const MaterialProps& props(CellMaterial m) {
    static const MaterialProps table[] = {
        { BLACK,  false, false, false, 0 },  // Empty
        { YELLOW,   true,  false, true, 2 },  // Sand
        { BEIGE,   true,  false, true, 2 },  // WetSand
        { BLUE, true,  true,  false, 1 },  // Water
        { GRAY,   false, false, true, 9 },  // Stone
        { ORANGE, true, true, false, 1 },   // Lava
        { LIGHTGRAY, true, false, false, -1 },   // Steam
        { WHITE, false, false, true, 9 },   // Glass
    };
    return table[static_cast<unsigned char>(m)];
}