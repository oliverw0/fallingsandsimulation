// material.h
#pragma once
#include <raylib.h>

struct Reaction {
    CellMaterial a;
    CellMaterial b;
    CellMaterial resultA;
    CellMaterial resultB;
    int chance; // 1 -> N chance. If N=1, 100%.
};

inline const Reaction reactions[] = {
    { CellMaterial::Water, CellMaterial::Lava, CellMaterial::Steam, CellMaterial::Stone, 1 },
    { CellMaterial::Lava, CellMaterial::Sand, CellMaterial::Steam, CellMaterial::Glass, 1 },
};

inline const Reaction* findReaction(CellMaterial a, CellMaterial b) {
    
    for (const auto& r : reactions){
        if ((r.a == a && r.b == b) || (r.a == b && r.b == a)) {
            return &r;
        }
    }
    return nullptr;
};