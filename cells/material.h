// material.h
#pragma once
#include <raylib.h>

enum class CellMaterial : unsigned char {
    Empty = 0,
    Sand,
    Water,
    Stone,
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
        { BLUE, true,  true,  false, 1 },  // Water
        { WHITE,   false, false, true, 3 },  // Stone
    };
    return table[static_cast<unsigned char>(m)];
}