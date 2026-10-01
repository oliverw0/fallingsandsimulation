#pragma once
#include <vector>
#include <cstdint>
#include <raylib.h>
#include "cells/cell.h"
#include "cells/reactions.h"

// Explosions requested by the sim (gunpowder, kegs). The game applies entity damage.
struct Blast { int x, y, r, power; float dmg; };
// Cells flung by explosions; the game turns them into particles that land as cells again.
struct Debris { float x, y, vx, vy; Cell cell; };

struct World
{
    int w = 0, h = 0;
    std::vector<Cell> cells;
    uint8_t clock = 0;
    int frame = 0;
    std::vector<Blast> blasts;
    std::vector<Debris> debris;
    std::vector<Color> bg; // back wall / sky colour per cell

    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    Cell& at(int x, int y) { return cells[(size_t)y * w + x]; }
    CellMaterial mat(int x, int y) const { return in(x, y) ? cells[(size_t)y * w + x].material : CellMaterial::Bedrock; }
};

extern World world;

inline Color lerpColor(Color a, Color b, float t)
{
    return Color{
        (unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
inline Color brighten(Color c, int d)
{
    auto cl = [](int v) { return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    return Color{cl(c.r + d), cl(c.g + d), cl(c.b + d), c.a};
}

void worldInit(int w, int h);
void setCell(int x, int y, CellMaterial m);
void simulate(int x0, int y0, int x1, int y1);
void ignite(int x, int y);
void explodeCells(int cx, int cy, int r, int power);
void paintCircle(int cx, int cy, int r, CellMaterial m, bool onlyEmpty);
bool isSolid(int x, int y);       // blocks entities
bool isLiquidAt(int x, int y);
bool lineOfSight(float x0, float y0, float x1, float y1);
Color cellColor(const Cell& c, int x, int y);
void renderWorld(Color* px, int camX, int camY, int vw, int vh);
