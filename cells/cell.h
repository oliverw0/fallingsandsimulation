#pragma once
#include <cstdint>
#include "material.h"

enum CellFlags : uint8_t {
    CF_CLOCK   = 1,  // which sim tick last touched this cell
    CF_LOOSE   = 2,  // ore knocked free: falls like powder and can be collected
    CF_BURNING = 4,
};

struct Cell {
    CellMaterial material = CellMaterial::Empty;
    uint8_t shade = 0;   // per-cell colour variation
    uint8_t flags = 0;
    uint8_t life = 0;    // fire / gas lifetime, burn timer
};
