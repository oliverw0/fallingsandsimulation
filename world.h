#pragma once
#include <functional>
#include <vector>
#include <memory>
#include <cstdint>
#include <raylib.h>
#include "cells/cell.h"
#include "cells/reactions.h"

// Explosions requested by the sim (gunpowder, kegs). The game applies entity damage.
struct Blast { int x, y, r, power; float dmg; };
// Where terrain was broken (blasted, burnt through, dissolved, dug); the game checks what's left still holds up.
struct Disturbance { int x, y, r; };
// Cells flung by explosions; the game turns them into particles that land as cells again.
struct Debris { float x, y, vx, vy; Cell cell; };

// The world is stored in square chunks, allocated only once something is written there. A chunk that was
// never touched is solid `fill` (the plain rock between biomes), so the whole run can stay in memory -
// every biome you've passed through, foes, scars and all - without paying for the empty rock around it.
//
// Levels are laid out one cell per world unit, then upscaled: in play the grid has `scale` (2) cells per
// unit, so sand, water and rock are twice as fine as the units the game's things are measured in. The
// back wall stays at one colour per unit. Gameplay code talks in units (atU, matU, inU and the functions
// below the struct); the simulation and renderer work in cells.
const int CS = 64; // chunk side in cells, a power of two
// A back-wall colour's alpha doubles as a pattern, drawn per cell when rendering (world.cpp:wallStyle): the colour
// is the base tone, the pattern its fine grain - so tiled walls and boards are as fine as the terrain, not 2x2 blocks.
enum WallStyle : unsigned char { WALL_TILE = 250, WALL_PLANK_V, WALL_PLANK_H, WALL_COBBLE, WALL_BRICK };
struct Chunk
{
    Cell cells[CS * CS];
    std::unique_ptr<Color[]> bg;    // back wall / sky colour, one per (CS >> bgShift)^2
    std::unique_ptr<uint8_t[]> sky; // 1 = open night sky behind (the moon shows through)
    uint32_t hash = 0;              // what the chunk held after its last tick
    uint8_t awake = 4;              // ticks left before an unchanging chunk stops being simulated
};

struct World
{
    int w = 0, h = 0, cw = 0, ch = 0; // size in cells, and in chunks
    int scale = 1, bgShift = 0;       // cells per world unit; back-wall entries are 2^bgShift cells across
    std::vector<std::unique_ptr<Chunk>> chunks;
    Cell fill{};
    Color fillBg{12, 12, 16, 255};
    uint8_t clock = 0;
    int frame = 0;
    std::vector<Blast> blasts;
    std::vector<Debris> debris;
    std::vector<Disturbance> disturbed;
    int gen = 0; // bumped whenever the world is rebuilt
    float pushX = -1e9f, pushY = 0; // where something stands (cells) that the grass bends away from
    float storm = 0, flash = 0; // the sky: 0 clear .. 1 a black storm; lightning, fading

    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    Chunk* chunk(int x, int y) const { return chunks[(size_t)(y / CS) * cw + x / CS].get(); }
    static int idx(int x, int y) { return (y & (CS - 1)) * CS + (x & (CS - 1)); }
    int bidx(int x, int y) const { return ((y & (CS - 1)) >> bgShift) * (CS >> bgShift) + ((x & (CS - 1)) >> bgShift); }
    Chunk& touch(int x, int y) { Chunk* c = chunk(x, y); return c ? *c : alloc(x, y); }
    Chunk& alloc(int x, int y);
    // writable access allocates the chunk and wakes it; reads through get/bgOf/skyOf/mat never do
    Cell& at(int x, int y) { Chunk& c = touch(x, y); c.awake = 4; return c.cells[idx(x, y)]; }
    Cell& atq(int x, int y) { return touch(x, y).cells[idx(x, y)]; } // the simulation's own access: wakes nothing
    Color& bgAt(int x, int y) { return touch(x, y).bg[bidx(x, y)]; }
    uint8_t& skyAt(int x, int y) { return touch(x, y).sky[bidx(x, y)]; }
    const Cell& get(int x, int y) const { Chunk* c = chunk(x, y); return c ? c->cells[idx(x, y)] : fill; }
    Color bgOf(int x, int y) const { Chunk* c = chunk(x, y); return c ? c->bg[bidx(x, y)] : fillBg; }
    uint8_t skyOf(int x, int y) const { Chunk* c = chunk(x, y); return c ? c->sky[bidx(x, y)] : 0; }
    CellMaterial mat(int x, int y) const { return in(x, y) ? get(x, y).material : CellMaterial::Bedrock; }
    // in world units: the top-left cell of each unit
    bool inU(int x, int y) const { return in(x * scale, y * scale); }
    Cell& atU(int x, int y) { return at(x * scale, y * scale); }
    CellMaterial matU(int x, int y) const { return mat(x * scale, y * scale); }
    int wU() const { return w / scale; }
    int hU() const { return h / scale; }
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

// w, h in cells; `scale` cells per world unit
void worldInit(int w, int h, Cell fill = Cell{}, Color fillBg = Color{12, 12, 16, 255}, int scale = 1, int bgShift = -1); // bgShift -1: one per unit
inline void disturb(int x, int y, int r) { world.disturbed.push_back({x, y, r}); } // in cells
// in cells: the simulation over a rectangle, and drawing the grid
void simulate(int x0, int y0, int x1, int y1);
// Runs fn(0..n-1) across a pool of worker threads started once (a fresh std::thread costs ~0.1 ms on Windows,
// which several times a frame cost more than the work). The caller helps, and it returns when all are done.
void parallelFor(int n, const std::function<void(int)>& fn);
int workerCount(); // threads parallelFor spreads over, the caller included
Color cellColor(const Cell& c, int x, int y);
void renderWorld(Color* px, int camX, int camY, int vw, int vh, bool mask = false);
void parallaxPrep(int camX, int camY, int vw, int vh, int frame);   // parallax.cpp: the layered backdrop seen through the sky
int parallaxAt(int i, int j, Color& out, float& glow);
float parallaxHaze(int j);
// in world units: these act on every cell of the units they cover
void setCell(int x, int y, CellMaterial m);
void setCellC(int x, int y, CellMaterial m); // just the one cell (x, y in cells)
void ignite(int x, int y);
void explodeCells(int cx, int cy, int r, int power);
void paintCircle(int cx, int cy, int r, CellMaterial m, bool onlyEmpty);
bool isSolid(int x, int y);       // blocks entities (any cell of the unit)
bool isSolidC(int x, int y);      // the one cell (in cells)
bool isLiquidAt(int x, int y);
float gripAt(int x, int y);      // grip() of the unit's first solid cell, 1 if none
bool lineOfSight(float x0, float y0, float x1, float y1);
void materialSelfTest(); // --selftest: reactions and the material table's tags
