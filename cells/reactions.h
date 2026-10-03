// reactions.h
#pragma once
#include "material.h"

// Unordered pairs: a cell checks one random neighbour per tick (world.cpp:tryReact, via its pair lookup).
// A new reaction is just a row here.
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
