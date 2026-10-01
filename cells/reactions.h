// reactions.h
#pragma once
#include "material.h"

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
    { CellMaterial::Water, CellMaterial::Sand, CellMaterial::Water, CellMaterial::WetSand, 60 * 10 },
    { CellMaterial::Lava, CellMaterial::Ice, CellMaterial::Lava, CellMaterial::Water, 6 },
    { CellMaterial::Lava, CellMaterial::Snow, CellMaterial::Lava, CellMaterial::Steam, 3 },
    { CellMaterial::Lava, CellMaterial::Blood, CellMaterial::Lava, CellMaterial::Smoke, 2 },
    { CellMaterial::Lava, CellMaterial::Bone, CellMaterial::Lava, CellMaterial::Smoke, 20 },
};

inline const Reaction* findReaction(CellMaterial a, CellMaterial b) {
    for (const auto& r : reactions) {
        if ((r.a == a && r.b == b) || (r.a == b && r.b == a)) {
            return &r;
        }
    }
    return nullptr;
}
