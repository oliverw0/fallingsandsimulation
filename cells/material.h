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
};

inline const MaterialProps& props(CellMaterial m) {
    static const MaterialProps table[] = {
        { BLACK,  false, false, false },  // Empty
        { YELLOW,   true,  false, true },  // Sand
        { BLUE, true,  true,  false },  // Water
        { WHITE,   false, false, true  },  // Stone
    };
    return table[static_cast<unsigned char>(m)];
}