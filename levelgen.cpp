#include <array>
#include "game.h"
#include "util.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <chrono>
#include <cstring>
#include "sprites_trees.h"

using M = CellMaterial;

#define O(m, w) {(int)M::m, w}

// name, subtitle, back wall colours, base/alt/alt2/top materials, ores, enemies, liquids, counts, hazards, boss, surface, drop metals
const StageDef STAGES[] = {
    {"The Greenmarch", "Rolling plains above, hungry caves below", {30, 36, 30, 255}, {52, 58, 44, 255},
     M::Dirt, M::Stone, M::Dirt, M::Grass,
     {O(CopperOre, 5), O(IronOre, 3), O(Coal, 3), O(GoldOre, 1)},
     {{E_WOLF, 4}, {E_GOBLIN, 3}, {E_REDCAP, 2}, {E_SLIME, 2}, {E_BAT, 2}, {E_BOMBER, 1}},
     M::Water, M::Oil, 12, 34,
     4, 1, 0, 3, 0, 0, 2, 0, -1, true, {M_COPPER, M_IRON, M_COPPER}, SK_PLAINS, 0},
    {"Castle Dunmoor", "Its lords are long dead. Its guards are not.", {12, 10, 16, 255}, {26, 20, 30, 255},
     M::Masonry, M::Stone, M::Masonry, M::Empty,
     {O(IronOre, 3), O(Coal, 2), O(GoldOre, 2)},
     {{E_GUARD, 5}, {E_KNIGHT, 2}, {E_CULTIST, 2}, {E_REDCAP, 2}, {E_ARCHER, 1}, {E_BAT, 1}},
     M::Water, M::Blood, 4, 26,
     3, 3, 1, 4, 6, 0, 0, 0, -1, false, {M_IRON, M_STEEL, M_IRON}, SK_CASTLE, -1},
    {"Forsaken Crypts", "The dead do not rest here", {28, 24, 30, 255}, {46, 38, 46, 255},
     M::Stone, M::Dirt, M::Basalt, M::Empty,
     {O(IronOre, 4), O(Coal, 3), O(GoldOre, 2), O(Venomite, 2), O(CopperOre, 2)},
     {{E_SKELETON, 4}, {E_ARCHER, 3}, {E_DRAUGR, 3}, {E_BANSHEE, 2}, {E_CULTIST, 2}, {E_BAT, 1}},
     M::Water, M::Blood, 8, 40,
     2, 1, 4, 6, 6, 0, 2, 2, -1, false, {M_IRON, M_STEEL, M_VENOMITE}, SK_CRYPT, -1},
    {"Deepdelve Mines", "Abandoned by the dwarves", {30, 28, 26, 255}, {50, 44, 38, 255},
     M::Stone, M::Basalt, M::Dirt, M::Empty,
     {O(CopperOre, 4), O(IronOre, 5), O(Coal, 6), O(GoldOre, 2), O(Stormite, 2)},
     {{E_GOBLIN, 3}, {E_BOMBER, 3}, {E_GOLEM, 2}, {E_TROLL, 2}, {E_BAT, 2}},
     M::Water, M::Oil, 10, 42,
     6, 6, 1, 3, 2, 0, 5, 6, E_BLACKKNIGHT, false, {M_IRON, M_STEEL, M_STORMITE}, SK_MINES, -1},
    {"Frostdeep Caverns", "Cold enough to stop a heart", {26, 34, 46, 255}, {44, 56, 72, 255},
     M::Stone, M::Ice, M::Stone, M::Snow,
     {O(IronOre, 3), O(Frostite, 5), O(GoldOre, 2), O(Stormite, 1), O(Coal, 2)},
     {{E_WRAITH, 3}, {E_DRAUGR, 3}, {E_KELPIE, 2}, {E_KNIGHT, 2}, {E_TROLL, 1}, {E_BAT, 1}},
     M::Water, M::Water, 14, 44,
     1, 1, 0, 6, 4, 0, 3, 0, -1, false, {M_STEEL, M_DAMASCUS, M_FROSTITE}, SK_CAVE, 1},
    {"The Infernal Forge", "Where the firestone is born", {40, 22, 18, 255}, {64, 34, 24, 255},
     M::Basalt, M::Obsidian, M::Stone, M::Empty,
     {O(Firestone, 5), O(Adamantite, 2), O(IronOre, 2), O(GoldOre, 2), O(Coal, 2)},
     {{E_IMP, 5}, {E_CULTIST, 3}, {E_KNIGHT, 2}, {E_GOLEM, 2}, {E_TROLL, 1}},
     M::Lava, M::Oil, 14, 46,
     2, 4, 3, 3, 2, 8, 2, 2, -1, false, {M_DAMASCUS, M_FIRESTONE, M_ADAMANTIUM}, SK_CAVE, 2},
    {"The Lich's Citadel", "End of all roads", {12, 10, 16, 255}, {26, 20, 30, 255},
     M::Obsidian, M::Basalt, M::Stone, M::Empty,
     {O(Adamantite, 4), O(Firestone, 1), O(Frostite, 1), O(Stormite, 1), O(Venomite, 1), O(GoldOre, 2)},
     {{E_SKELETON, 2}, {E_ARCHER, 2}, {E_CULTIST, 2}, {E_KNIGHT, 2}, {E_BANSHEE, 2}, {E_WRAITH, 1}},
     M::Acid, M::Lava, 10, 50,
     2, 3, 4, 6, 6, 4, 1, 2, E_LICH, false, {M_DAMASCUS, M_ADAMANTIUM, M_STORMITE}, SK_CRYPT, -1},
};
const int STAGE_COUNT = (int)(sizeof(STAGES) / sizeof(STAGES[0]));

static int W, H, seed;
// The deep biomes overlap like wide steps: each reaches WING units left under the one before it (wingL) and WING
// units right over the one after it (wingR). The haven sits at W - wingR; the entry at wingL.
static const int WING = 600;
static int wingL = 0, wingR = 0;
static std::vector<uint8_t> air;
static std::vector<Vector2> path;
static std::vector<int> surf;
static std::vector<int> pathFloor; // first solid row under the main road, per column
static std::vector<int> worldRoad; // the road's floor per column of the live world, -1 where there's none

struct Spot { int x, y; }; // y = first solid row under the feet

// Smooth, low-frequency noise sampled every `st` cells and interpolated: looks the same as sampling
// every cell, at a fraction of the cost (generation runs while you stand in a haven).
struct NoiseGrid
{
    int gw = 0, st = 4;
    std::vector<float> v;
    template <class F> NoiseGrid(int w, int h, int step, F f) : gw(w / step + 2), st(step), v((size_t)gw * (h / step + 2))
    {
        for (int j = 0; j < h / step + 2; j++)
            for (int i = 0; i < gw; i++) v[(size_t)j * gw + i] = f((float)(i * step), (float)(j * step));
    }
    float at(int x, int y) const
    {
        int i = x / st, j = y / st;
        float fx = (float)(x % st) / st, fy = (float)(y % st) / st;
        const float* r0 = &v[(size_t)j * gw + i];
        const float* r1 = r0 + gw;
        return (r0[0] * (1 - fx) + r0[1] * fx) * (1 - fy) + (r1[0] * (1 - fx) + r1[1] * fx) * fy;
    }
};

static bool isRock(int x, int y)
{
    if (!world.in(x, y)) return false;
    M m = world.at(x, y).material;
    return props(m).kind == Kind::Solid && m != M::Bedrock && m != M::Metal;
}

// Bricks get a mortar pattern, everything else a noisy strata shade.
static void place(int x, int y, M m)
{
    if (!world.in(x, y)) return;
    Cell& c = world.at(x, y);
    c = Cell{};
    c.material = m;
    if (m == M::Brick)
    {
        bool mortar = (y % 4 == 0) || ((x + ((y / 4) % 2) * 4) % 8 == 0);
        c.shade = mortar ? (uint8_t)irange(0, 25) : (uint8_t)irange(110, 230);
    }
    else if (m == M::Masonry) // big uneven blocks, each its own tone, in staggered courses
    {
        int row = y / 5, col = (x + (row % 2) * 6) / 12;
        bool mortar = y % 5 == 0 || (x + (row % 2) * 6) % 12 == 0;
        c.shade = mortar ? (uint8_t)irange(0, 20) : (uint8_t)(70 + hash2(col, row, seed + 9) * 120 + irand(40) + ((y % 5 == 1) ? 25 : 0));
    }
    else if (m == M::Platform)
        c.shade = (x % 7 == 0) ? 10 : (uint8_t)(140 + (y % 2) * 60 + irand(40));
    else if (m == M::Dirt || m == M::Moss || m == M::Grass)
        c.shade = (uint8_t)(fbm(x * 0.2f, y * 0.2f, seed + 21, 2) * 128 + irand(128));
    else
    {
        float s = fbm(x * 0.09f, y * 0.25f, seed + 11, 3);
        c.shade = (uint8_t)(clampf((s - 0.25f) * 2.0f, 0, 1) * 200 + irand(56));
    }
}

static void carve(float cx, float cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy > r * r) continue;
            int x = (int)cx + dx, y = (int)cy + dy;
            if (x >= 0 && y >= 0 && x < W && y < H) air[(size_t)y * W + x] = 1;
        }
}

static void clearRect(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (world.in(x, y) && world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
}

static bool findFloor(int x, int y0, int& fy)
{
    if (!world.in(x, y0) || isSolid(x, y0)) return false;
    int y = y0;
    while (y < H - 2 && !isSolid(x, y + 1)) y++;
    if (y >= H - 6) return false;
    fy = y + 1;
    return true;
}

static std::vector<Spot> findSpots(int n, int minX, int maxX, int w, int h)
{
    std::vector<Spot> out;
    for (int i = 0; i < n * 40 && (int)out.size() < n; i++)
    {
        int x = irange(minX, maxX), y = irange(8, H - 8), fy;
        if (!findFloor(x, y, fy)) continue;
        if (boxSolid(x - w / 2.0f, (float)(fy - h - 3), w, h)) continue; // a little slope is fine
        if (isLiquidAt(x, fy - 1) || world.mat(x, fy) == M::Spikes) continue;
        out.push_back({x, fy});
    }
    return out;
}

static int pickWeighted(const Weighted* list, int n)
{
    int total = 0;
    for (int i = 0; i < n; i++) total += list[i].w;
    int r = irand(total);
    for (int i = 0; i < n; i++)
    {
        if (r < list[i].w) return list[i].id;
        r -= list[i].w;
    }
    return list[0].id;
}

// ---------------------------------------------------------------- structures

static void placeCrate(int x, int fy)
{
    int w = irange(10, 14), h = irange(7, 9);
    for (int y = fy - h; y < fy; y++)
        for (int xx = x - w / 2; xx <= x + w / 2; xx++)
        {
            bool edge = y == fy - h || y == fy - 1 || xx == x - w / 2 || xx == x + w / 2;
            place(xx, y, edge ? M::Wood : M::Gunpowder);
        }
    for (int xx = x - w / 2; xx <= x + w / 2; xx++)
        if (!isSolid(xx, fy)) place(xx, fy, M::Wood);
}

static void placeKegs(int x, int fy)
{
    int n = irange(1, 3);
    for (int k = 0; k < n; k++)
        for (int y = fy - 6; y < fy; y++)
            for (int xx = 0; xx < 5; xx++)
            {
                int px = x + k * 6 + xx;
                if (!world.in(px, y)) continue;
                place(px, y, M::Keg);
                if (y == fy - 5 || y == fy - 2) world.at(px, y).shade = 0; // iron bands
            }
}

static void placeVat(int x, int fy)
{
    int w = irange(16, 24), depth = irange(9, 13);
    for (int y = fy - 1; y <= fy + depth; y++)
        for (int xx = x - w / 2 - 1; xx <= x + w / 2 + 1; xx++)
        {
            bool wall = xx == x - w / 2 - 1 || xx == x + w / 2 + 1 || y == fy + depth;
            if (wall) place(xx, y, y == fy - 1 ? M::Metal : M::Glass);
            else if (y >= fy + 2) place(xx, y, M::Acid);
            else if (world.in(xx, y)) world.at(xx, y) = Cell{};
        }
}

static void placeSpikePit(int x, int fy)
{
    int w = irange(10, 18);
    for (int xx = x - w / 2; xx <= x + w / 2; xx++)
    {
        for (int y = fy; y <= fy + 6; y++)
            if (world.in(xx, y) && world.at(xx, y).material != M::Bedrock) world.at(xx, y) = Cell{};
        if (xx % 3 == 0) place(xx, fy + 4, M::Spikes);
        if (xx % 3 != 2) place(xx, fy + 5, M::Spikes);
        place(xx, fy + 6, M::Spikes);
        if (!isSolid(xx, fy + 7)) place(xx, fy + 7, M::Stone);
    }
}

static void placeArrowTrap(int x, int fy)
{
    int ty = fy - 10;
    int sd = irand(2) ? 1 : -1;
    for (int pass = 0; pass < 2; pass++, sd = -sd)
    {
        for (int k = 4; k < 90; k++)
        {
            int wx = x + sd * k;
            if (!world.in(wx, ty)) break;
            if (!isSolid(wx, ty)) continue;
            for (int yy = ty - 6; yy <= ty + 6; yy++) // the slab the head is set in (drawn over by its sprite)
                for (int xx = 0; xx < 3; xx++)
                    if (isSolid(wx + sd * xx, yy) || yy == ty) place(wx + sd * xx, yy, M::Masonry);
            Trap t;
            t.type = TR_ARROW;
            t.x = wx; t.y = ty; t.dir = -sd;
            G.traps.push_back(t);
            return;
        }
    }
}

static void placeFlameVent(int x, int fy)
{
    for (int xx = x - 1; xx <= x + 1; xx++) place(xx, fy, M::Metal);
    Trap t;
    t.type = TR_FLAME;
    t.x = x; t.y = fy;
    G.traps.push_back(t);
}

static void placeCollapse(int x, int fy)
{
    int cy = fy - 2;
    while (cy > 4 && !isSolid(x, cy) && fy - cy < 60) cy--;
    if (fy - cy >= 60 || cy <= 4) return;
    Trap t;
    t.type = TR_COLLAPSE;
    t.rx0 = x - 16; t.rx1 = x + 16; t.ry0 = cy - 12; t.ry1 = cy;
    for (int xx = t.rx0; xx <= t.rx1; xx++)
        if (isRock(xx, cy)) place(xx, cy, M::Wood); // rotten beams give it away
    G.traps.push_back(t);
}

static void addChest(int x, int fy) { G.inter.push_back({IT_CHEST, (float)x, (float)fy, false, G.stage + 1}); G.inter.back().rest = 0; } // data: the tier it was left at, + 1

static void placeCryptRoom(int x, int fy)
{
    int w = irange(80, 120), h = irange(46, 62);
    int x0 = x - w / 2, x1 = x + w / 2, y0 = fy - h, y1 = fy;
    for (int y = y0; y <= y1 + 1; y++)
        for (int xx = x0; xx <= x1; xx++)
        {
            if (!world.in(xx, y) || world.at(xx, y).material == M::Bedrock) continue;
            bool wall = y <= y0 + 1 || y >= y1 || xx <= x0 + 1 || xx >= x1 - 1;
            bool door = (xx <= x0 + 1 || xx >= x1 - 1) && y > y1 - 40 && y < y1;
            if (wall && !door) place(xx, y, M::Masonry);
            else world.at(xx, y) = Cell{};
        }
    // coffins full of bones
    for (int k = 0; k < 2; k++)
    {
        int cx = x0 + 5 + k * (w - 16);
        for (int y = y1 - 4; y < y1; y++)
            for (int xx = cx; xx < cx + 10; xx++)
                place(xx, y, (y == y1 - 4 || xx == cx || xx == cx + 9) ? M::Wood : M::Bone);
    }
    addChest(x, y1);
}

// ---- background decorations: painted onto the back wall, so they never block movement

static Color shadeC(Color c, float k) { return {(unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), 255}; }

// Everything painted on the back wall is toned down a little, so it reads as behind the playfield.
static void bgPut(int x, int y, Color c)
{
    if (!world.in(x, y)) return;
    world.bgAt(x, y) = shadeC(c, 0.8f);
    world.skyAt(x, y) = 0;
}

// A back-wall cell painted in a pattern (WallStyle, world.h): `c` is the base tone, the pattern its cell-fine grain,
// worked out as the frame is drawn - so boards, tiles and stones are as fine as the terrain in front of them.
static void bgStyle(int x, int y, Color c, int style)
{
    if (!world.in(x, y)) return;
    Color& b = world.bgAt(x, y);
    b = shadeC(c, 0.8f);
    b.a = (unsigned char)style;
    world.skyAt(x, y) = 0;
}

// A piece of furniture or a hanging (decor.cpp), at the terrain's grain: see DecorKind in game.h for what x, y and size mean.
static void addDecor(int kind, float x, float y, int var = -1, int size = 0, bool flip = false)
{
    Interact d{IT_DECOR, x, y, false, kind, ((var < 0 ? irand(64) : var) & 63) | (flip ? 64 : 0)};
    d.w = size;
    G.inter.push_back(d);
}

// A skeleton sprawled on the floor, painted behind (dir flips which end the skull is).
static void paintSkeleton(int x, int fy, int dir)
{
    Interact d{IT_DECOR, (float)x + dir * 11, (float)fy, false, DK_SKELETON, irand(64) | (dir < 0 ? 64 : 0)};
    G.inter.push_back(d);
}

// Cobweb strung across a ceiling corner: spokes fanning down and towards `dir`, joined by threads.
static void paintCobweb(int x, int y, int dir) { addDecor(DK_COBWEB, (float)x, (float)y, -1, irange(7, 12), dir < 0); }

// ---- trees: the user's own painted tree sheet (sprites_trees.h, from tools/trees_user.py), stamped into the back wall a
// unit at a time (the renderer smooths them to cell diagonals). Five living kinds (small oak, birch, great oak, spruce,
// round oak) and six dead ones (bare oak, bare birch, broken hollow trunk, stump, fallen log, blackened snag).
static void treePx(int x, int y, Color c)
{
    if (!world.in(x, y)) return;
    world.bgAt(x, y) = c;
    world.skyAt(x, y) = 0;
}

static void placeTree(int x, int fy, int deadPct = 0)
{
    static const int LIVING[] = {0, 0, 0, 1, 1, 2, 3, 3, 4, 4}, DEAD[] = {5, 6, 7, 8, 9, 10};
    const TreeSprite& s = TREE_SPR[irand(100) < deadPct ? DEAD[irand(6)] : LIVING[irand(10)]];
    bool flip = chance(2);
    for (int j = 0; j < s.h; j++)
        for (int i = 0; i < s.w; i++)
        {
            char ch = s.px[j * s.w + i];
            if (ch == '.') continue;
            const unsigned char* c = TREE_PAL[std::strchr(TREE_ALPHABET, ch) - TREE_ALPHABET];
            treePx(x + (flip ? s.w - 1 - i : i) - s.w / 2, fy + 1 - s.h + j, {c[0], c[1], c[2], 255});
        }
}

static const Color LAMP_WARM = {255, 168, 84, 255};

// An oil lantern on a chain from (x, top): it swings, snaps off, and breaks into burning oil (entities.cpp).
static void hangLantern(int x, int top, int len)
{
    if (irand(5) < 2) return; // fewer of them, and on short chains, so nothing swings into a wall and burns the house down
    Interact it{IT_LANTERN, (float)x, (float)top};
    it.data = std::max(3, std::min(len, 3) + 1);
    G.inter.push_back(it);
}

// An iron bracket on the wall holding a burning torch (the flame is drawn live).
static void addSconce(int x, int y, int dir)
{
    for (int k = 0; k < 4; k++) bgPut(x - dir * k, y + 2 + k / 2, {56, 56, 62, 255});
    for (int yy = y; yy < y + 4; yy++) bgPut(x, yy, {92, 60, 34, 255});
    G.lamps.push_back({(float)x, (float)y - 2, 78, LAMP_WARM, true});
}

static void placeMineSupport(int x, int fy)
{
    int cy = fy - 2;
    while (cy > 4 && !isSolid(x, cy)) cy--;
    if (fy - cy > 70 || fy - cy < 24) return;
    Color wood = {110, 76, 44, 255};
    for (int y = cy; y < fy; y++)
        for (int k = -15; k <= -12; k++) { bgPut(x + k, y, shadeC(wood, k == -15 ? 0.6f : 0.8f)); bgPut(x - k, y, shadeC(wood, k == -12 ? 0.6f : 0.8f)); }
    for (int xx = x - 17; xx <= x + 17; xx++)
        for (int y = cy + 1; y <= cy + 4; y++) bgPut(xx, y, shadeC(wood, y == cy + 4 ? 0.55f : 0.85f));
    hangLantern(x, cy + 5, 5);
}

static void placePillar(int x, int fy)
{
    int cy = fy - 2;
    while (cy > 4 && !isSolid(x, cy)) cy--;
    if (fy - cy < 24) return;
    Color st = {96, 90, 104, 255};
    for (int y = cy; y < fy; y++)
        for (int xx = -5; xx <= 5; xx++)
        {
            float k = 0.7f + 0.03f * (5 - std::abs(xx - 2)) + 0.1f * hash2(x + xx, y / 4, seed + 31);
            if (y % 12 == 0) k *= 0.75f;
            bgPut(x + xx, y, shadeC(st, k));
        }
    for (int xx = -7; xx <= 7; xx++) { bgPut(x + xx, fy - 1, shadeC(st, 0.6f)); bgPut(x + xx, fy - 2, shadeC(st, 0.8f)); bgPut(x + xx, cy + 1, shadeC(st, 0.8f)); }
    if (chance(2)) // banner
    {
        Color cloth = chance(2) ? Color{120, 24, 30, 255} : Color{40, 46, 110, 255};
        for (int y = cy + 6; y < cy + 6 + std::min(30, (fy - cy) / 2); y++)
            for (int xx = -4; xx <= 4; xx++)
            {
                bool trim = std::abs(xx) == 4;
                bgPut(x + xx, y, trim ? Color{170, 140, 60, 255} : shadeC(cloth, 0.8f + 0.2f * (xx < 0)));
            }
    }
}

static void placeHanging(int x, int cy, int kind)
{
    // kind 0 roots, 1 icicles, 2 chains
    int len = irange(6, 26);
    for (int k = 0; k < len; k++)
    {
        int y = cy + k;
        if (y >= H || world.at(x, y).material != M::Empty) break;
        Color c = kind == 0 ? Color{74, 52, 34, 255} : (kind == 1 ? Color{150, 200, 235, 255} : Color{70, 70, 78, 255});
        if (kind == 1 && k > len - (len / 3)) { if (k % 2) continue; }
        if (kind == 2 && k % 3 == 1) c = shadeC(c, 1.4f);
        bgPut(x + (kind == 0 ? (int)(std::sin(k * 0.4f + x) * 1.2f) : 0), y, c);
        if (kind == 1 && k < len / 2) bgPut(x + 1, y, shadeC(c, 0.8f));
    }
}

// ---------------------------------------------------------------- back wall

static int castleGround = 0; // the castle grounds' level: the lower halls become crypts

static void buildBackground(const StageDef& d, bool skies)
{
    std::vector<int> localSurf(W, H); // mountains sit behind the nearby ground, not the highest hill in the level
    if (skies)
        for (int x = 0; x < W; x++)
            for (int k = std::max(0, x - 160); k < std::min(W, x + 160); k += 4) localSurf[x] = std::min(localSurf[x], d.kind == SK_CASTLE ? std::max(surf[k], castleGround) : surf[k]); // the hills sit behind the ground, not the castle walls
    NoiseGrid wallNoise(W, H, 4, [&](float x, float y) { return fbm(x * 0.02f, y * 0.03f, seed + 50, 3); });
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            float n = wallNoise.at(x, y);
            float t = fbm(x * 0.16f, y * 0.16f, seed + 51, 2);
            Color c = lerpColor(d.bgA, d.bgB, clampf((n - 0.3f) * 2.5f, 0, 1));
            c = brighten(c, (int)((t - 0.5f) * 26));
            if (skies && y < surf[x] + 6)
            {
                int ls = localSurf[x];
                float k = clampf((float)y / (ls + 10), 0, 1);
                Color sky = lerpColor(Color{1, 1, 4, 255}, Color{10, 10, 24, 255}, std::pow(k, 9.0f)); // night; the violet horizon haze, hills, castles and forest are parallax layers (parallax.cpp)
                bool open = true;
                if (y > surf[x] - 2) { sky = lerpColor(sky, c, (y - surf[x] + 2) / 8.0f); open = false; }
                world.skyAt(x, y) = open;
                c = sky;
            }
            else
                c = Color{(unsigned char)(c.r * 0.62f), (unsigned char)(c.g * 0.62f), (unsigned char)(c.b * 0.62f), 255};
            world.bgAt(x, y) = c;
        }
}

// ---------------------------------------------------------------- stages

static void resetLevelState()
{
    G.mobs.clear();
    G.projs.clear();
    G.pickups.clear();
    G.parts.clear();
    G.traps.clear();
    G.inter.clear();
    G.lamps.clear();
    G.duneEnd = 0;
    G.seaEnd = 0;
    G.desert = {};
    G.stormX0 = G.stormX1 = 0;
    world.storm = world.flash = 0;
    G.roamX0 = G.roamX1 = 0;
    G.underwater = false;
    G.sailT = 0;
    G.havens.clear();
    G.playX0 = 0;
    worldRoad.clear();
    G.duneCrossed = false;
    G.texts.clear();
    G.rags.clear();
    clearCorpses();
    G.msgs.clear();
    G.stoneLoot.clear();
    G.winTimer = 0;
    G.nearInteract = -1;
    G.p.hook = 0;
    G.p.mantleT = 0;
    G.p.m.vx = G.p.m.vy = 0;
}

static void placePlayer(int x, int fy)
{
    G.p.m.x = x - G.p.m.w / 2.0f;
    G.p.m.y = (float)(fy - G.p.m.h);
    G.camX = G.p.m.x - G.vw / 2.0f;
    G.camY = G.p.m.y - G.vh / 2.0f;
    syncRenderCamera();
}


// ---------------------------------------------------------------- the road to Dunmoor (plains fortifications)

static std::vector<Vector2> lookouts; // tower tops where archers stand
static std::vector<std::pair<int, int>> fortZones; // x ranges cave ramps must not tunnel into
struct Cellar { int x0, x1, floor; };
static std::vector<Cellar> cellars;
static std::vector<std::pair<int, int>> doorYards; // x ranges kept clear outside house doors (no trees)
static bool inFortZone(int x)
{
    for (auto& z : fortZones)
        if (x >= z.first && x <= z.second) return true;
    return false;
}

static void levelGround(int x0, int x1, int g)
{
    fortZones.push_back({x0 - 12, x1 + 12});
    for (int x = std::max(5, x0); x <= std::min(W - 6, x1); x++)
    {
        for (int y = 5; y < g; y++)
            if (world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
        for (int y = g; y < g + 8; y++) place(x, y, y < g + 2 ? M::Grass : M::Dirt);
        surf[x] = g;
    }
}

// Crates (or barrels) stacked into a doorway: real wood cells, tracked so any weapon, bolt or blast can smash them.
static void placeObstacle(int x0, int fy, int w, int h)
{
    bool barrel = chance(3);
    Interact it{IT_CRATE, (float)x0, (float)fy};
    it.w = w; it.h = h; it.data = h > 12 ? 3 : 2;
    for (int y = fy - h; y < fy; y++)
        for (int x = x0; x < x0 + w; x++)
        {
            int lx = x - x0, by = (fy - 1 - y) % 11; // each box in the stack is 11 tall
            bool edge = lx == 0 || lx == w - 1 || by == 0 || by == 10;
            if (barrel && (lx == 0 || lx == w - 1) && (by == 0 || by == 10)) { world.at(x, y) = Cell{}; continue; } // rounded rims
            place(x, y, M::Wood);
            uint8_t sh;
            if (barrel) sh = (by == 2 || by == 8) ? 0 : (lx % 3 == 0 ? 70 : (uint8_t)irange(150, 210)); // iron hoops, staves
            else if (edge) sh = 15;
            else if (std::abs(by * (w - 1) - lx * 10) < w) sh = 60; // diagonal brace
            else sh = by % 4 == 0 ? 95 : (uint8_t)irange(160, 230);   // planks
            world.at(x, y).shade = sh;
            it.cells++;
        }
    G.inter.push_back(it);
}

// A plank door filling an end doorway of a house (3 cells thick, 24 high): closed until the player opens it with F
// (entities.cpp:updateInteract: it swings open and the way is clear), or smashes it with a blow (an IT_CRATE with style 1 = hinged
// on the left edge, 2 = on the right).
static void placeDoor(int x0, int fy, int hingeSide)
{
    Interact it{IT_CRATE, (float)x0, (float)fy};
    it.w = 3; it.h = 24; it.data = 3; it.style = hingeSide < 0 ? 1 : 2;
    for (int y = fy - 24; y < fy; y++)
        for (int x = x0; x < x0 + 3; x++)
        {
            place(x, y, M::Wood);
            world.at(x, y).shade = (uint8_t)(((fy - y) % 17 == 5 || (fy - y) % 17 == 6) ? 25 : irange(120, 190)); // iron-strapped planks
            it.cells++;
        }
    G.inter.push_back(it);
}

// A wall of sharpened logs with a gateway through its foot.
static void placePalisade(int x)
{
    int g = surf[x + 4];
    levelGround(x - 4, x + 12, g);
    Color logC = {120, 80, 46, 255};
    for (int bx = x - 70; bx < x + 80; bx++) // the rest of the fence line, stretching away behind
        for (int y = g - 32 + ((bx / 3) % 2) * 2; y < g; y++)
            if (world.at(bx, y).material == M::Empty) bgPut(bx, y, shadeC(logC, (bx % 3 == 0) ? 0.45f : 0.6f));
    for (int k = 0; k < 8; k++)
        for (int y = g - 44 + (k % 2) * 3; y < g + 3; y++)
        {
            if (y > g - 27 && y < g) continue; // the gateway
            place(x + k, y, M::Wood);
        }
    if (chance(2)) placeObstacle(x, g, 8, 22); // barricaded
    for (int y = g - 26; y < g; y++) // gate doors swung open, painted behind
        for (int k = 0; k < 6; k++) { bgPut(x - 6 + k, y, shadeC(logC, 0.7f)); bgPut(x + 8 + k, y, shadeC(logC, 0.7f)); }
    G.inter.push_back({IT_TORCH, (float)x - 4, (float)g});
    G.inter.push_back({IT_TORCH, (float)x + 12, (float)g});
}

// ---------------------------------------------------------------- Norse halls
// Old weathered boards, carved posts, painted shields, and a steep roof of thatch or shingles whose gable
// boards cross over the ridge into carved heads. Farmsteads build theirs solid; Hearthwick's are scenery.

static const Color OLDWOOD = {96, 74, 54, 255}, GREYWOOD = {112, 104, 92, 255}, SEAM = {32, 24, 20, 255}, HEART = {156, 116, 72, 255};

// Upright boards with dark seams and the odd knot.
static void paintBoards(int x0, int y0, int x1, int y1, int bw)
{
    (void)bw; // (the boards, their seams, grain and knots are the wall's pattern now: see WALL_PLANK_V)
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            bgStyle(x, y, shadeC(lerpColor(OLDWOOD, GREYWOOD, 0.4f), 0.85f + 0.3f * hash2(x / 11, y / 17, seed + 6)), WALL_PLANK_V);
}

// A carved post: a block capital, a zig-zag engraved down its face, a plinth (decor.cpp).
static std::vector<std::array<int, 3>> postList; // every post and beam painted so far (x, top, foot), so windows keep clear of them
static void paintPost(int x, int y0, int y1) { postList.push_back({x + 1, y0, y1}); addDecor(DK_POST, x + 1.5f, (float)y1, 0, y1 - y0); }

// A round shield hung on the wall: iron rim, painted halves or quarters or a cross, a boss in the middle.
static void paintShield(int cx, int cy, int r)
{
    (void)r;
    addDecor(DK_SHIELD, (float)cx, (float)cy, irand(12), 0, chance(2));
}

struct Hall { int mid, apexY, roofBase; };

// The shared part of every hall: back wall, plinth, posts, shields, and the A-frame roof with its gable,
// ridge spikes and crossed horn-headed boards. `solid` builds the roof as real thatch / shingle cells.
static Hall paintHall(int x, int w, int g, int wallTop, bool solid, int doorTop = 0)
{
    Hall h;
    bool thatch = chance(2);
    int eave = irange(6, 10), T = 7;
    float slope = g - wallTop > 45 ? frange(0.75f, 0.95f) : frange(1.0f, 1.25f); // tall halls get a gentler pitch
    h.mid = x + w / 2;
    h.roofBase = wallTop - 2;
    h.apexY = h.roofBase - (int)((w / 2 + eave) * slope);
    paintBoards(x, wallTop, x + w + 1, g, irange(4, 5));
    for (int y = g - 5; y < g; y++) // fieldstone plinth
        for (int xx = x; xx <= x + w; xx++) bgStyle(xx, y, shadeC({112, 108, 104, 255}, 0.75f + 0.25f * hash2(xx / 9, y, seed)), WALL_COBBLE);
    int bays = std::max(1, w / 26);
    for (int b = 0; b <= bays; b++) paintPost(x + (w - 3) * b / bays, wallTop, doorTop && (b == 0 || b == bays) ? doorTop : g - 5); // (the end posts stop at the top of the door frame)
    for (int b = 0; b < bays; b++)
        if (chance(2)) paintShield(x + (w - 3) * (2 * b + 1) / (2 * bays) + 1, wallTop + 7, 4);
    auto roofCell = [&](int xx, int y, int shade) {
        M m = thatch ? M::Thatch : M::Wood;
        if (solid) { place(xx, y, m); world.at(xx, y).shade = (uint8_t)shade; }
        else bgPut(xx, y, lerpColor(props(m).a, props(m).b, shade / 255.0f));
    };
    for (int xx = x - eave; xx <= x + w + eave; xx++)
    {
        float d = std::fabs(xx + 0.5f - (h.mid + 0.5f));
        int top = h.apexY + (int)(d * slope);
        for (int y = top; y < top + T; y++)
        {
            int along = y - top; // depth into the roof
            if (along == T - 1) { roofCell(xx, y, 10); continue; } // shadowed underside
            int shade = thatch ? (int)(90 + 130 * hash2(xx, (y - (int)(d * slope)) / 2 + xx / 4, seed + 5)) // straw runs down the slope
                               : (((along + (xx / 3) % 2 * 2) % 4 == 0) ? 30 : 120 + irand(90)); // overlapping shingles
            roofCell(xx, y, std::min(255, shade));
        }
        if (thatch && (xx <= x - eave + 1 || xx >= x + w + eave - 1 || chance(3))) roofCell(xx, top + T, 60 + irand(60)); // ragged eaves
        if ((xx - x) % 5 == 0 && top < h.roofBase - 8) // carved spikes along the ridge
            addDecor(DK_SPIKE, xx + 0.5f, (float)top, 0);
        for (int y = top + T; y < wallTop; y++) // the gable: upright boards under the roof
            if (xx >= x && xx <= x + w)
                bgStyle(xx, y, shadeC(lerpColor(OLDWOOD, GREYWOOD, 0.4f), 0.62f + 0.16f * hash2(xx / 10, y / 30, seed + 7)), WALL_PLANK_V);
    }
    addDecor(DK_HORNS, (float)h.mid, (float)(h.apexY + T - 1), -1, (int)(slope * 10)); // the boards cross at the ridge, ending in carved heads
    for (int xx = x - 1; xx <= x + w + 1; xx++) // the tie beam between wall and gable
    {
        bgPut(xx, wallTop - 1, shadeC(HEART, 0.85f));
        bgPut(xx, wallTop, (xx - x) % 4 == 0 ? shadeC(HEART, 0.45f) : shadeC(HEART, 0.65f));
    }
    return h;
}

// A lean-to porch over one doorway: a sloping roof on a carved post topped with a finial.
static void paintPorch(int doorX, int sd, int g, bool solid)
{
    int len = 13, y0 = g - 31;
    for (int k = 0; k <= len; k++)
    {
        int y = y0 + k / 3, xx = doorX + sd * k;
        for (int t = 0; t < 2; t++)
        {
            if (solid) { place(xx, y + t, M::Wood); world.at(xx, y + t).shade = (uint8_t)(t ? 40 : 140 + irand(80)); }
            else bgPut(xx, y + t, shadeC(OLDWOOD, t ? 0.6f : 1.0f));
        }
    }
    int px = doorX + sd * (len - 2) - 1, py = y0 + len / 3 + 2;
    paintPost(px, py, g);
    for (int y = py - 7; y < py; y++) bgPut(px + 1, y, shadeC(HEART, 0.8f)); // finial
    bgPut(px + 1 + sd, py - 7, shadeC(HEART, 0.8f));
    bgPut(px + 1 + sd * 2, py - 6, shadeC(HEART, 0.8f));
}

// Firewood stacked log-ends out, or a bound hay bale (decor.cpp).
static void paintYardProp(int x, int g) { addDecor(DK_YARD, x + 5.0f, (float)g, chance(2) ? 0 : 1); }

// ---------------------------------------------------------------- indoors: walls, and the clutter of living
static void feastTable(int x0, int x1, int fy);
static void hearthCrane(int cx, int fy);
static void antlerSkull(int cx, int cy);
static void triskeleBanner(int cx, int top, int len);

// Rounded fieldstones bedded in mortar: the wall's own pattern (WALL_COBBLE), a footing in the base tone.
static void paintCobble(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) bgStyle(x, y, lerpColor({124, 118, 110, 255}, {106, 90, 76, 255}, hash2(x / 14, y / 10, seed + 17)), WALL_COBBLE);
}

// A room's back wall: boards darkening into the corners and up under the ceiling, a cobbled footing, a beam.
static void paintRoom(int x0, int y0, int x1, int y1) // y0 the ceiling, y1 the floor
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            float edge = std::min(1.0f, std::min({x - x0, x1 - 1 - x, (y - y0) * 2}) / 10.0f); // shadow gathers in the corners
            float k = (0.55f + 0.45f * edge) * (0.82f + 0.18f * hash2(x / 7, y / 9, seed + 8));
            bgStyle(x, y, shadeC(lerpColor({118, 86, 58, 255}, {100, 84, 70, 255}, hash2(y / 4, x / 23, seed)), k), WALL_PLANK_H);
        }
    paintCobble(x0, y1 - 6, x1, y1);
    for (int x = x0; x < x1; x++) { bgPut(x, y0, shadeC(HEART, 0.45f)); bgPut(x, y0 + 1, shadeC(HEART, 0.7f)); }
}

static void paintLadder(int x, int y0, int y1) { addDecor(DK_LADDER, x + 3.5f, (float)y1, 0, y1 - y0); }

// A little painting in a gilt frame: a longship at sundown, hills under a pale sun, a jarl, a stag, runes.
static void paintPicture(int cx, int cy) { addDecor(DK_PICTURE, (float)cx, (float)cy, irand(15)); }

// A tool hung on a wooden peg: an axe, a saw, a sickle, a hammer or a drinking horn on its strap.
static void paintTool(int x, int y, int kind) { addDecor(DK_TOOL, x + 0.5f, (float)y, kind); }

// A dresser: two rows of drawers on stubby feet, with a jug, a crock and a candle on top.
static void paintDresser(int x, int fy) { addDecor(DK_DRESSER, x + 8.0f, (float)fy); }

// A plank shelf on brackets: bowls, jars, a wheel of cheese, books.
static void paintShelf(int x, int y, int w) { addDecor(DK_SHELF, x + w / 2.0f, (float)(y + 5), -1, w); }

// Spears stood in a rack, an axe and a sword hung from its rails.
static void paintRack(int x, int fy) { addDecor(DK_RACK, x + 11.0f, (float)fy, -1, 22); }

// A loose crate (0), barrel (1) or small box (2) standing on the floor at fy: a rigid body (entities.cpp).
static int addProp(int x, int fy, int style)
{
    Interact it{IT_PROP, (float)x, (float)fy};
    it.style = style;
    it.data = style == 2 ? 2 : 3; // blows it takes
    G.inter.push_back(it);
    return (int)std::ceil(bodyHalf(it).x * 2); // its width
}

// A row of loose crates, barrels and boxes side by side, filling up to `room`; returns the width used.
static int placeStores(int x, int fy, int room)
{
    int px = x;
    for (int n = irange(1, 3); n > 0; n--)
    {
        int st = irand(3);
        int w = (int)std::ceil(bodyHalf(Interact{IT_PROP, 0, 0, false, 0, st}).x * 2);
        if (px + w > x + room) break;
        addProp(px + w / 2, fy, st);
        px += w + 1;
    }
    return px - x;
}

// A bundle of arrows in a basket.
static void paintArrows(int x, int fy) { addDecor(DK_ARROWS, x + 4.0f, (float)fy); }

// Bunks against the wall: a straw mattress and a rolled blanket on each.
static void paintBunk(int x, int fy, int tall) { addDecor(DK_BUNK, x + 10.0f, (float)fy, -1, std::max(8, std::min(22, tall - 2))); }

// A fireplace of fieldstone with a mantel and a fire in its mouth, its chimney breast going up into the beams.
static void paintFireplace(int cx, int fy, int ceil)
{
    addDecor(DK_HEARTH, (float)cx, (float)fy, -1, fy - ceil - 1);
    G.lamps.push_back({(float)cx, (float)(fy - 4), 84, LAMP_WARM, true});
}

// Fill a storey's back wall from x0 to x1: furniture standing on the floor and, between it, things hung up.
// theme: 0 anything, 1 smithy, 2 apothecary, 3 armory, 4 bedroom - the ids below are the `k` cases, listed by how common each is.
enum { TH_ANY, TH_SMITH, TH_APOTH, TH_ARMORY, TH_BED, TH_PORT };
static void furnishRoom(int x0, int x1, int fy, int ceil, bool hearth, int theme = TH_ANY)
{
    static const std::vector<int> MIX[] = {{}, {3, 5, 3, 6, 5, 7, 3}, {4, 4, 0, 4, 2, 0, 6, 4}, {5, 9, 5, 7, 5, 9, 3, 5}, {8, 8, 0, 2, 4, 0, 8}, {6, 6, 3, 6, 4, 7, 6, 3}};
    paintRoom(x0, ceil, x1, fy);
    int hx = hearth ? (x0 + x1) / 2 + irange(-8, 8) : -1000;
    if (hearth) paintFireplace(hx, fy, ceil);
    int tall = fy - ceil; // room for things hung up high
    if (chance(3) && x1 - x0 > 30) addDecor(DK_LEANSHIELD, chance(2) ? (float)x0 + 8 : (float)x1 - 8, (float)fy, -1, 0, chance(2)); // a shield leant against the wall
    for (int x = x0 + irange(3, 8); x < x1 - 12;)
    {
        if (x + 26 > hx - 13 && x < hx + 13) { x = hx + 14; continue; } // keep clear of the fireplace
        int room = std::min(x1 - 3, hearth && x < hx ? hx - 13 : x1 - 3) - x, k = theme ? MIX[theme][irand((int)MIX[theme].size())] : irand(9), used = 0;
        if (k == 0 && room >= 16) { paintDresser(x, fy); used = 16; }
        else if (k == 1 && room >= 26) { feastTable(x + 4, x + 21, fy); used = 26; }
        else if (k == 2 && room >= 14 && tall >= 20) { paintPicture(x + 7, fy - tall / 2 - 4); used = 14; }
        else if (k == 3 && room >= 14 && tall >= 20) { for (int t = 0; t < 2 + (room >= 20); t++) paintTool(x + 2 + t * 6, fy - tall + 5, irand(5)); used = room >= 20 ? 18 : 12; }
        else if (k == 4 && room >= 14) { paintShelf(x, fy - tall / 2 - 3, 14); used = 14; }
        else if (k == 5 && room >= 18) { paintRack(x + 1, fy); used = 18; }
        else if (k == 6 && room >= 13) used = placeStores(x + 1, fy, room - 1) + 1; // loose: knock them about
        else if (k == 7 && room >= 10) { paintArrows(x + 1, fy); used = 10; }
        else if (k == 8 && room >= 22 && tall >= 20) { paintBunk(x, fy, tall); used = 21; }
        else if (k == 9 && room >= 12 && tall >= 20) { paintShield(x + 4, fy - tall / 2, 5); paintShield(x + 11, fy - tall / 2 + 2, 5); used = 14; } // shields on the wall
        x += (used ? used : 4) + irange(3, 8);
    }
}

// A Norse watchtower: a fieldstone foot with a doorway right through it, log walls slit for arrows and
// strapped with iron at every floor, floors you climb by jumping up through hatches that swap sides, and on
// top an archers' deck jutting out past the walls - torches on its corner posts, its rail hung with shields,
// open to the wind on both sides under a shingled roof on posts (or under the sky). Some have a lean-to.
static void placeWatchtower(int x, int TW)
{
    const int S = 24, ov = 6; // storey height, the deck's overhang
    int g = surf[x + TW / 2], levels = irange(2, 4), deck = g - S * levels;
    bool roofed = !chance(3);
    int shed = chance(2) ? (chance(2) ? -1 : 1) : 0; // a lean-to on one side
    levelGround(x - ov - 6 - (shed < 0 ? 18 : 0), x + TW + ov + 6 + (shed > 0 ? 18 : 0), g);
    auto hatchX = [&](int lv) { return lv % 2 ? x + 5 : x + TW - 16; };
    for (int lv = 0; lv < levels; lv++) // each storey furnished
    {
        int fy = g - lv * S;
        furnishRoom(x + 4, x + TW - 4, fy, fy - S + 2, false);
        paintLadder(hatchX(lv + 1) + 2, fy - S + 2, fy);
        hangLantern(lv % 2 ? x + TW - 14 : x + 14, fy - S + 2, 3);
    }
    for (int y = deck; y < g; y++) // the walls
        for (int k = 0; k < 4; k++)
        {
            int up = (g - y) % S;
            bool door = y >= g - 24, slit = y < g - S && up >= 12 && up < 17;
            if (door || slit) continue;
            for (int wx : {x + k, x + TW - 1 - k})
            {
                if (y >= g - 30) { place(wx, y, M::Stone); world.at(wx, y).shade = (uint8_t)(((g - y) % 5 == 0 || (wx + (g - y) / 5 * 3) % 6 == 0) ? 20 : 140 + irand(90)); }
                else if (up <= 2 && k < 2) place(wx, y, M::Metal); // an iron strap round each floor
                else { place(wx, y, M::Wood); world.at(wx, y).shade = (uint8_t)((g - y) % 4 == 0 ? 25 : 120 + ((g - y) % 4) * 30 + irand(20)); } // stacked logs
            }
        }
    for (int lv = 1; lv <= levels; lv++) // floors, the deck wider than the rest
    {
        int fy = g - lv * S, a = lv == levels ? x - ov : x + 4, b = lv == levels ? x + TW - 1 + ov : x + TW - 5;
        for (int xx = a; xx <= b; xx++) // one-way all across: jump up through anywhere
            for (int t = 0; t < 2; t++) place(xx, fy + t, M::Platform);
    }
    int roofY = deck - 30; // the lookout's roof, high enough for archers to stand under it
    for (int sd : {-1, 1}) // braces under the overhang, the corner posts, and a torch on each
    {
        int wall = sd < 0 ? x : x + TW - 1, edge = wall + sd * ov;
        for (int k = 0; k <= ov; k++) for (int t = 0; t < 2; t++) bgPut(wall + sd * k, deck + 2 + ov - k + t, shadeC(HEART, t ? 0.5f : 0.8f));
        int postTop = roofed ? roofY + 1 : deck - 12;
        for (int y = postTop; y < deck; y++)
            for (int k = 0; k < 3; k++)
            {
                bool rail = y >= deck - 12;
                if (!rail && k == 0) continue; // above the rail the post is slimmer
                place(edge - sd * k, y, M::Wood);
                world.at(edge - sd * k, y).shade = (uint8_t)(k == 1 ? 200 : 90);
            }
        for (int xx = edge - sd * 4; xx != edge - sd * 7; xx -= sd) bgPut(xx, deck - 15, {56, 56, 62, 255}); // a bracket holding the torch
        G.lamps.push_back({(float)(edge - sd * 6), (float)deck - 18, 70, LAMP_WARM, true});
        if (!roofed) // dragon heads rearing over the drop instead
        {
            static const char* DRAGON[7] = {"..###...", ".#####..", "##.####.", "..######", "...##.##", "...##...", "..###..."};
            for (int j = 0; j < 7; j++)
                for (int i = 0; i < 8; i++)
                    if (DRAGON[j][i] == '#') bgPut(edge - sd + sd * (i - 2), deck - 19 + j, j == 2 && i == 2 ? Color{220, 60, 40, 255} : shadeC(HEART, 0.9f - 0.05f * j));
        }
    }
    for (int xx = x - ov + 3; xx <= x + TW - 4 + ov; xx++) bgPut(xx, deck - 9, shadeC(HEART, 0.7f)); // the shield rail
    if (roofed) // shingles on a low pitch, eaves past the posts; a lantern hangs from the ridge
    {
        int half = TW / 2 + ov + 5, mid = x + TW / 2;
        for (int r = 0; r < 8; r++)
            for (int xx = mid - half + r * 2; xx <= mid + half - r * 2; xx++)
            {
                place(xx, roofY - r, M::Wood);
                world.at(xx, roofY - r).shade = (uint8_t)(r == 0 ? 25 : (((r + (xx / 3) % 2) % 3 == 0) ? 50 : 140 + irand(80)));
            }
        for (int y = roofY - 10; y < roofY - 7; y++) bgPut(mid, y, shadeC(HEART, 0.8f)); // a finial
        hangLantern(mid, roofY + 1, 5);
        for (int y = roofY + 1; y < deck - 12; y++) // the back of the lookout: open framing against the sky
            for (int xx = x - ov + 3; xx <= x + TW + ov - 4; xx++)
                if ((xx - x) % 12 == 0 || y == roofY + 1) bgPut(xx, y, shadeC(HEART, 0.6f));
    }
    else
    {
        int pole = chance(2) ? x - ov + 1 : x + TW + ov - 2; // a pennant streaming from a corner
        for (int y = deck - 34; y < deck - 12; y++) bgPut(pole, y, {92, 62, 36, 255});
        Color cloth = chance(2) ? Color{150, 30, 30, 255} : Color{30, 50, 120, 255};
        for (int k = 0; k < 14; k++)
            for (int t = 0; t < 6 - k * 5 / 14; t++) bgPut(pole + (pole < x ? -1 : 1) * (k + 1), deck - 33 + t + (int)(std::sin(k * 0.6f) * 1.2f), shadeC(cloth, t == 0 ? 1.2f : 0.9f));
    }
    if (shed) // a lean-to against the foot: a sloping plank roof on a post, firewood and stores under it
    {
        int wall = shed < 0 ? x - 1 : x + TW, len = 18;
        for (int k = 0; k <= len; k++)
            for (int t = 0; t < 2; t++) { place(wall + shed * k, g - 30 + k / 3 + t, M::Wood); world.at(wall + shed * k, g - 30 + k / 3 + t).shade = (uint8_t)(t ? 30 : 150 + irand(70)); }
        paintPost(wall + shed * (len - 1) - 1, g - 30 + len / 3 + 2, g);
        for (int y = g - 28 + len / 3; y < g; y++) for (int k = 1; k < len - 1; k++) bgPut(wall + shed * k, y, shadeC(OLDWOOD, 0.45f));
        paintYardProp(shed < 0 ? wall - 15 : wall + 5, g);
        addProp(shed < 0 ? wall - 4 : wall + 4, g, irand(3));
    }
    G.inter.push_back({IT_TORCH, (float)x - ov - 4 + (shed < 0 ? -18 : 0), (float)g});
    lookouts.push_back({(float)x + TW / 2 + 2, (float)deck});
    if (levels >= 3) lookouts.push_back({(float)x - 2, (float)deck});
}

// A farmhouse of two or three storeys you walk straight through: doorways at both ends, plank floors you
// climb by jumping up through stairwells (they swap sides), every room furnished and lit by oil lanterns
// hung from the beams, and maybe a cellar under a trapdoor.
// A window in a house's back wall, Norse fashion: no glass, no shutter, just an opening cut in the planking, narrow at the top and
// widening down in an upside-down parabola, edged with pale carved oak and a heavy sill. The night sky shows through (stars and all),
// and a faint slanting beam of moonlight comes in across the room to the floor, `len` units below the sill (drawn live: entities.cpp;
// Lamp::dim keeps it subtle and leaves out the flat glow a pane would give). It slides sideways to keep clear of every post and beam,
// and is painted only over plain wall, so nothing already hung or stood there is drawn across by it.
static void hutWindow(int cx, int top, int w, int h, int len)
{
    int half = w / 2 + 4;
    auto clash = [&](int c) { for (const auto& p : postList) if (std::abs(c - p[0]) < half && p[1] <= top + h + 3 && p[2] >= top - 4) return true; return false; };
    for (int d = 1; d <= 14 && clash(cx); d++)
        for (int s : {-1, 1}) if (!clash(cx + s * d)) { cx += s * d; d = 99; break; }
    const Color oak = {138, 98, 60, 255}, plank = {150, 108, 66, 255};
    float hw = w / 2.0f;
    auto open = [&](int dx, int y) { // inside the parabola: the half-width grows with the square root of the depth below the apex
        float t = (y - top + 0.5f) / (h * 0.55f);
        return y >= top && y < top + h && std::abs(dx) <= hw * std::sqrt(std::min(1.0f, std::max(0.0f, t)));
    };
    for (int y = top - 3; y <= top + h + 1; y++)
        for (int dx = -w / 2 - 3; dx <= w / 2 + 3; dx++)
        {
            int x = cx + dx;
            if (!world.in(x, y) || world.bgAt(x, y).a == 255) continue; // behind everything: furniture, hangings and posts already painted here stay on top
            if (y > top + h - 1) { if (y <= top + h && std::abs(dx) <= w / 2 + 3) bgPut(x, y, shadeC(plank, y == top + h ? 1.15f : 0.7f)); continue; } // the sill
            if (open(dx, y))
            {
                bool reveal = dx <= -w / 2 + 1 || !open(dx, y - 2); // the near jamb and the underside of the arch are in shade
                Color sky = {(unsigned char)(44 + 8 * (y - top) / h), (unsigned char)(62 + 8 * (y - top) / h), (unsigned char)(116 + 6 * (y - top) / h), 255};
                world.bgAt(x, y) = reveal ? shadeC(sky, 0.62f) : sky;
                world.skyAt(x, y) = reveal ? 0 : 1;
                continue;
            }
            bool edge = false; // within two cells of the opening: the carved frame
            for (int k = 1; k <= 2 && !edge; k++) edge = open(dx - k, y) || open(dx + k, y) || open(dx, y + k) || open(dx - k, y + k) || open(dx + k, y + k);
            if (edge) bgPut(x, y, shadeC(oak, (dx < 0 || y < top + 2 ? 1.0f : 0.66f) + 0.1f * hash2(x, y, seed + 33) - ((x + y) % 5 == 0 ? 0.18f : 0))); // knot-carved notches along it
        }
    Lamp l{(float)cx, (float)top + h / 2.0f, 90, {150, 180, 235, 255}};
    l.beam = (float)len; l.w = (float)(w - 2); l.wh = (float)h; l.dim = 0.6f;
    G.lamps.push_back(l);
}

static bool villageHouses = false; // Hearthwick's cottages: no clutter piled against the doors, no cellar
// shop: -1 an ordinary farmhouse, else a TH_ theme for the ground floor (smithy, apothecary, armory); shops keep a
// bedroom on the top floor, and the armory always has a basement and an upstairs.
static void placeHouse(int x, int w, int shop = -1)
{
    int g = surf[x + w / 2];
    levelGround(x - 8, x + w + 8, g);
    const int S = 26;
    int storeys = shop == TH_ARMORY || shop == TH_PORT ? 2 : (w < 100 ? irange(1, 2) : irange(2, 3)), wallTop = g - storeys * S - irange(3, 7);
    Hall hall = paintHall(x, w, g, wallTop, true, g - 24);
    postList.push_back({x + 4, wallTop, g}); postList.push_back({x + w - 4, wallTop, g}); // the end posts (painted below), known before the windows so they keep clear
    int first = chance(2); // which side the first stairwell is on
    auto wellX = [&](int k) { return (k + first) % 2 ? x + 8 : x + w - 26; };
    for (int k = 0; k < storeys; k++)
    {
        int fy = g - k * S, ceil = k == storeys - 1 ? wallTop + 1 : fy - S + 2;
        furnishRoom(x + 3, x + w - 2, fy, ceil, k == 0, shop < 0 ? TH_ANY : (k == 0 ? shop : (k == storeys - 1 ? TH_BED : shop == TH_APOTH ? TH_APOTH : TH_ANY)));
        paintPost(x + 3, ceil + 2, k == 0 ? g - 24 : fy); // the ground floor's end posts stop at the top of the door frame
        paintPost(x + w - 5, ceil + 2, k == 0 ? g - 24 : fy);
        if (k + 1 < storeys) // the floor above, and its stairwell
        {
            int up = fy - S, h0 = wellX(k + 1);
            paintLadder(h0 + 5, up + 2, fy);
            for (int xx = x + 3; xx <= x + w - 3; xx++) // the whole floor is one-way: jump up through it anywhere
                for (int t = 0; t < 2; t++) place(xx, up + t, M::Platform);
            for (int lx = x + 16 + irand(10); lx < x + w - 14; lx += irange(34, 50)) // lanterns hung from its beams
                if (lx + 3 < h0 || lx - 3 > h0 + 18) hangLantern(lx, up + 2, irange(2, 4));
        }
    }
    hangLantern(hall.mid, hall.apexY + 8, std::max(3, wallTop + 4 - (hall.apexY + 8))); // from the ridge, into the top room
    for (int k = 0; k < storeys; k++) // windows on every floor, the moon coming in through them
    {
        int fy = g - k * S, jit = irange(-3, 3);
        for (int sd : {-1, 1})
            if (w >= 64 || sd < 0) hutWindow(x + w / 2 + sd * std::max(16, w * 3 / 10) + jit, fy - 23, 9, 11, 12);
    }
    for (int y = wallTop; y < g; y++) // end walls: doorways below, a window on every floor above
    {
        bool door = y >= g - 24, window = false;
        for (int k = 1; k < storeys; k++) window = window || (y >= g - k * S - 18 && y < g - k * S - 7);
        if (door || window) continue;
        for (int k = 0; k < 3; k++) { place(x + k, y, M::Wood); place(x + w - k, y, M::Wood); }
    }
    for (int xx = x; xx <= x + w; xx++) { place(xx, g, M::Wood); place(xx, g + 1, M::Wood); } // floorboards
    bool blockL = !villageHouses && irand(3) > 0, blockR = !villageHouses && irand(3) > 0;
    if (blockL) placeDoor(x, g, -1); // a closed door in either end wall: F opens it, or break it down
    if (blockR) placeDoor(x + w - 2, g, 1);
    int sd = chance(2) ? -1 : 1;
    for (int side : {-1, 1}) // the way out of either door is left clear: nothing standing, hanging or piled in front of it
    {
        int e0 = side < 0 ? x - 18 : x + w + 1, e1 = side < 0 ? x - 1 : x + w + 18;
        doorYards.push_back({e0, e1});
        for (int yy = g - 34; yy < g; yy++)
            for (int xx = e0; xx <= e1; xx++)
                if (world.in(xx, yy) && world.at(xx, yy).material != M::Empty) world.at(xx, yy) = Cell{};
        for (size_t i = 0; i < G.inter.size();) // and no props or yard clutter there either
        {
            const Interact& it = G.inter[i];
            if ((it.type == IT_DECOR || it.type == IT_PROP || it.type == IT_TORCH) && it.x >= e0 - 4 && it.x <= e1 + 4 && it.y >= g - 3 && it.y <= g + 3) G.inter.erase(G.inter.begin() + i);
            else i++;
        }
    }
    if (chance(2)) paintCobweb(x + 3, wallTop + 2, 1); // up in the rafters
    G.inter.push_back({IT_TORCH, (float)x + 10, (float)g});
    if (shop >= 0) // a sign by the door: a hammer, a sickle, a pair of shields
    {
        int dx = sd < 0 ? x - 4 : x + w + 4;
        if (shop == TH_ARMORY) { paintShield(dx, g - 18, 5); paintShield(dx + sd * 8, g - 14, 5); }
        else paintTool(dx, g - 20, shop == TH_SMITH ? 3 : shop == TH_PORT ? 4 : 2); // a hammer, a drinking horn, a sickle
    }
    if (villageHouses || (shop != TH_ARMORY && chance(3))) return; // no cellar under this one
    // the cellar, reached by a plank trapdoor
    int cf = g + irange(24, 32);
    for (int y = g + 2; y < cf; y++)
        for (int xx = x + 4; xx <= x + w - 4; xx++) world.at(xx, y) = Cell{};
    for (int y = g + 2; y < cf; y++)
        for (int xx = x + 4; xx <= x + w - 4; xx++) bgStyle(xx, y, shadeC(Color{112, 106, 100, 255}, 0.5f), WALL_COBBLE);
    for (int k = irange(1, 4); k > 0; k--) addProp(x + 8 + irand(std::max(1, w - 18)), cf, chance(3) ? 0 : 1); // barrels and crates
    int td = x + irange(12, w - 12); // trapdoor
    for (int xx = td - 6; xx <= td + 6; xx++) { place(xx, g, M::Platform); place(xx, g + 1, M::Platform); }
    for (int xx = x + 2; xx <= x + w - 2; xx++) place(xx, cf, M::Stone);
    paintCobweb(x + 4, g + 2, 1);
    paintCobweb(x + w - 4, g + 2, -1);
    G.inter.push_back({IT_TORCH, (float)x + w - 6, (float)cf});
    cellars.push_back({x + 4, x + w - 4, cf});
}

static void linkCellars()
{
    for (size_t i = 1; i < cellars.size(); i++)
    {
        const Cellar& a = cellars[i - 1];
        const Cellar& b = cellars[i];
        int n = b.x0 - a.x1;
        for (int k = 0; k <= n; k++) // a crouch-height tunnel, sagging a little in the middle
        {
            int x = a.x1 + k;
            int f = a.floor + (b.floor - a.floor) * k / std::max(1, n) + (int)(std::sin(3.14159f * k / std::max(1, n)) * 10);
            for (int y = f - 15; y < f; y++)
                if (world.in(x, y) && world.at(x, y).material != M::Bedrock && world.at(x, y).material != M::Platform) world.at(x, y) = Cell{};
            if (world.at(x, f).material == M::Empty) place(x, f, M::Stone);
        }
    }
}

// ---------------------------------------------------------------- castle

static void carveWorld(float cx, float cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy > r * r) continue;
            int x = (int)cx + dx, y = (int)cy + dy;
            if (world.in(x, y) && world.at(x, y).material != M::Bedrock && x > 4 && y > 4 && x < W - 5 && y < H - 5) world.at(x, y) = Cell{};
        }
}

struct LevelEnds { int sx, sy, ex, ey; };

static bool roadValid = false;      // the biome being built has a single road (not the castle)
int roadFloorAt(int x)
{
    if (x < 0 || x >= (int)worldRoad.size()) return -1;
    return worldRoad[x];
}

// Carve a tunnel you can actually walk: never steeper than ~40 degrees, switching back and forth
// when it has to drop (or climb) further than it travels.
static void carveRamp(float x, float y, float tx, float ty, int r)
{
    float sx0 = x, dirX = tx >= x ? 1.0f : -1.0f;
    for (int guard = 0; guard < 6000 && (std::fabs(tx - x) > 3 || std::fabs(ty - y) > 3); guard++)
    {
        carveWorld(x, y, r);
        float dx = tx - x, dy = ty - y;
        if (std::fabs(dy) > std::fabs(dx) + 4)
        {
            x += dirX * 1.2f;
            y += (dy > 0 ? 1.0f : -1.0f) * 0.9f;
            if (x - sx0 > 22) dirX = -1;
            if (x - sx0 < -22) dirX = 1;
        }
        else
        {
            float l = std::sqrt(dx * dx + dy * dy);
            x += dx / l * 1.5f;
            y += dy / l * 1.5f;
        }
    }
}

// Castle Dunmoor, laid out like a Dead Cells biome: you enter a grand hall at the top, then a chain of
// rooms of different sizes wanders downward - sideways ramps, drop-downs, platform-capped shafts,
// uneven floors and ledges - changing direction as it goes.
struct Room { int x0, y0, x1, y1; }; // interior; the floor is the row below y1

// ---------------------------------------------------------------- the Norse castle: hall furnishings

// A tall arched window, open to the night: no glass and no lattice, just a dressed-stone arch (alternating voussoirs, a pale sill, a
// deep shadowed reveal on its far side) round a round-topped opening onto the stars. A faint, narrow shaft of moonlight comes in
// below it (Lamp::dim keeps the shaft subtle and leaves out the flat glow a pane would give).
static void latticeWindow(int cx, int top, int w, int h)
{
    int R2 = w / 2;
    for (int y = top; y < top + h; y++)
        for (int dx = -R2; dx <= R2; dx++)
        {
            int ay = top + R2 - y; // above the springing of the arch
            float r = std::sqrt((float)(dx * dx + ay * ay));
            if (ay > 0 && r > R2 + 0.5f) continue;
            int x = cx + dx;
            bool arch = ay > 0, ring = arch ? r > R2 - 2.2f : std::abs(dx) >= R2 - 1;
            bool sill = y >= top + h - 2;
            if (!world.in(x, y)) continue;
            if (sill) { bgPut(x, y, y == top + h - 2 ? Color{138, 132, 146, 255} : Color{96, 90, 104, 255}); continue; }
            if (ring)
            {
                int seg = arch ? (int)((std::atan2((float)ay, (float)dx) + 3.15f) * 3.2f) : y / 5;
                Color c = seg % 2 ? Color{122, 118, 132, 255} : Color{92, 88, 102, 255};
                if (dx < 0 && !arch) c = shadeC(c, 1.12f);
                bgPut(x, y, c);
                continue;
            }
            float depth = (float)(y - top) / h; // the sky pales toward the horizon
            Color sky = {(unsigned char)(20 + 16 * depth), (unsigned char)(28 + 20 * depth), (unsigned char)(60 + 30 * depth), 255};
            bool reveal = dx <= -R2 + 3 || (arch && r > R2 - 3.4f); // the reveal on the near side and under the arch is in shadow
            world.bgAt(x, y) = reveal ? shadeC(sky, 0.6f) : sky;
            world.skyAt(x, y) = reveal ? 0 : 1;
        }
    Lamp l{(float)cx, (float)top + h / 2.0f, 80, {150, 180, 235, 255}};
    l.beam = std::min(34.0f, (float)h); l.w = (float)(w - 6); l.wh = (float)h; l.dim = 0.9f;
    G.lamps.push_back(l);
}

// A red pillar carved with interlaced dragons, on a dark plinth under a dark capital.
static void dragonPillar(int cx, int y0, int y1, int hw = 4) { (void)hw; addDecor(DK_DRAGONPILLAR, cx + 0.5f, (float)(y1 + 1), 0, y1 - y0 + 1); }

// A post carved into a bearded god's face near its top.
static void idolPillar(int cx, int y0, int y1) { addDecor(DK_IDOL, cx + 0.5f, (float)y1, 0, y1 - y0); }

// A black banner with a white triskele and a fringe.
static void triskeleBanner(int cx, int top, int len) { addDecor(DK_TAPESTRY, (float)cx, (float)(top - 1), 6 | 32, len); } // always the wide cloth, so the triskele has room

// An elk skull with antlers, hung on the wall.
static void antlerSkull(int cx, int cy) { addDecor(DK_ANTLERS, cx + 0.5f, (float)(cy + 1)); }

// A trestle table laid for a feast, with benches either side.
static void feastTable(int x0, int x1, int fy) { addDecor(DK_TABLE, (x0 + x1) / 2.0f, (float)fy, -1, x1 - x0 + 1); }

// A hearth pit with a cauldron hung from a crane of lashed poles over the flames.
static void hearthCrane(int cx, int fy)
{
    addDecor(DK_CRANE, (float)cx, (float)fy, 0);
    G.lamps.push_back({(float)cx, (float)(fy - 4), 90, LAMP_WARM, true});
}

// A stone sarcophagus with an effigy on the lid; solid, so it can be climbed (and broken).
static void sarcophagus(int x0, int fy)
{
    for (int y = fy - 9; y < fy; y++)
        for (int x = x0; x < x0 + 24; x++)
        {
            place(x, y, M::Masonry);
            bool lid = y < fy - 6, carve = !lid && ((x - x0) % 6 == 3 || y == fy - 3);
            world.at(x, y).shade = (uint8_t)(lid ? 210 : (carve ? 30 : 130 + irand(40)));
        }
    for (int x = x0 + 4; x < x0 + 20; x++) { place(x, fy - 10, M::Stone); world.at(x, fy - 10).shade = 220; } // the effigy
    place(x0 + 4, fy - 11, M::Stone); place(x0 + 5, fy - 11, M::Stone); // its head
}

static void paintFallen(int x, int g, float k);
static void decorateRoom(const Room& r, bool grand)
{
    int w = r.x1 - r.x0, h = r.y1 - r.y0;
    // back wall: lattice windows, black banners, and in the deep halls the dead in their stone beds
    bool crypt = r.y0 > castleGround + 260;
    for (int wx = r.x0 + 18 + irand(12); wx < r.x1 - 12; wx += irange(36, 60))
    {
        if (!crypt && (grand || chance(3)))
        {
            int wh = std::min(h - 14, grand ? 44 : 28);
            latticeWindow(wx, r.y0 + 6, grand ? 15 : 11, wh);
            if (chance(2) && h >= 50) addDecor(DK_DRAPE, (float)wx, (float)(r.y0 + 3), irand(64), std::min(h - 8, wh + 16)); // old drapes either side of it
        }
        else if (int roll = irand(3); roll == 0) triskeleBanner(wx, r.y0 + 8, std::min(30, h - 16));
        else if (roll == 1 && h >= 44) addDecor(DK_TAPESTRY, (float)wx, (float)(r.y0 + 4), irand(64), irange(24, std::min(56, h - 14)), chance(2)); // a tapestry
    }
    for (int cx = r.x0 + 14 + irand(20); cx < r.x1 - 10 && h >= 44; cx += irange(40, 90)) // chains from the ceiling, the way the lanterns hang
        if (chance(3)) addDecor(DK_CHAIN, (float)cx, (float)r.y0, irand(64), irange(10, std::min(46, h - 24)));
    for (int k = crypt ? irange(1, 5) : (chance(2) ? irange(1, 3) : 0); k > 0; k--) // old blood: on the walls, in pools on the floor
    {
        int bx = irange(r.x0 + 14, std::max(r.x0 + 15, r.x1 - 14)), v = irand(6) + 6 * irand(10);
        if (v % 6 == 4) addDecor(DK_BLOOD, (float)bx, (float)(r.y1 + 1), v, irange(10, 20));
        else addDecor(DK_BLOOD, (float)bx, (float)irange(r.y0 + 16, std::max(r.y0 + 17, r.y1 - 6)), v, irange(12, 22), chance(2));
    }
    if (crypt)
        for (int k = irange(1, 2), sx = r.x0 + irange(10, 40); k > 0 && sx < r.x1 - 30; k--, sx += irange(40, 70)) sarcophagus(sx, r.y1 + 1);
    else if (!grand && w > 120 && chance(3)) feastTable(r.x0 + w / 2 - 20, r.x0 + w / 2 + 20, r.y1 + 1);
    for (int tx = r.x0 + 12; tx < r.x1 - 8; tx += irange(50, 80)) G.inter.push_back({IT_TORCH, (float)tx, (float)(r.y1 + 1)});
    if (w > 60) // where the floor meets the back wall: the fallen slumped against it (painted flat), their blood, racked weapons
    {
        int fy = r.y1 + 1;
        for (int k = crypt ? irange(0, 1) : irange(0, 2); k > 0; k--)
            if (chance(2))
            {
                int bx = irange(r.x0 + 16, r.x1 - 34);
                paintFallen(bx, fy, 0.3f);
                addDecor(DK_BLOOD, (float)(bx + irange(-2, 8)), (float)fy, 4 + 6 * irand(10), irange(10, 18));
            }
        if (!crypt && chance(2)) paintRack(irange(r.x0 + 10, r.x1 - 34), fy);
        if (h >= 44 && chance(6)) addDecor(DK_LEAK, (float)irange(r.x0 + 14, r.x1 - 14), (float)irange(r.y0 + 12, std::max(r.y0 + 13, r.y1 - 36)), irand(4)); // a hole where water breaks through
    }
    if (grand) return;
    // uneven floor: raised steps and plinths
    for (int k = irand(3); k > 0; k--)
    {
        int bw = irange(8, 26), bh = irange(4, 11), bx = irange(r.x0 + 4, std::max(r.x0 + 5, r.x1 - bw - 4));
        for (int y = r.y1 - bh + 1; y <= r.y1; y++)
            for (int x = bx; x < bx + bw; x++) place(x, y, M::Masonry);
    }
    // one-way ledges for verticality
    if (h > 46)
        for (int k = irange(1, 2); k > 0; k--)
        {
            int lw = irange(16, 40), lx = irange(r.x0 + 4, std::max(r.x0 + 5, r.x1 - lw - 4)), ly = r.y1 - irange(18, std::min(34, h - 26));
            for (int x = lx; x < lx + lw; x++) { place(x, ly, M::Platform); place(x, ly + 1, M::Platform); }
        }
    (void)w;
}

// Castle Dunmoor, laid out like a Dead Cells biome but at castle scale. You come through the gatehouse
// into the open grounds, enter the keep's great hall at ground level, then a chain of halls and passages
// wanders down through the cellars and basements - sideways ramps, drop-downs, plank-capped shafts -
// to the crypt gate at the bottom. `g` is the ground level, matched to the gatehouse you came through.
static LevelEnds castleLayout(const StageDef& d, int g)
{
    const int keepX0 = 380, keepX1 = W - 580; // the keep fills all but the crag at the far end
    castleGround = g;
    surf.assign(W, g);
    for (int x = 0; x < W; x++)
    {
        if (x >= keepX0 && x < keepX1) surf[x] = g - 230 - ((((x - keepX0) / 110) % 3 == 1) ? 90 : 0); // curtain walls, with towers above them
        else if (x >= keepX1) surf[x] = g - 50 + (int)(fbm(x * 0.01f, 2.2f, seed, 2) * 40); // the crag the castle is built into
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            bool keep = x >= keepX0 && x < keepX1;
            if (x >= W - 4 || y >= H - 4 || (x < 4 && y >= g + 14)) { place(x, y, M::Bedrock); continue; }
            if (y < surf[x])
            {
                if (keep && y >= surf[x] - 7 && ((x - keepX0) / 6) % 2 == 0) place(x, y, M::Masonry); // merlons
                continue;
            }
            if (keep && y < g + 6) place(x, y, M::Masonry);
            else if (!keep && y < surf[x] + 14) place(x, y, y < surf[x] + 2 ? M::Grass : M::Dirt);
            else place(x, y, M::Basalt); // dark rock between the halls
        }
    for (int t0 = keepX0 + 110; t0 + 110 <= keepX1; t0 += 330) // a tiled conical roof on each tower, eaves overhanging the walls
    {
        int cx = t0 + 55, top = surf[t0 + 55];
        for (int x = t0 - 7; x < t0 + 117; x++)
        {
            int hr = (int)(96 * std::pow(std::max(0.0f, 1 - std::fabs((float)(x - cx)) / 62.0f), 1.45f)); // a flared, slightly concave cone
            if (x == cx || x == cx + 1) hr += 14; // a finial on the point
            for (int y = top - hr; y < top; y++) if (world.in(x, y)) place(x, y, M::Brick);
        }
    }
    for (int dx = 0; dx < 14; dx++) // the keep's two ends rounded off instead of square
        for (int dy = -8; dy < 14; dy++)
            if ((14 - dx) * (14 - dx) + (14 - std::max(dy, 0)) * (14 - std::max(dy, 0)) > 196)
                for (int xx : {keepX0 + dx, keepX1 - 1 - dx}) { int y = surf[xx] + dy; if (world.in(xx, y)) world.at(xx, y) = Cell{}; }
    for (int x = keepX0; x < keepX1; x += 37) // arrow slits in the curtain wall
        for (int y = surf[x] + 18; y < surf[x] + 34; y++) world.at(x, y).shade = 0;
    buildBackground(d, true);
    { // inside, the back wall is dark stone tile (see WALL_TILE): whatever is painted over it later keeps its own colours
        NoiseGrid tn(W, H, 4, [&](float x, float y) { return fbm(x * 0.03f, y * 0.04f, seed + 55, 2); });
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (y >= surf[x] + 6 && !world.skyAt(x, y)) bgStyle(x, y, lerpColor({92, 82, 108, 255}, {124, 106, 130, 255}, tn.at(x, y)), WALL_TILE);
    }
    for (int x = keepX0 + 30; x < keepX1 - 30; x += irange(44, 64)) // arched windows in the keep, some lit by candles
        for (int y = surf[x] + 40; y < g - 175; y += irange(46, 60))
        {
            bool lit = !chance(3);
            for (int wy = y; wy < y + 9; wy++)
                for (int wx = x - 1; wx <= x + 1; wx++)
                {
                    if (wy == y && wx != x) continue; // the arch
                    world.at(wx, wy) = Cell{};
                    world.bgAt(wx, wy) = lit ? (wy > y + 5 ? Color{214, 136, 64, 255} : Color{255, 196, 110, 255}) : Color{20, 18, 24, 255};
                    world.skyAt(wx, wy) = 0;
                }
            if (lit) G.lamps.push_back({(float)x, (float)(y + 5), 16, LAMP_WARM});
        }

    std::vector<Room> rooms;
    Room lobby{keepX0 + 34, g - 150, keepX0 + 430, g - 1}; // the great hall, on the ground floor of the keep
    clearRect(lobby.x0, lobby.y0, lobby.x1, lobby.y1);
    for (int y = g - 64; y < g; y++) // the great doors, under a rounded arch
        for (int x = keepX0; x < lobby.x0; x++)
        {
            int ay = y - (g - 64), r = (lobby.x0 - keepX0) / 2;
            int dx = x - keepX0 - r;
            if (ay < r && dx * dx + (r - ay) * (r - ay) > r * r) continue;
            world.at(x, y) = Cell{};
        }
    { // the great hall: a timber hall built inside the stone keep
        paintBoards(lobby.x0, lobby.y0, lobby.x1 + 1, lobby.y1 + 1, 6);
        for (int x = lobby.x0; x <= lobby.x1; x++) // wainscot rail and the beam under the roof
        {
            bgPut(x, lobby.y1 - 30, shadeC(HEART, 0.7f)); bgPut(x, lobby.y1 - 29, shadeC(HEART, 0.45f));
            for (int k = 0; k < 4; k++) bgPut(x, lobby.y0 + k, shadeC(OLDWOOD, 0.5f + 0.1f * k));
        }
        int mid = (lobby.x0 + lobby.x1) / 2;
        for (int px = lobby.x0 + 40; px < lobby.x1 - 20; px += 64) // red dragon pillars, braced to the roof beam
        {
            if (std::abs(px - mid) < 30) continue;
            dragonPillar(px, lobby.y0 + 4, lobby.y1);
            for (int k = 0; k < 14; k++) { bgPut(px - 5 - k, lobby.y0 + 4 + k, shadeC(OLDWOOD, 0.6f)); bgPut(px + 5 + k, lobby.y0 + 4 + k, shadeC(OLDWOOD, 0.6f)); }
        }
        for (int px = lobby.x0 + 72; px < lobby.x1 - 30; px += 64) // between them: windows high up, banners below
        {
            if (std::abs(px - mid) < 40) continue;
            latticeWindow(px, lobby.y0 + 14, 15, 40);
            if (chance(2)) triskeleBanner(px, lobby.y0 + 60, 34);
        }
        idolPillar(mid - 34, lobby.y1 - 70, lobby.y1);
        idolPillar(mid + 34, lobby.y1 - 70, lobby.y1);
        antlerSkull(mid, lobby.y0 + 30);
        hearthCrane(mid, lobby.y1 + 1);
        for (int cx = lobby.x0 + 50; cx < lobby.x1 - 40; cx += irange(70, 110)) addDecor(DK_CHAIN, (float)cx, (float)(lobby.y0 + 4), irand(64), irange(26, 50)); // chains from the roof beam
        for (int k = 0; k < 5; k++) addDecor(DK_BLOOD, (float)irange(lobby.x0 + 10, lobby.x0 + 90), (float)irange(lobby.y0 + 60, lobby.y1 - 10), irand(6) + 6 * irand(10), irange(14, 22), chance(2)); // a fight at the doors
        addDecor(DK_BLOOD, (float)(lobby.x0 + irange(14, 60)), (float)(lobby.y1 + 1), 4 + 6 * irand(10), irange(14, 22));
        feastTable(lobby.x0 + 50, lobby.x0 + 120, lobby.y1 + 1);
        feastTable(lobby.x1 - 120, lobby.x1 - 50, lobby.y1 + 1);
        for (int tx = lobby.x0 + 12; tx < lobby.x1 - 8; tx += 60) G.inter.push_back({IT_TORCH, (float)tx, (float)(lobby.y1 + 1)});
    }
    rooms.push_back(lobby);
    G.inter.push_back({IT_TORCH, (float)keepX0 - 10, (float)g});
    G.inter.push_back({IT_TORCH, (float)lobby.x0 + 8, (float)g});
    for (int x = 70; x < keepX0 - 40; x += irange(70, 110)) placeTree(x, g, 60);
    { // a well in the courtyard
        int wx = irange(150, 260);
        for (int y = g - 8; y < g; y++)
            for (int x = wx - 7; x <= wx + 7; x++)
                if (std::abs(x - wx) > 4 || y < g - 6) place(x, y, M::Masonry);
        for (int y = g - 26; y < g - 8; y++) { bgPut(wx - 6, y, {90, 60, 36, 255}); bgPut(wx + 6, y, {90, 60, 36, 255}); }
        for (int x = wx - 8; x <= wx + 8; x++) bgPut(x, g - 26, {110, 74, 44, 255});
    }

    auto overlaps = [&](const Room& b) {
        for (auto& o : rooms)
            if (b.x0 < o.x1 + 14 && b.x1 > o.x0 - 14 && b.y0 < o.y1 + 14 && b.y1 > o.y0 - 14) return true;
        return false;
    };
    auto corridor = [&](int xa, int fa, int xb, int fb) { // a tall passage whose floor slides from fa to fb
        int sg = xa < xb ? 1 : -1, n = std::abs(xb - xa);
        for (int k = 0; k <= n; k++)
        {
            int x = xa + k * sg;
            int f = fa + (fb - fa) * k / std::max(1, n);
            for (int y = f - 46; y <= f; y++)
                if (world.in(x, y) && world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
        }
    };

    // the keep above ground: halls end to end across its whole length, a storey of rooms over them (reached by shafts of
    // one-way ledges climbing out of the ground-floor halls), and at the far east end a vestibule where the grand staircase
    // starts - it goes down and comes back west under the keep. The chain below starts from the vestibule.
    std::vector<Room> upperRooms;
    {
        const int uf = g - 160; // the upper storey's floor
        std::vector<Room> ground{lobby};
        for (int x = lobby.x1 + 16; ; )
        {
            int w = irange(150, 290);
            if (x + w > keepX1 - 130) break;
            Room r{x, g - irange(120, 150), x + w, g - 1};
            clearRect(r.x0, r.y0, r.x1, r.y1);
            corridor(ground.back().x1 - 2, g - 1, r.x0 + 2, g - 1); // a doorway through the wall between halls
            decorateRoom(r, true);
            int sx = r.x0 + irange(20, std::max(21, w - 60)); // the climb: a shaft up through the ceiling, ledges every 26
            for (int y = uf + 1; y <= r.y0; y++)
                for (int xx = sx; xx <= sx + 40; xx++)
                    if (world.at(xx, y).material != M::Bedrock) world.at(xx, y) = Cell{};
            for (int y = g - 27; y > uf + 10; y -= 26)
                for (int xx = sx; xx <= sx + 40; xx++) place(xx, y, M::Platform);
            if (chance(2)) addChest(r.x0 + w / 2 + irange(-w / 4, w / 4), r.y1 + 1);
            ground.push_back(r);
            x += w + irange(16, 24);
        }
        for (auto& r : ground)
        {
            Room u{r.x0, uf - irange(46, 56), r.x1, uf};
            clearRect(u.x0, u.y0, u.x1, u.y1);
            if (!upperRooms.empty()) corridor(upperRooms.back().x1 - 2, uf, u.x0 + 2, uf);
            decorateRoom(u, false);
            upperRooms.push_back(u);
        }
        Room vest{ground.back().x1 + 16, g - 100, ground.back().x1 + 76, g - 1}; // undecorated: just torches and the stairs
        clearRect(vest.x0, vest.y0, vest.x1, vest.y1);
        corridor(ground.back().x1 - 2, g - 1, vest.x0 + 2, g - 1);
        G.inter.push_back({IT_TORCH, (float)vest.x0 + 10, (float)g});
        for (size_t i = 1; i < ground.size(); i++) rooms.push_back(ground[i]);
        rooms.push_back(vest);
    }
    bool stairDone = false;
    int dir = 1;
    for (int tries = 0; tries < 1000 && rooms.size() < 26 + 12; tries++)
    {
        const Room a = rooms.back();
        int w = irange(200, 380), h = irange(70, 150);
        Room b;
        bool stair = !stairDone; // the grand staircase: from the vestibule at the keep's east end, down and back west into the cellars
        bool vertical = !stair && chance(6);
        if (stair)
        {
            int desc = 3 * irange(37, 56); // a drop in whole 3-unit steps, descending 1:1
            h = irange(70, std::min(150, desc - 26));
            b.y1 = a.y1 + desc;
            b.y0 = b.y1 - h;
            b.x1 = a.x1 - 10 - desc + 30; // the cellar's east end meets the foot of the stairs
            b.x0 = b.x1 - w;
            if (b.x0 < 20) continue;
            dir = -1; // the cellars run on west, back under the keep
        }
        else if (!vertical)
        {
            int gap = irange(16, 50);
            b.x0 = dir > 0 ? a.x1 + gap : a.x0 - gap - w;
            b.x1 = b.x0 + w;
            b.y1 = a.y1 + irange(10, 60);
            b.y0 = b.y1 - h;
            if (b.x0 < 20 || b.x1 > W - 300) vertical = true;
        }
        if (vertical)
        {
            b.x0 = std::max(20, std::min(W - 300 - w, a.x0 + irange(-w / 2, a.x1 - a.x0 - w / 2)));
            b.x1 = b.x0 + w;
            b.y0 = a.y1 + irange(34, 70);
            b.y1 = b.y0 + h;
        }
        if (b.y0 < g + 24) continue; // everything after the great hall lies beneath the keep
        if (b.y1 > H - 40) break;
        if (overlaps(b)) { if (tries % 6 == 5) dir = -dir; continue; }
        clearRect(b.x0, b.y0, b.x1, b.y1);
        if (stair)
        {
            int xs = a.x1 - 10, n = b.y1 - a.y1; // a flight of 3x3 masonry steps from the vestibule floor to the cellar floor, under a high vault
            stairDone = true;
            for (int k = 0; k <= n; k++)
            {
                int x = xs - k, f = a.y1 + k / 3 * 3;
                for (int y = f - 54; y <= f; y++)
                    if (world.in(x, y) && world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
                place(x, f + 1, M::Masonry);
                world.at(x, f + 1).shade = (uint8_t)(k % 3 == 0 ? 220 : 150); // the lip of each tread catches the light
                if (k % 36 == 18) G.inter.push_back({IT_TORCH, (float)x, (float)(f + 1)});
            }
        }
        else if (vertical)
        {
            // a shaft through a's floor into b, capped with planks you drop through
            int s0 = std::max(a.x0, b.x0) + 6, s1 = std::min(a.x1, b.x1) - 6;
            if (s1 - s0 < 40) s1 = std::min(a.x1 - 2, s0 + 40);
            s1 = std::min(s1, s0 + 56);
            for (int y = a.y1 + 1; y < b.y0; y++)
                for (int x = s0; x <= s1; x++)
                    if (world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
            for (int x = s0; x <= s1; x++) { place(x, a.y1 + 1, M::Platform); place(x, a.y1 + 2, M::Platform); }
            for (int ly = a.y1 + 26; ly < b.y0 - 8; ly += 26)
                for (int x = s0; x <= s1; x++) place(x, ly, M::Platform);
            dir = chance(4) ? -1 : 1; // the castle's depths run, on the whole, away from the gate
        }
        else
        {
            int xa = dir > 0 ? a.x1 - 2 : a.x0 + 2, xb = dir > 0 ? b.x0 + 2 : b.x1 - 2;
            corridor(xa, a.y1, xb, b.y1);
            if (b.y1 - a.y1 > 40) // a long drop gets a ledge to catch you
                for (int x = std::min(xa, xb); x <= std::max(xa, xb); x++) place(x, (a.y1 + b.y1) / 2, M::Platform);
            if (chance(6)) dir = -dir; // the occasional doubling back
        }
        decorateRoom(b, w > 240 && h > 120);
        rooms.push_back(b);
    }
    // side halls: off any hall (side halls too), up, down, left or right - chains that wander off and end somewhere,
    // most with something left behind. The way on stays the main chain above.
    const Room last = rooms.back();
    for (auto& u : upperRooms) rooms.push_back(u); // (after the chain, so `last` above is its end)
    for (int tries = 0, added = 0; tries < 3000 && added < 34; tries++)
    {
        const Room a = rooms[1 + irand((int)rooms.size() - 1)]; // anything below the great hall
        int w = irange(110, 260), h = irange(60, 130), side = irand(4); // 0 east, 1 west, 2 down, 3 up
        Room b;
        if (side < 2)
        {
            int gap = irange(30, 70);
            b.x0 = side == 0 ? a.x1 + gap : a.x0 - gap - w;
            b.x1 = b.x0 + w;
            b.y1 = a.y1 + irange(-gap * 4 / 5, 50); // up no steeper than you can walk; down, any drop
            b.y0 = b.y1 - h;
        }
        else
        {
            b.x0 = std::max(20, std::min(W - 300 - w, a.x0 + irange(-w / 2, a.x1 - a.x0 - w / 2)));
            b.x1 = b.x0 + w;
            if (side == 2) { b.y0 = a.y1 + irange(34, 80); b.y1 = b.y0 + h; }
            else { b.y1 = a.y0 - irange(34, 80); b.y0 = b.y1 - h; }
        }
        if (b.x0 < 20 || b.x1 > W - 300 || b.y0 < g + 24 || b.y1 > H - 40 || overlaps(b)) continue;
        if (side >= 2 && std::min(a.x1, b.x1) - std::max(a.x0, b.x0) < 42) continue; // too little overlap for a shaft
        clearRect(b.x0, b.y0, b.x1, b.y1);
        if (side < 2)
        {
            int xa = side == 0 ? a.x1 - 2 : a.x0 + 2, xb = side == 0 ? b.x0 + 2 : b.x1 - 2;
            corridor(xa, a.y1, xb, b.y1);
            if (std::abs(b.y1 - a.y1) > 40)
                for (int x = std::min(xa, xb); x <= std::max(xa, xb); x++) place(x, (a.y1 + b.y1) / 2, M::Platform);
        }
        else // a planked shaft between them, the upper room's floor dropping into the lower
        {
            const Room& up = side == 2 ? a : b;
            const Room& dn = side == 2 ? b : a;
            int s0 = std::max(up.x0, dn.x0) + 6, s1 = std::min(s0 + 50, std::min(up.x1, dn.x1) - 6);
            for (int y = up.y1 + 1; y < dn.y0; y++)
                for (int x = s0; x <= s1; x++)
                    if (world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
            for (int x = s0; x <= s1; x++) { place(x, up.y1 + 1, M::Platform); place(x, up.y1 + 2, M::Platform); }
            for (int ly = up.y1 + 26; ly < dn.y0 - 8; ly += 26)
                for (int x = s0; x <= s1; x++) place(x, ly, M::Platform);
        }
        decorateRoom(b, w > 200 && h > 110);
        if (chance(2)) addChest((b.x0 + b.x1) / 2 + irange(-w / 4, w / 4), b.y1 + 1); // what someone left down here
        rooms.push_back(b);
        added++;
    }
    // down from the last hall of the main chain to the crypt gate at the castle's foot
    int hf = std::min(H - 24, std::max(last.y1 + 1, (int)(H * 0.68f)));
    carveRamp((float)last.x1 - 16, (float)last.y1 - 22, (float)(W - 236), (float)(hf - 22), 23);
    // line every hall and passage with masonry, three bricks deep
    {
        std::vector<uint8_t> near((size_t)W * H, 0);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) near[(size_t)y * W + x] = world.at(x, y).material == M::Empty;
        for (int pass = 0; pass < 3; pass++)
        {
            std::vector<uint8_t> nx(near);
            for (int y = 1; y < H - 1; y++)
                for (int x = 1; x < W - 1; x++)
                    if (near[(size_t)y * W + x - 1] || near[(size_t)y * W + x + 1] || near[(size_t)(y - 1) * W + x] || near[(size_t)(y + 1) * W + x])
                        nx[(size_t)y * W + x] = 1;
            near.swap(nx);
        }
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (near[(size_t)y * W + x] && world.at(x, y).material == M::Basalt) place(x, y, M::Masonry);
    }
    LevelEnds e{};
    e.sx = 30;
    e.sy = g;
    e.ex = W - 220;
    e.ey = hf;
    path.clear();
    pathFloor.assign(W, -1);
    return e;
}

// Sealed pockets the player is meant to dig into: connectPockets leaves these alone.
static std::vector<Rectangle> keepZones;
static bool inKeepZone(int x, int y)
{
    for (auto& z : keepZones)
        if (x >= z.x && x < z.x + z.width && y >= z.y && y < z.y + z.height) return true;
    return false;
}

// Any cave pocket the player can't walk/fall into from the road is either filled in (if small)
// or connected to the nearest stretch of road with a tunnel, so nothing is unreachable
// (except the sealed pockets above, which are meant to be dug into).
// Roomy tunnels you can fight in (the hero is ~23 tall), now and then a tighter squeeze.
static int tunnelRadius() { return chance(5) ? irange(10, 11) : irange(15, 19); }

// A natural, wandering tunnel (no switchbacks) from one cave to another, kept well below the surface.
static void carveTunnel(float x, float y, float tx, float ty, int r)
{
    // a straight line bent sideways by smooth noise, so it snakes like a natural passage
    float dx = tx - x, dy = ty - y, len = std::sqrt(dx * dx + dy * dy) + 0.01f;
    float nx = -dy / len, ny = dx / len, amp = std::min(28.0f, len * 0.18f);
    int key = seed + irand(1000), steps = (int)(len / 1.5f) + 1;
    for (int i = 0; i <= steps; i++)
    {
        float t = (float)i / steps;
        float bend = (fbm(t * len * 0.012f, 0.5f, key, 3) - 0.5f) * 2 * amp * std::sin(3.14159f * t); // pinned at both ends
        float px = x + dx * t + nx * bend, py = y + dy * t + ny * bend;
        if (!surf.empty() && px >= 0 && px < W) py = std::max(py, (float)surf[(int)px] + 75 + r); // stay clear of cellars and the road
        carveWorld(px, py, r + (fbm(px * 0.05f, py * 0.05f, seed + 70, 2) > 0.55f)); // bulges here and there
    }
}

// caveNet: instead of ramps up to the road, each pocket is joined to the deep cave network (cells of the
// start region at least 60 below the surface), so the caves form one system whose only way in is from above.
static void connectPockets(int sx, int sy, M fillMat, size_t minConnect, bool caveNet = false)
{
    std::vector<int> lab((size_t)W * H, 0), q;
    auto open = [&](int x, int y) { return world.in(x, y) && !isSolid(x, y); };
    auto flood = [&](int x0, int y0, int id, std::vector<int>* cells) {
        q.clear();
        q.push_back(y0 * W + x0);
        lab[(size_t)y0 * W + x0] = id;
        for (size_t h = 0; h < q.size(); h++)
        {
            int k = q[h], x = k % W, y = k / W;
            if (cells) cells->push_back(k);
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; d++)
            {
                int nx = x + dx[d], ny = y + dy[d];
                if (!open(nx, ny) || lab[(size_t)ny * W + nx]) continue;
                lab[(size_t)ny * W + nx] = id;
                q.push_back(ny * W + nx);
            }
        }
    };
    if (!open(sx, sy)) return;
    flood(sx, sy, 1, nullptr);
    std::vector<int> net; // sampled cells of the deep cave network
    auto deep = [&](int k) { return k / W > surf[k % W] + 75; };
    if (caveNet)
        for (int y = 5; y < H - 5; y += 3)
            for (int x = 5; x < W - 5; x += 3)
                if (lab[(size_t)y * W + x] == 1 && deep(y * W + x)) net.push_back(y * W + x);
    int id = 2;
    for (int y = 5; y < H - 5; y++)
        for (int x = 5; x < W - 5; x++)
        {
            if (!open(x, y) || lab[(size_t)y * W + x]) continue;
            std::vector<int> cells;
            flood(x, y, id++, &cells);
            if (inKeepZone(x, y)) continue;
            if (cells.size() < minConnect || (caveNet ? net.empty() : path.empty()))
            {
                for (int k : cells) place(k % W, k / W, fillMat);
                continue;
            }
            if (caveNet)
            {
                int c = cells[cells.size() / 2], cx = c % W, cy = c / W, best = net[0];
                long long bd = 1LL << 40;
                for (int k : net)
                {
                    long long ddx = k % W - cx, ddy = k / W - cy;
                    long long d2 = ddx * ddx + 4 * ddy * ddy; // prefer sideways links: they stay walkable
                    if (d2 < bd) { bd = d2; best = k; }
                }
                carveTunnel((float)cx, (float)cy, (float)(best % W), (float)(best / W), tunnelRadius());
                for (size_t i = 0; i < cells.size(); i += 9)
                    if (deep(cells[i])) net.push_back(cells[i]); // later caves can branch off this one
                continue;
            }
            int c = cells[cells.size() / 2], cx = c % W, cy = c / W;
            Vector2 best = path[0];
            float bd = 1e9f;
            for (size_t i = 0; i < path.size(); i += 4)
            {
                if (inFortZone((int)path[i].x)) continue; // don't tunnel up into a building
                float d = (path[i].x - cx) * (path[i].x - cx) + (path[i].y - cy) * (path[i].y - cy);
                if (d < bd) { bd = d; best = path[i]; }
            }
            float len = std::sqrt(bd);
            (void)len;
            carveRamp((float)cx, (float)cy, best.x, best.y, 12); // a walkable way in and out
        }
}

// ---------------------------------------------------------------- the old mine (plains, one per run)

struct Chamber { int x, floor, rx, ry; };

// A dome with a flat floor: rows [floor - ry, floor) inside the half-ellipse are cleared, the floor made solid.
static void carveChamber(const Chamber& c)
{
    for (int y = c.floor - c.ry; y < c.floor; y++)
        for (int x = c.x - c.rx; x <= c.x + c.rx; x++)
        {
            float u = (float)(x - c.x) / c.rx, v = (float)(c.floor - y) / c.ry;
            if (u * u + v * v > 1 || x < 5 || y < 5 || x > W - 6 || y > H - 6) continue;
            if (world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
        }
    for (int x = c.x - c.rx; x <= c.x + c.rx; x++)
        for (int y = c.floor; y < c.floor + 3; y++)
            if (world.in(x, y) && world.at(x, y).material == M::Empty) place(x, y, M::Dirt);
}

static bool allRock(int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r && !isRock(cx + dx, cy + dy)) return false;
    return true;
}

// A sealed hollow in solid rock near a cave, reached only by digging (or mining out the gold seam that points to it).
static bool placeSealedPocket()
{
    int pr = irange(24, 28); // room to stand
    Chamber p{irange(G.duneEnd + 60, W - 340), 0, pr + 4, pr};
    p.floor = irange(surf[p.x] + 120, H - 30);
    int cy = p.floor - pr / 2;
    if (!allRock(p.x, cy, pr + 8)) return false;
    int ox = -1, oy = 0; // the nearest open cave cell, within a short dig
    for (int k = 0; k < 400 && ox < 0; k++)
    {
        float a = frange(0, 6.2832f), d = frange(pr + 9.0f, pr + 26.0f);
        int x = p.x + (int)(std::cos(a) * d), y = cy + (int)(std::sin(a) * d);
        if (world.in(x, y) && world.at(x, y).material == M::Empty) { ox = x; oy = y; }
    }
    if (ox < 0) return false;
    carveChamber(p);
    keepZones.push_back({(float)(p.x - p.rx - 2), (float)(p.floor - pr - 2), (float)(p.rx * 2 + 5), (float)(pr + 3)});
    for (int k = 0; k < 40; k++) // a seam of gold from the cave wall towards the hollow
    {
        float t = frand();
        int x = ox + (int)((p.x - ox) * t) + irange(-2, 2), y = oy + (int)((cy - oy) * t) + irange(-2, 2);
        if (isRock(x, y)) place(x, y, M::GoldOre);
    }
    int side = ox < p.x ? -1 : 1;
    addChest(p.x + irange(-3, 3), p.floor);
    paintSkeleton(p.x - side * (p.rx - 6), p.floor, side);
    if (chance(4)) addPickupWeapon((float)p.x + side * 6, (float)p.floor - 8, randomWeapon(1));
    paintCobweb(p.x - p.rx / 2, p.floor - pr + 6, 1);
    return true;
}

// A timber headframe over a shaft; a rope down into a string of caverns with loot (and the dead who came for it).
static int mineX = -1; // keeps trees off the headframe
static void placeMine(int mx)
{
    mineX = mx;
    int g = surf[mx];
    levelGround(mx - 30, mx + 30, g);
    int bottom = std::min(H - 70, g + irange(170, 210));
    const Color timber = {112, 76, 44, 255}, iron = {70, 70, 78, 255};
    for (int y = g; y < bottom; y++) // the shaft, timbered on the back wall
        for (int x = mx - 10; x <= mx + 10; x++)
        {
            world.at(x, y) = Cell{};
            if (x == mx - 10 || x == mx + 10 || (y - g) % 20 < 2) bgPut(x, y, shadeC(timber, (y - g) % 20 == 1 ? 0.7f : 1.0f));
        }
    for (int y = g + 22, sd = 1; y < bottom - 16; y += 30, sd = -sd) addSconce(mx + sd * 8, y, sd); // torches down the shaft
    for (int x = mx - 11; x <= mx + 11; x++) { place(x, g, M::Platform); place(x, g + 1, M::Platform); } // plank cover: S drops through
    for (int y = g - 42; y < g; y++) // headframe legs leaning in
        for (int k = 0; k < 3; k++)
        {
            bgPut(mx - 15 + k + (g - y) / 8, y, shadeC(timber, k ? 1.0f : 0.7f));
            bgPut(mx + 13 + k - (g - y) / 8, y, shadeC(timber, k == 2 ? 0.7f : 1.0f));
        }
    for (int x = mx - 12; x <= mx + 12; x++)
        for (int y = g - 44; y < g - 40; y++) bgPut(x, y, shadeC(timber, y == g - 41 ? 0.7f : 1.0f));
    for (int dy = -7; dy <= 7; dy++) // winding wheel
        for (int dx = -7; dx <= 7; dx++)
        {
            int d2 = dx * dx + dy * dy;
            if (d2 <= 49 && (d2 >= 30 || dx == 0 || dy == 0 || d2 <= 2)) bgPut(mx + dx, g - 50 + dy, iron);
        }
    for (int y = g - 50; y < g; y++) bgPut(mx, y, {150, 120, 76, 255}); // the rope, up to the wheel
    Interact rope{IT_ROPE, (float)mx, (float)g};
    rope.data = bottom;
    G.inter.push_back(rope);

    std::vector<Chamber> ch{{mx, bottom, irange(48, 60), irange(44, 54)}};
    for (int side : {-1, 1})
    {
        Chamber prev = ch[0];
        for (int k = irange(2, 3); k > 0; k--)
        {
            Chamber c{prev.x + side * irange(130, 180), 0, irange(42, 62), irange(40, 54)};
            if (c.x < G.duneEnd + 70 || c.x > W - 380) break;
            c.floor = std::max(surf[c.x] + 130, std::min(H - 30, prev.floor + irange(-25, 45)));
            int r = tunnelRadius();
            carveRamp(prev.x + side * prev.rx * 0.7f, (float)(prev.floor - r - 1), c.x - side * c.rx * 0.7f, (float)(c.floor - r - 1), r);
            ch.push_back(c);
            prev = c;
        }
    }
    for (size_t i = 0; i < ch.size(); i++)
    {
        const Chamber& c = ch[i];
        carveChamber(c);
        for (int sd : {-1, 1}) // webs in the upper corners
        {
            int wx = c.x + sd * c.rx / 2, wy = c.floor - 2;
            while (wy > c.floor - c.ry - 4 && world.at(wx, wy - 1).material == M::Empty) wy--;
            paintCobweb(wx, wy, -sd);
        }
        if (i == 0) { addSconce(c.x + 26, c.floor - 16, -1); addSconce(c.x - 26, c.floor - 16, 1); continue; } // the landing under the rope stays clear
        int lx = c.x + irange(-c.rx / 3, c.rx / 3);
        if (chance(8)) addChest(lx, c.floor);
        else if (chance(4))
        {
            paintSkeleton(lx - 10, c.floor, 1); // someone didn't make it back up
            addPickupWeapon((float)lx + 8, (float)c.floor - 8, randomWeapon(1));
        }
        if (chance(3)) paintSkeleton(c.x + (lx < c.x ? 1 : -1) * c.rx / 2, c.floor, lx < c.x ? -1 : 1);
        if (chance(2)) addCoins((float)c.x, (float)c.floor - 4, irange(2, 4), 1);
        placeMineSupport(c.x + irange(-6, 6), c.floor);
    }
}

// An archery butt out in the fields, with a rack of longbows beside it, painted on the back wall.
static void paintArchery(int x, int fy)
{
    addDecor(DK_TARGET, (float)x + 16, (float)fy + 1, irand(8)); // the butt: a side-on straw drum on a tripod, arrows in it, hay bales behind
    { // the longbow rack: dark timber, bows in two rows, a quiver of arrows - painted into the back wall (so the grass grows in front of it), a unit per 2x2 of its sprite
        Image im = decorImageFine(DK_BOWRACK, irand(8), 0);
        const Color* src = (const Color*)im.data;
        int ux0 = x - 18 - im.width / 4, uy0 = fy + 1 - im.height / 2;
        for (int uy = 0; uy < im.height / 2; uy++)
            for (int ux = 0; ux < im.width / 2; ux++)
            {
                int n = 0, r = 0, g = 0, b = 0;
                for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { const Color& c = src[(uy * 2 + j) * im.width + ux * 2 + i]; if (c.a > 128) { n++; r += c.r; g += c.g; b += c.b; } }
                if (n < 2 || !world.in(ux0 + ux, uy0 + uy)) continue;
                world.bgAt(ux0 + ux, uy0 + uy) = Color{(unsigned char)(r / n * 0.86f), (unsigned char)(g / n * 0.86f), (unsigned char)(b / n * 0.9f), 255};
                world.skyAt(ux0 + ux, uy0 + uy) = 0;
            }
        UnloadImage(im);
    }
}

// ---------------------------------------------------------------- the Whispering Dunes

static void paintCactus(int x, int fy)
{
    G.inter.push_back({IT_DECOR, (float)x, (float)fy, false, DK_CACTUS, irand(64) | (chance(2) ? 64 : 0)});
}

static void paintDeadBush(int x, int fy)
{
    G.inter.push_back({IT_DECOR, (float)x, (float)fy, false, DK_BUSH, irand(64) | (chance(2) ? 64 : 0)});
}

// The bones of a giant, half swallowed by the sand: a skull, a spine and a cage of ribs.
static void paintGiantBones(int x, int fy)
{
    G.inter.push_back({IT_DECOR, (float)x + 50, (float)fy + 4, false, DK_GIANT, irand(64)}); // a sprite, half sunk in the sand
}

// One of the levy that died before Dunmoor, painted on the back wall: sprawled in mail or a tunic, spears
// and arrows standing out of it, maybe a broken shield beside it or a torn banner over it.
static void paintFallen(int x, int g, float k)
{
    static const Color TUNIC[3] = {{120, 30, 34, 255}, {40, 52, 96, 255}, {92, 78, 52, 255}};
    const Color tunic = TUNIC[irand(3)], skin = {150, 132, 118, 255}, mail = {104, 106, 116, 255}, legs = {62, 52, 42, 255}, wood = {104, 74, 46, 255};
    int dir = chance(2) ? 1 : -1, y = g - 1;
    bool helm = chance(2);
    auto p = [&](int dx, int dy, Color c) { int xx = x + dir * dx, yy = y - dy; if (world.in(xx, yy) && world.at(xx, yy).material == M::Empty) bgPut(xx, yy, c); };
    for (int dx = -3; dx < 16; dx++) p(dx, 0, shadeC({96, 16, 18, 255}, 0.8f + 0.3f * hash2(x + dx, y, seed))); // the blood under it
    for (int dx = 0; dx < 3; dx++) for (int dy = 0; dy < 3; dy++) p(dx, dy, helm && dy > 0 ? mail : skin); // head
    for (int dx = 3; dx < 10; dx++) for (int dy = 0; dy < 3; dy++) p(dx, dy, shadeC(chance(4) ? mail : tunic, 0.75f + 0.12f * dy));
    p(5, 3, skin); p(6, 4, skin); p(7, 4, tunic); // an arm flung up
    for (int dx = 10; dx < 17; dx++) for (int dy = 0; dy < 2; dy++) p(dx, dy, dx > 14 ? Color{40, 30, 24, 255} : legs);
    for (int s = irange(0, 1) + (k > 0.5f) + chance(2); s > 0; s--) // spears driven in, standing at a slant
    {
        float a = -PI / 2 + frange(-0.55f, 0.55f), len = (float)irange(16, 28);
        int bx = irange(3, 9);
        for (int t = 0; t < (int)len; t++) p(bx + (int)std::lround(std::cos(a) * t) * dir, 1 + (int)std::lround(-std::sin(a) * t), t > len - 3 ? Color{160, 160, 170, 255} : wood);
        if (chance(4)) // a tattered pennon on it
            for (int t = 0; t < 7; t++) for (int h = 0; h < 4 - t / 2; h++) p(bx + (int)std::lround(std::cos(a) * (len - 4 - h)) * dir + t * dir, 1 + (int)std::lround(-std::sin(a) * (len - 4 - h)), shadeC(TUNIC[0], 0.8f));
    }
    for (int s = irange(0, 3); s > 0; s--) // arrows
    {
        float a = -PI / 2 + frange(-0.8f, 0.8f);
        int bx = irange(2, 12);
        for (int t = 0; t < 7; t++) p(bx + (int)std::lround(std::cos(a) * t) * dir, 1 + (int)std::lround(-std::sin(a) * t), t > 4 ? Color{220, 214, 200, 255} : Color{130, 98, 64, 255});
    }
}

// The battlefield before Dunmoor: the dead lie thicker the nearer the walls, the ground drinks their blood,
// and the levy that fell here has risen to hold the field.
static void decorateBattlefield(int x0, int x1, bool thinning = false, bool storm = true) // thinning: the dead are thickest at x0 and fade away towards x1 (the far side of the waystone)
{
    int nextBody = x0;
    for (int x = x0; x < x1; x++)
    {
        float k = thinning ? clampf(1.05f - (x - x0) / (float)(x1 - x0) * 0.85f, 0.15f, 1) : clampf((x - x0) / (float)(x1 - x0) * 1.4f, 0, 1);
        int gy = 5;
        while (gy < H - 5 && !isSolid(x, gy)) gy++;
        if (gy >= H - 5) continue;
        if (0.6f * fbm(x * 0.04f, 3, seed + 21, 3) + 0.4f * hash2(x, 0, seed) < 0.25f + k * 0.65f) // the soaked ground
            for (int d = 0, n = irange(1, 3) + (int)(k * 3); d < n; d++)
            {
                M gm = world.at(x, gy + d).material;
                if (gm == M::Grass || gm == M::Dirt) { place(x, gy + d, M::BloodEarth); world.at(x, gy + d).shade = (uint8_t)irand(256); }
            }
        if (k > 0.2f && irand(90) == 0) // blood pooling in the ruts
            for (int xx = x; xx < x + irange(4, 10) && xx < x1; xx++)
                if (world.at(xx, gy - 1).material == M::Empty && isSolid(xx, gy)) place(xx, gy - 1, M::Blood);
        if (x >= nextBody)
        {
            paintFallen(x, gy, k);
            nextBody = x + (int)(irange(16, 34) / (0.45f + k));
        }
        if (k > 0.1f && irand(14) == 0) addDecor(DK_SPEARPOST, (float)x, (float)gy + 1, irand(32) | (chance(20) ? 32 : 0), irange(22, 34), chance(2)); // a spear driven into the ground; one in twenty has a pennon streaming from it
        if (k > 0.05f && irand(110) == 0) // the risen levy, with the odd skeleton among them
        {
            Mob mb = makeEnemy(chance(4) ? E_SKELETON : E_RISEN, (float)x, (float)gy);
            if (!boxSolid(mb.x, mb.y, mb.w, mb.h)) G.mobs.push_back(mb);
        }
    }
    if (!storm) return;
    G.stormX0 = (float)x0 - 250;
    G.stormX1 = (float)x1 + 80;
}

static void decorateDunes(int D)
{
    G.inter.push_back({IT_BOAT, 66, (float)surf[66] + 2, true}); // the longship you came in on, run up on the beach
    for (int x = 160; x < D - 20; x += irange(40, 110))
    {
        int fy;
        if (!findFloor(x, 10, fy)) continue;
        if (chance(3)) paintDeadBush(x, fy);
        else paintCactus(x, fy);
    }
    int gx = irange(D / 2 - 80, D / 2 + 40), gy;
    if (findFloor(gx + 40, 10, gy)) paintGiantBones(gx, gy);
    if (chance(3)) // very rarely, someone's lost chest half-buried in the sand
    {
        int cx = irange(260, D - 120), cy;
        if (findFloor(cx, 10, cy)) addChest(cx, cy + 3);
    }
}

// ---------------------------------------------------------------- waystones: where one biome gives way to the next

void setGate(int x0, int x1, int y0, int y1, bool closed)
{
    int k = world.scale; // in world units, over every cell
    for (int y = y0 * k; y < (y1 + 1) * k; y++)
        for (int x = x0 * k; x < (x1 + 1) * k; x++)
        {
            if (!world.in(x, y)) continue;
            Cell& c = world.at(x, y);
            if (!closed) { if (c.material == M::Metal) c = Cell{}; continue; }
            int ux = x / k - x0, uy = y / k - y0;
            if (ux % 3 == 1 && uy % 7 != 0) continue; // iron bars: gaps you can see through, not squeeze through
            c = Cell{};
            c.material = M::Metal;
            c.shade = (uint8_t)(ux % 3 == 0 ? (x % k ? 160 : 220) : 120); // each bar lit down one side
        }
}

struct HavenTheme { const char* name; M wall; Color back, accent; };
static const HavenTheme HAVEN_THEMES[] = {
    {"Dunmoor Gatehouse", M::Masonry, {58, 46, 44, 255}, {150, 34, 40, 255}},
    {"The Crypt Gate", M::Stone, {44, 42, 50, 255}, {196, 186, 156, 255}},
    {"The Dwarven Waystation", M::Stone, {54, 46, 40, 255}, {204, 160, 70, 255}},
    {"The Frozen Hall", M::Stone, {52, 64, 82, 255}, {170, 220, 250, 255}},
    {"The Ember Gate", M::Basalt, {58, 34, 28, 255}, {240, 110, 40, 255}},
    {"The Black Chapel", M::Obsidian, {30, 24, 40, 255}, {150, 90, 200, 255}},
};

// The first waystone is a chieftain's hall: one long timber hut with a thatched roof you walk straight through,
// doorways at both ends, idols flanking the doors, trophies and shields on the walls, furs and a rack of spears.
// The runestone, the anvil and the mead horn stand inside it.
static void placeTribalHut(int x0, int x1, int floor)
{
    const int hx = x0 + 8, w = x1 - x0 - 24, wallTop = floor - 58;
    Hall hall = paintHall(hx, w, floor, wallTop, true);
    paintRoom(hx + 3, wallTop + 1, hx + w - 2, floor);
    for (int x = hx + 24; x < hx + w - 40; x += irange(34, 50)) // trophies and shields on the long wall, banners between
    {
        int k = irand(4);
        if (k == 0) antlerSkull(x, wallTop + irange(10, 18));
        else if (k == 1) paintShield(x, wallTop + 14, 4);
        else if (k == 2) triskeleBanner(x, wallTop + 4, 26);
        else addDecor(DK_TAPESTRY, (float)x, (float)(wallTop + 5), irand(64), irange(26, 40), chance(2));
    }
    for (int x = hx + 20; x < hx + w - 20; x += irange(44, 64)) paintPost(x, wallTop + 2, floor);
    for (int sd : {-1, 1}) // idols either side of each doorway
    {
        idolPillar(sd < 0 ? hx + 14 : hx + w - 14, floor - 70, floor);
        paintRack(sd < 0 ? hx + 22 : hx + w - 44, floor);
    }
    for (int x = hx + 40; x < hx + w - 40; x += irange(60, 80)) addDecor(DK_CHAIN, (float)x, (float)(wallTop + 2), irand(64), irange(10, 20)); // pot-chains from the beams
    for (int k = 0; k < 3; k++) addDecor(DK_BLOOD, (float)irange(hx + 30, hx + w - 30), (float)(floor + 1), 4 + 6 * irand(10), irange(10, 16)); // a hunt's worth of blood on the floor
    paintYardProp(hx - 12, floor);
    paintYardProp(hx + w + 3, floor);
    for (int y = wallTop; y < floor; y++) // the end walls: a wide doorway below, a smoke hole above
    {
        if (y >= floor - 30) continue;
        for (int k = 0; k < 3; k++) { place(hx + k, y, M::Wood); place(hx + w - k, y, M::Wood); }
    }
    for (int x = hx; x <= hx + w; x++) { place(x, floor, M::Wood); place(x, floor + 1, M::Wood); } // planked floor
    Lamp smoke{(float)hall.mid, (float)hall.apexY + 2, 0, BLANK};
    smoke.smoke = true;
    G.lamps.push_back(smoke);
    G.lamps.push_back({(float)hall.mid, (float)(floor - 6), 120, LAMP_WARM, true});
}

// No walls and no gates: the way simply runs on into the next biome, whose rock and back wall bleed into
// this one's (see compose). At the seam stands a runestone, glowing in the stage's colour, an anvil, and a
// horn of mead that heals you whole. Walking past it builds the biome after next. Only a guardian's
// waystone is barred (buildStage), by a portcullis in a stone plug, until the guardian falls.
static void placeHaven(int s, int floor)
{
    const HavenTheme& t = HAVEN_THEMES[std::min(s, 5)];
    const StageDef& d = STAGES[s];
    const int x0 = W - wingR - 220, x1 = W - wingR - 1; // (a deep biome carries on east past it, over the next)
    floor = std::max(120, std::min(floor, H - 16));
    int top = floor - 76;
    if (d.surface) { levelGround(x0 - 120, W - 1, floor); placeTribalHut(x0, x1, floor); } // (the runestone, anvil and horn below stand inside it)
    else
    {
        if (roadValid) // a broad tunnel from wherever the road ended up
            carveRamp((float)x0 - 70, (float)std::max(40, pathFloor[x0 - 70] - 22), (float)x0 + 4, (float)floor - 23, 23);
        for (int x = x0; x <= x1; x++) // a cave clearing under a rough arch, its floor made good
        {
            float k = (float)(x - x0) / (x1 - x0);
            int roof = top + (int)(22 * (1 - std::sin(k * PI))) + (int)(fbm(x * 0.08f, 0, seed + 7, 2) * 6);
            for (int y = roof; y < floor; y++) world.at(x, y) = Cell{};
            for (int y = floor; y < floor + 6; y++) if (!isSolid(x, y)) place(x, y, d.base);
        }
    }
    for (int y = d.surface ? 5 : floor - HAVEN_DOOR; y < floor; y++) // the edge's bedrock gives way, to meet the next biome
        for (int x = W - 6; x < W && !wingR; x++) world.at(x, y) = Cell{};
    int sx = x0 + 110; // the runestone, its carving lit from within
    for (int y = floor - 26; y < floor; y++)
        for (int x = sx - 6; x <= sx + 6; x++)
        {
            float u = (x - sx) / 6.5f, v = (floor - y) / 26.0f;
            if (u * u + (v > 0.75f ? (v - 0.75f) * (v - 0.75f) * 14 : 0) > 1) continue;
            bool rune = std::fabs((x - sx) - std::sin((floor - y) * 0.5f) * 3) < 0.8f || ((floor - y) % 7 == 3 && std::abs(x - sx) < 3);
            bgPut(x, y, rune ? t.accent : shadeC({120, 118, 112, 255}, 0.7f + 0.3f * hash2(x, y / 2, seed + 3)));
        }
    G.lamps.push_back({(float)sx, (float)floor - 14, 64, t.accent});
    G.inter.push_back({IT_ANVIL, (float)x0 + 64, (float)floor});
    G.inter.push_back({IT_TORCH, (float)x0 + 26, (float)floor});
    G.inter.push_back({IT_TORCH, (float)x1 - 30, (float)floor});
    addPickup((float)sx + 26, (float)floor - 12, PU_MEAD);
    Haven h;
    h.stage = s;
    h.x0 = x0;
    h.x1 = x1;
    h.top = top;
    h.floor = floor;
    h.name = t.name;
    G.havens.push_back(h);
}

// Builds one biome on its own (into `world` and the G lists), ready to be stitched into the running world:
// entered from its left edge at `pieceEntryX/Floor`, left through the haven at its right edge.
// `entryFloor` asks for a particular entry height (the castle grounds meet the gatehouse's floor).
static int pieceEntryX = 0, pieceEntryFloor = 0;
// ---------------------------------------------------------------- dressing: the fine detail, added last

// A floor cell whose neighbours are floor at the same height: loose powder laid on it stays put.
static bool flatFloor(int x, int y)
{
    auto rock = [](int x, int y) { return props(world.at(x, y).material).kind == Kind::Solid; }; // (not powder: snow already laid next door is fine)
    for (int dx : {-1, 1})
        if (!rock(x + dx, y) || rock(x + dx, y - 1)) return false;
    return true;
}

static bool plainRock(M m) { return m == M::Stone || m == M::Dirt || m == M::Basalt || m == M::Obsidian || m == M::Ice || m == M::Moss || m == M::Grass; }

// What grows on, drips from and splits the rock of each biome: moss and pale cave mushrooms in the plains
// and mines, frost and solid icicles below, ash in the forge, bone dust in the crypts; stalactites over
// big caverns, and cracks running into the walls everywhere.
static void dressCaves(const StageDef& d)
{
    bool green = d.kind == SK_PLAINS || d.kind == SK_MINES, frost = d.hang == 1, forge = d.hang == 2, crypt = d.kind == SK_CRYPT;
    M spike = frost ? M::Ice : (forge ? M::Obsidian : d.base);
    if (spike == M::Brick || spike == M::Masonry) spike = M::Stone;
    int lastSpike = -99;
    for (int x = 6; x < W - 6; x++)
        for (int y = 6; y < H - 6; y++)
        {
            if (d.surface && y < surf[x] + 10) continue; // the open air above ground is dressed by hand
            M m = world.at(x, y).material;
            if (!plainRock(m)) continue;
            bool openUp = world.at(x, y - 1).material == M::Empty, openDown = world.at(x, y + 1).material == M::Empty;
            bool openSide = world.at(x - 1, y).material == M::Empty || world.at(x + 1, y).material == M::Empty;
            if (openUp) // a floor
            {
                if (green && chance(3)) { place(x, y, M::Moss); if (chance(2)) place(x, y + 1, M::Moss); }
                if (green && chance(4)) // tufts of cave grass, painted behind
                    for (int k = 1; k <= irange(1, 3); k++) bgPut(x, y - k, shadeC({70, 120, 52, 255}, 1.0f - k * 0.15f));
                if (green && chance(70) && world.at(x, y - 5).material == M::Empty) // a glowing mushroom
                {
                    Color cap = chance(2) ? Color{120, 220, 210, 255} : Color{200, 150, 230, 255};
                    for (int k = 1; k <= 3; k++) bgPut(x, y - k, {200, 196, 176, 255});
                    for (int dx = -2; dx <= 2; dx++) bgPut(x + dx, y - 4, cap);
                    for (int dx = -1; dx <= 1; dx++) bgPut(x + dx, y - 5, brighten(cap, 30));
                    G.lamps.push_back({(float)x, (float)(y - 4), 22, cap});
                }
                if (frost && chance(3) && flatFloor(x, y)) place(x, y - 1, M::Snow);
                if (forge && chance(8)) place(x, y - 1, M::Gravel);
                if (crypt && chance(40)) place(x, y - 1, M::Bone);
                if (!frost && chance(25)) place(x, y - 1, M::Gravel); // loose pebbles
            }
            if (openDown) // a ceiling
            {
                int clear = 0;
                while (clear < 40 && world.at(x, y + 1 + clear).material == M::Empty) clear++;
                if (clear >= 40 && x - lastSpike > 6 && chance(18)) // a stalactite over open space
                {
                    int len = irange(4, 9);
                    for (int k = 0; k < len; k++)
                        for (int dx = -(len - k) / 3; dx <= (len - k) / 3; dx++)
                            if (world.at(x + dx, y + 1 + k).material == M::Empty) place(x + dx, y + 1 + k, spike);
                    lastSpike = x;
                }
                else if (green && chance(5)) // moss dripping from the roof
                    for (int k = 1; k <= irange(2, 6); k++) bgPut(x, y + k, shadeC({58, 100, 44, 255}, 1.0f - k * 0.08f));
            }
            if ((openUp || openSide) && chance(140)) // a crack running back into the rock
            {
                float cx = (float)x, cy = (float)y, a = frange(0, 6.283f);
                for (int k = irange(6, 18); k > 0; k--)
                {
                    a += frange(-0.6f, 0.6f);
                    cx += std::cos(a); cy += std::sin(a);
                    int ix = (int)cx, iy = (int)cy;
                    if (!world.in(ix, iy) || !plainRock(world.at(ix, iy).material)) break;
                    world.at(ix, iy).shade = (uint8_t)irand(20);
                }
            }
        }
}

// Planks you can drop through should never be mistaken for the floor: bright boards with seams, nails
// and a shadowed underside, held up by posts to the floor below or ropes from the roof above.
static void dressPlatforms()
{
    for (int y = 1; y < H - 2; y++)
        for (int x = 0; x < W; x++)
        {
            if (world.at(x, y).material != M::Platform || world.at(x, y - 1).material == M::Platform) continue;
            if (x > 0 && world.at(x - 1, y).material == M::Platform) continue; // only the start of each run
            int x1 = x;
            while (x1 + 1 < W && world.at(x1 + 1, y).material == M::Platform) x1++;
            for (int xx = x; xx <= x1; xx++)
            {
                int seam = (xx - x) % 8;
                world.at(xx, y).shade = seam == 0 ? 20 : (seam == 2 && chance(2) ? 255 : (uint8_t)irange(170, 230)); // seams and nail heads
                if (world.at(xx, y + 1).material == M::Platform) world.at(xx, y + 1).shade = (uint8_t)irange(40, 80);
                int under = world.at(xx, y + 1).material == M::Platform ? y + 2 : y + 1;
                if (world.in(xx, under) && world.at(xx, under).material == M::Empty) bgPut(xx, under, shadeC(world.bgOf(xx, under), 0.6f)); // its shadow
            }
            for (int end : {x + 1, x1 - 2})
            {
                if (x1 - x < 6) break;
                int top = y + 2, fl = top, dir = end == x + 1 ? 1 : -1;
                if (isSolid(end - dir * 2, y)) // butting a wall: a knee brace into it
                {
                    for (int k = 0; k < 7; k++) { bgPut(end - dir + dir * k, top + 6 - k, {96, 66, 40, 255}); bgPut(end - dir + dir * k, top + 7 - k, {64, 44, 28, 255}); }
                    continue;
                }
                while (fl < y + 50 && world.in(end, fl) && world.at(end, fl).material == M::Empty) fl++;
                if (fl < y + 50 && world.in(end, fl) && world.at(end, fl).material != M::Empty) // a post down to whatever's below, with a brace
                {
                    for (int yy = top; yy < fl; yy++)
                    {
                        bgPut(end, yy, {96, 66, 40, 255});
                        bgPut(end + 1, yy, {64, 44, 28, 255});
                    }
                    for (int k = 0; k < std::min(6, fl - top); k++) bgPut(end + dir * (k + 2), top + k, {80, 56, 34, 255});
                    continue;
                }
                int ce = y - 1;
                while (ce > y - 110 && world.in(end, ce) && world.at(end, ce).material == M::Empty) ce--;
                if (ce > y - 110 && world.in(end, ce) && isSolid(end, ce)) // or ropes from the roof
                    for (int yy = ce + 1; yy < y; yy++) bgPut(end, yy, (yy % 3 == 0) ? Color{120, 96, 60, 255} : Color{160, 130, 84, 255});
            }
        }
}

// Depth on the back wall: rock strata and hairline cracks, and shadow pooling where it meets the rock in front.
static void shadeBackWall(bool surface)
{
    std::vector<float> occ((size_t)W * H), tmp((size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) occ[(size_t)y * W + x] = isSolid(x, y) ? 1.0f : 0.0f;
    const int R = 5;
    for (int pass = 0; pass < 2; pass++) // two box blurs make a soft falloff from every wall
    {
        for (int y = 0; y < H; y++)
        {
            float s = 0;
            for (int x = -R; x < W + R; x++)
            {
                if (x + R < W) s += occ[(size_t)y * W + std::min(W - 1, x + R)];
                if (x - R - 1 >= 0) s -= occ[(size_t)y * W + x - R - 1];
                if (x >= 0 && x < W) tmp[(size_t)y * W + x] = s / (2 * R + 1);
            }
        }
        for (int x = 0; x < W; x++)
        {
            float s = 0;
            for (int y = -R; y < H + R; y++)
            {
                if (y + R < H) s += tmp[(size_t)std::min(H - 1, y + R) * W + x];
                if (y - R - 1 >= 0) s -= tmp[(size_t)(y - R - 1) * W + x];
                if (y >= 0 && y < H) occ[(size_t)y * W + x] = s / (2 * R + 1);
            }
        }
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (world.skyOf(x, y) || world.at(x, y).material != M::Empty || (surface && y < surf[x] + 6)) continue;
            Color c = world.bgOf(x, y);
            float strata = std::sin(y * 0.11f + fbm(x * 0.008f, y * 0.01f, seed + 70, 2) * 9);
            float crack = fbm(x * 0.01f, y * 0.01f, seed + 72, 2) > 0.6f ? std::fabs(fbm(x * 0.03f, y * 0.03f, seed + 71, 3) - 0.5f) : 1.0f; // only in patches
            float k = (1 - 0.55f * clampf(occ[(size_t)y * W + x] * 1.6f, 0, 1)) * (0.93f + 0.07f * strata) * (crack < 0.008f ? 0.65f : 1.0f);
            Color sh = shadeC(c, std::min(1.0f, k));
            sh.a = c.a; // (keep a wall pattern id)
            world.bgAt(x, y) = sh;
        }
}

// ---------------------------------------------------------------- the starter caves, under the castle too
// The Greenmarch caves carry on east under Castle Dunmoor as far as the crypts. They're the same system:
// a stub tunnel at CAVE_JOINT below the shared ground line (the gatehouse floor = the castle grounds) joins
// the two sides when the pieces are stitched. They keep well clear of the castle's halls - break into
// those and you could walk back under the gatehouse you've sealed behind you.
static const int CAVE_JOINT = 330;

static void castleCaves(int g)
{
    const StageDef& gm = STAGES[0];
    const int M_ = 30; // rock kept round every hall and passage
    std::vector<int> sum((size_t)(W + 1) * (H + 1), 0); // prefix sums of the castle's open cells
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            sum[(size_t)(y + 1) * (W + 1) + x + 1] = (world.at(x, y).material == M::Empty) + sum[(size_t)y * (W + 1) + x + 1] + sum[(size_t)(y + 1) * (W + 1) + x] - sum[(size_t)y * (W + 1) + x];
    auto nearOpen = [&](int x, int y) {
        int x0 = std::max(0, x - M_), y0 = std::max(0, y - M_), x1 = std::min(W, x + M_ + 1), y1 = std::min(H, y + M_ + 1);
        return sum[(size_t)y1 * (W + 1) + x1] - sum[(size_t)y0 * (W + 1) + x1] - sum[(size_t)y1 * (W + 1) + x0] + sum[(size_t)y0 * (W + 1) + x0] > 0;
    };
    NoiseGrid caveN(W, H, 4, [&](float x, float y) { return fbm(x * 0.0055f, y * 0.009f, seed + 300, 4); });
    NoiseGrid caveN2(W, H, 4, [&](float x, float y) { return fbm(x * 0.016f, y * 0.024f, seed + 399, 3); });
    NoiseGrid rockN(W, H, 4, [&](float x, float y) { return fbm(x * 0.025f, y * 0.035f, seed + 307, 3); });
    std::vector<uint8_t> cave((size_t)W * H, 0); // 1 = Greenmarch rock, 2 = open cave
    for (int y = g + 60; y < H - 4; y++)
        for (int x = 4; x < W - 4; x++)
            if (!nearOpen(x, y) && world.at(x, y).material != M::Bedrock)
                cave[(size_t)y * W + x] = 1 + (caveN.at(x, y) > 0.63f || caveN2.at(x, y) > 0.76f);
    for (int x = 0; x < 170; x++) // the stub west, to meet the plains caves (through the bedrock rim)
        for (int dy = -13; dy <= 13; dy++)
        {
            int y = g + CAVE_JOINT + dy;
            if (!nearOpen(x, y)) cave[(size_t)y * W + x] = 2;
        }
    // every cave joins the stub's: flood each, tunnel the big ones over, fill the scraps
    std::vector<int> lab((size_t)W * H, 0), q, mainCells;
    auto flood = [&](int k0, int id, std::vector<int>& cells) {
        q.assign(1, k0);
        lab[k0] = id;
        for (size_t h = 0; h < q.size(); h++)
        {
            int k = q[h];
            cells.push_back(k);
            for (int nk : {k + 1, k - 1, k + W, k - W})
                if (nk >= 0 && nk < W * H && cave[nk] == 2 && !lab[nk]) { lab[nk] = id; q.push_back(nk); }
        }
    };
    flood((g + CAVE_JOINT) * W + 2, 1, mainCells);
    for (int k = 0; k < W * H; k++)
    {
        if (cave[k] != 2 || lab[k]) continue;
        std::vector<int> cells;
        flood(k, 2, cells);
        if (cells.size() < 300) { for (int c : cells) cave[c] = 1; continue; }
        int c = cells[cells.size() / 2], best = mainCells[0];
        long long bd = 1LL << 40;
        for (size_t i = 0; i < mainCells.size(); i += 7)
        {
            long long dx = mainCells[i] % W - c % W, dy = mainCells[i] / W - c / W, d2 = dx * dx + 4 * dy * dy; // sideways links stay walkable
            if (d2 < bd) { bd = d2; best = mainCells[i]; }
        }
        float x0 = (float)(c % W), y0 = (float)(c / W), x1 = (float)(best % W), y1 = (float)(best / W);
        int steps = (int)(std::sqrt((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))) / 1.5f) + 1, r = tunnelRadius();
        for (int i = 0; i <= steps; i++)
        {
            float t = (float)i / steps, px = x0 + (x1 - x0) * t, py = y0 + (y1 - y0) * t;
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++)
                {
                    int x = (int)px + dx, y = (int)py + dy;
                    if (dx * dx + dy * dy <= r * r && x >= 4 && y >= 0 && x < W - 4 && y < H - 4 && cave[(size_t)y * W + x]) cave[(size_t)y * W + x] = 2;
                }
        }
        for (int c2 : cells) mainCells.push_back(c2); // later caves can join this one
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            uint8_t v = cave[(size_t)y * W + x];
            if (!v) continue;
            if (v == 2) world.at(x, y) = Cell{};
            else place(x, y, rockN.at(x, y) > 0.62f ? gm.alt : (rockN.at(x, y) < 0.36f ? gm.alt2 : gm.base));
            Color c = lerpColor(gm.bgA, gm.bgB, fbm(x * 0.02f, y * 0.03f, seed + 350, 2));
            world.bgAt(x, y) = shadeC(c, 0.62f);
            world.skyAt(x, y) = 0;
        }
}

// The plains' end of the joint: a tunnel in from the east edge at the same depth below the gatehouse floor.
static void plainsCaveJoint(int hf)
{
    for (int x = W - 200; x < W; x++) carve((float)x, (float)(hf + CAVE_JOINT), 13);
}
static void openPlainsJoint(int hf) // after the rock is laid: through the bedrock rim
{
    for (int y = hf + CAVE_JOINT - 12; y <= hf + CAVE_JOINT + 12; y++)
        for (int x = W - 5; x < W; x++)
            if (world.in(x, y)) world.at(x, y) = Cell{};
}

// ---------------------------------------------------------------- the damp caves (work in progress)
// Below the starter caves and beside the descent, everything the biomes leave unused is one cave system:
// dim, mossy and wet. For now it's sealed - an obsidian barrier against every biome, bedrock rims inside
// them - and there's nothing in it. It's grown a chunk at a time as the camera comes near, so the huge
// unused corner of the world costs nothing until it's seen.
// ponytail: an unseen damp chunk reads as solid rock to collision and light (World::get returns `fill`).
// Fine while it's sealed; once there's a way in, allocate chunks around the player as well as the camera.
static std::vector<Rectangle> pieceRects; // every biome in the live world, in cells
std::vector<Rectangle> devPieceRects()
{
    std::vector<Rectangle> r;
    for (auto& p : pieceRects) r.push_back({p.x / world.scale, p.y / world.scale, p.width / world.scale, p.height / world.scale});
    return r;
}
static int dampSeed = 0, dampOX = 0, dampOY = 0; // noise origin, so the caves stay put when the world is re-cut
enum { DZ_NONE, DZ_BARRIER, DZ_CAVE };

static int dampZone(int x, int y)
{
    float top = -1, right = 1e9f;
    for (auto& r : pieceRects)
    {
        if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return DZ_NONE;
        if (x >= r.x && x < r.x + r.width && y >= r.y + r.height) top = std::max(top, r.y + r.height);
        if (y >= r.y && y < r.y + r.height && x < r.x) right = std::min(right, r.x);
    }
    if (top < 0 || right > 1e8f) return DZ_NONE; // only below a biome and west of one
    float depth = std::min(y - top, right - x), b = 40 + 24 * fbm((x - dampOX) * 0.01f, (y - dampOY) * 0.01f, dampSeed + 9, 2);
    return depth < b ? DZ_BARRIER : DZ_CAVE;
}

static bool dampOpen(int x, int y) // in cells
{
    float ux = (x - dampOX) / 2.0f, uy = (y - dampOY) / 2.0f;
    float wx = (fbm(ux * 0.004f, uy * 0.004f, dampSeed + 1, 2) - 0.5f) * 180, wy = (fbm(ux * 0.004f, uy * 0.004f, dampSeed + 2, 2) - 0.5f) * 180;
    float tube = 1 - std::fabs(2 * fbm((ux + wx) * 0.007f, (uy + wy) * 0.009f, dampSeed + 3, 3) - 1);
    return tube > 0.9f || fbm(ux * 0.006f, uy * 0.011f, dampSeed + 4, 3) > 0.6f;
}

static Color dampBack(int x, int y)
{
    float n = fbm((x - dampOX) * 0.01f, (y - dampOY) * 0.015f, dampSeed + 5, 3);
    return lerpColor(Color{8, 12, 10, 255}, Color{20, 30, 22, 255}, n);
}

static void growDampChunk(int cx, int cy)
{
    const int E = 3, B = CS + 2 * E; // a border, so moss can see the cave a few cells off
    static int zone[B][B];
    static bool open[B][B];
    int x0 = cx * CS - E, y0 = cy * CS - E;
    for (int j = 0; j < B; j++)
        for (int i = 0; i < B; i++)
        {
            zone[j][i] = dampZone(x0 + i, y0 + j);
            open[j][i] = zone[j][i] == DZ_CAVE && dampOpen(x0 + i, y0 + j);
        }
    Chunk& ch = world.touch(cx * CS, cy * CS); // filled with plain rock; the cave goes in over it
    for (int j = E; j < CS + E; j++)
        for (int i = E; i < CS + E; i++)
        {
            int x = x0 + i, y = y0 + j;
            if (zone[j][i] == DZ_NONE) continue;
            Cell c{};
            float h = hash2(x, y, dampSeed);
            if (zone[j][i] == DZ_BARRIER) { c.material = M::Obsidian; c.shade = (uint8_t)(h * 120); }
            else if (!open[j][i])
            {
                bool face = false; // moss grows a few cells deep on every wall
                for (int d = 1; d <= E && !face; d++) face = open[j - d][i] || open[j + d][i] || open[j][i - d] || open[j][i + d];
                c.material = face ? M::Moss : (fbm((x - dampOX) * 0.03f, (y - dampOY) * 0.04f, dampSeed + 6, 2) > 0.55f ? M::Dirt : M::Stone);
                if (face && fbm((x - dampOX) * 0.05f, (y - dampOY) * 0.05f, dampSeed + 8, 2) > 0.6f) c.material = M::Glowmoss; // patches of faint light
                c.shade = (uint8_t)(h * (face ? 90 : 110)); // wet rock: the dark end of every colour
            }
            else
            {
                int fl = 0; // how far down to the floor (within this chunk's border)
                while (j + fl + 1 < B && open[j + fl + 1][i] && fl < 6) fl++;
                bool floorSeen = j + fl + 1 < B && !open[j + fl + 1][i] && zone[j + fl + 1][i] == DZ_CAVE;
                int depth = (int)(fbm((x - dampOX) * 0.02f, 1.5f, dampSeed + 7, 2) * 9) - 3; // puddles here and there
                if ((floorSeen && fl < depth) || (!open[j - 1][i] && h < 0.008f)) // a puddle, or a drip off the roof
                {
                    c.material = M::Water;
                    c.shade = (uint8_t)xr();
                }
            }
            ch.cells[World::idx(x, y)] = c;
            world.bgAt(x, y) = dampBack(x, y);
        }
    ch.awake = 4; // let the water find its level
}

void growDampCaves()
{
    if (pieceRects.empty()) return;
    int k = world.scale, m = CS * 2;
    int cx0 = std::max(0, ((int)G.camX * k - m) / CS), cy0 = std::max(0, ((int)G.camY * k - m) / CS);
    int cx1 = std::min(world.cw - 1, (((int)G.camX + G.vw) * k + m) / CS), cy1 = std::min(world.ch - 1, (((int)G.camY + G.vh) * k + m) / CS);
    int budget = 3; // a few chunks a frame: the edge of the screen fills in over a moment, without a hitch
    for (int cy = cy0; cy <= cy1 && budget; cy++)
        for (int cx = cx0; cx <= cx1 && budget; cx++)
        {
            if (world.chunks[(size_t)cy * world.cw + cx]) continue;
            int x = cx * CS, y = cy * CS;
            if (dampZone(x, y) || dampZone(x + CS - 1, y) || dampZone(x, y + CS - 1) || dampZone(x + CS - 1, y + CS - 1)) { growDampChunk(cx, cy); budget--; }
        }
}

// --dump: what a damp cell looks like, without growing it (the whole corner would be hundreds of MB)
static bool dampPreview(int x, int y, Color& out)
{
    int z = dampZone(x, y);
    if (!z) return false;
    if (z == DZ_BARRIER) out = props(M::Obsidian).a;
    else if (!dampOpen(x, y)) out = brighten(props(M::Stone).a, -24); // wet rock (the moss is only on its faces)
    else out = dampBack(x, y);
    return true;
}

// ---------------------------------------------------------------- the deep biomes: a descent in levels
// Past the castle each biome is a stack of levels you work your way down through: every level crosses the
// biome, the next runs back the other way beneath it, and the last ends at the haven, bottom right - so the
// world as a whole falls away level by level. Each kind builds its levels its own way: crypts are masonry
// corridors and halls joined by stairs, mines are timbered galleries joined by roped shafts, caverns wind
// and open into chambers, with chutes dropping to the level below. At the far end of each level a low
// passage (a true crawlspace in the crypts) leads to a burial room or a forgotten drift with something in it.
// Around that guaranteed skeleton the rock is riddled the Noita way: domain-warped ridge noise for winding
// tube tunnels, low-frequency noise for open caverns. Each level's floor is laid back down after, so the
// noise can break into the way through but never cut it.
static bool deepLayout = false; // the biome being built is one of these
static const int DEEP_LEVELS = 5; // odd: the levels alternate direction and the last must end at the haven, bottom right
static int deepFloor[DEEP_LEVELS];               // each level's nominal floor row
static std::vector<int> levelFloor[DEEP_LEVELS]; // each level's actual floor per column (-1 where it doesn't run)
struct Shaft { int x, top, bottom; };            // mines: plank-capped, with a rope down
static std::vector<Shaft> shafts;
struct SideRoom { int x0, x1, floor; };
static std::vector<SideRoom> sideRooms;
struct Corridor { int x0, x1, floor, lvl; };     // crypts: for the tiled back wall
static std::vector<Corridor> corridors;
struct DeepHall { int x0, x1, floor, h; };       // crypts and mines: a tall room on the way (gets a mezzanine)
static std::vector<DeepHall> deepHalls;
struct PitCap { int x0, x1, floor; };            // a plank floor over a low chamber you drop into
static std::vector<PitCap> pitCaps;

static void airRect(int x0, int y0, int x1, int y1) // inclusive, kept off the bedrock rim
{
    for (int y = std::max(5, y0); y <= std::min(H - 6, y1); y++)
        for (int x = std::max(5, x0); x <= std::min(W - 6, x1); x++) air[(size_t)y * W + x] = 1;
}

static void deepLevels(const StageDef& d, int& arenaX, int& arenaFloor)
{
    bool crypt = d.kind == SK_CRYPT, mines = d.kind == SK_MINES;
    shafts.clear();
    sideRooms.clear();
    corridors.clear();
    deepHalls.clear();
    pitCaps.clear();
    bool structured = crypt || mines; // built like the castle: a chain of rooms, halls and passages whose floors step up and down
    // tunnels: 1 - |2n - 1| peaks along the creases of the noise, and warping its input bends them into
    // long winding tubes; caverns: the old low-frequency blobs, rarer than before. The crypts' stone is
    // mostly sound, the caves riddled.
    NoiseGrid tube(W, H, 4, [&](float x, float y) {
        float wx = (fbm(x * 0.003f, y * 0.003f, seed + 200, 2) - 0.5f) * 220, wy = (fbm(x * 0.003f, y * 0.003f, seed + 201, 2) - 0.5f) * 220;
        return 1 - std::fabs(2 * fbm((x + wx) * 0.006f, (y + wy) * 0.008f, seed + 210, 3) - 1);
    });
    NoiseGrid cavern(W, H, 4, [&](float x, float y) { return fbm(x * 0.0055f, y * 0.009f, seed, 4); });
    float tt = crypt ? 0.984f : (mines ? 0.945f : 0.923f), ct = crypt ? 0.75f : (mines ? 0.63f : 0.60f); // (tube width ~ 1 - tt)
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            air[(size_t)y * W + x] = (tube.at(x, y) > tt || cavern.at(x, y) > ct) &&
                                     !(x < wingL && y < 320) && !(x >= W - wingR && y >= H - 290); // under the last biome's floor / over the next's roof: its own rock

    const int xL = 120, xR = W - 300, N = DEEP_LEVELS; // room at both ends for a side room; the haven takes the last level's right end
    for (int i = 0; i < N; i++)
    {
        deepFloor[i] = 130 + i * (H - 280) / (N - 1);
        levelFloor[i].assign(W, -1);
    }
    std::vector<float> bottom(W);
    for (int i = 0; i < N; i++)
    {
        int dir = i % 2 ? -1 : 1, fy = deepFloor[i];
        int xs = i == 0 ? 12 + wingL : (dir > 0 ? xL : xR), xe = i == N - 1 ? W - wingR - 250 : (dir > 0 ? xR : xL);
        std::fill(bottom.begin(), bottom.end(), -1.0f);
        int nextHall = xs + dir * irange(120, 200);
        std::vector<int> off(W, 0), hgt(W, 0); // per column: how far this level's floor sits from its nominal row, and the room's height
        int zoneEnd = i == N - 1 && d.boss >= 0 ? 420 : 130; // the ends stay level: the stairs, the haven and the arena meet them there
        if (structured)
            for (int a = xs, prev = 0; dir > 0 ? a < xe : a > xe;)
            {
                bool free = std::abs(a - xs) >= 130 && std::abs(xe - a) >= zoneEnd + 150;
                bool hall = free && chance(3);
                int len = hall ? irange(70, 110) : irange(90, 150);
                int b = dir > 0 ? std::min(a + len, xe) : std::max(a - len, xe);
                int cur = free && std::abs(xe - b) >= zoneEnd ? std::max(-45, std::min(45, prev + irange(-32, 32))) : 0;
                int step = cur - prev, h = hall ? (crypt ? 62 : 52) : (crypt ? 36 : 38);
                for (int x = a; x != b + dir; x += dir) // a ramp up or down at 45 degrees over the start of the room, then flat
                {
                    int k = std::min(std::abs(x - a), std::abs(step));
                    off[x] = prev + (step > 0 ? k : -k);
                    hgt[x] = h;
                }
                if (hall) deepHalls.push_back({std::min(a, b), std::max(a, b), fy + cur, h});
                prev = cur;
                a = b;
            }
        for (int x = xs; dir > 0 ? x <= xe : x >= xe; x += 2 * dir)
        {
            float cy;
            if (structured)
            {
                int f = fy + off[x];
                airRect(x - 1, f - hgt[x], x + 1, f - 1);
                cy = (float)(f - 15);
                for (int dx = -1; dx <= 1; dx++)
                    if (x + dx >= 0 && x + dx < W) bottom[x + dx] = (float)f - 1;
            }
            else // a worm: it heads for the far end, nudged up and down by noise
            {
                float wob = mines ? (fbm(x * 0.004f, i * 3.7f, seed + 45, 2) - 0.5f) * 16 : (fbm(x * 0.005f, i * 3.7f, seed + 43, 2) - 0.5f) * 50;
                int r = mines ? 18 : 20 + (int)(fbm(x * 0.012f, i * 5.1f, seed + 41, 2) * 16);
                cy = fy - r + wob;
                carve((float)x, cy, r);
                for (int dx = -r; dx <= r; dx++)
                    if (x + dx >= 0 && x + dx < W) bottom[x + dx] = std::max(bottom[x + dx], cy + std::sqrt((float)(r * r - dx * dx)));
                if (x == nextHall) // a chamber on the way: a dome over the level's floor
                {
                    int rx = mines ? irange(40, 55) : irange(50, 85), ry = mines ? irange(36, 46) : irange(45, 75), fl = (int)cy + r;
                    for (int y = fl - ry; y < fl; y++)
                        for (int xx = x - rx; xx <= x + rx; xx++)
                        {
                            float u = (float)(xx - x) / rx, v = (float)(fl - y) / ry;
                            if (u * u + v * v <= 1) airRect(xx, y, xx, y);
                        }
                    nextHall += dir * irange(mines ? 260 : 180, mines ? 360 : 260);
                }
            }
            path.push_back({(float)x, cy});
        }
        if (crypt) corridors.push_back({std::min(xs, xe), std::max(xs, xe), fy, i});
        for (int x = 0; x < W; x++) // a guaranteed floor under the level, bridging any cavern it crosses
        {
            if (bottom[x] < 0) continue;
            int b = (int)bottom[x] + 1;
            levelFloor[i][x] = b;
            for (int y = b; y < b + 5 && y < H; y++) air[(size_t)y * W + x] = 0;
        }
        for (int px = xs + dir * irange(190, 260); structured && (dir > 0 ? px < xe : px > xe) && std::abs(xe - px) > zoneEnd + 60 && std::abs(px - xs) > 150; px += dir * irange(200, 300))
        {
            if (hgt[px] > 40 || off[px - 26] != off[px + 26] || hgt[px - 26] > 40 || hgt[px + 26] > 40 || off[px] != off[px - 26]) continue; // flat passage only
            int f = levelFloor[i][px]; // a plank floor over a low chamber (22 high, so you can jump back up through it)
            if (f < 0) continue;
            airRect(px - 24, f + 2, px + 24, f + 23);
            for (int x = px - 24; x <= px + 24; x++)
                for (int y = f + 24; y < f + 29 && y < H; y++) air[(size_t)y * W + x] = 0;
            pitCaps.push_back({px - 24, px + 24, f});
            sideRooms.push_back({px - 24, px + 24, f + 24});
        }
    }

    for (int i = 0; i + 1 < N; i++) // the ways down, at the far end of each level, and a side room past them
    {
        int dir = i % 2 ? -1 : 1, xe = dir > 0 ? xR : xL, fy = levelFloor[i][xe] > 0 ? levelFloor[i][xe] : deepFloor[i];
        int below = deepFloor[i + 1];
        if (mines) // a timbered shaft with a rope, planked over so the level walks on past it
        {
            int sx = xe - dir * 22;
            airRect(sx - 10, fy - 30, sx + 10, below - 1);
            shafts.push_back({sx, fy, below - 1});
            path.push_back({(float)sx, (float)(fy + below) / 2});
        }
        else if (crypt) // stairs doubling back under the corridor, five down for every six along
        {
            for (int x = xe - dir * 60, y = fy; y < below; x -= dir * 6, y += 5)
            {
                airRect(std::min(x, x - dir * 5), y - 36, std::max(x, x - dir * 5), y + 4);
                path.push_back({(float)x, (float)y - 18});
            }
        }
        else // a chute bending down to the level below
        {
            float x0 = (float)(xe - dir * 40), y0 = (float)fy - 10, x1 = (float)(xe - dir * 170), y1 = (float)below - 20;
            int key = seed + irand(1000);
            for (int k = 0; k <= 120; k++)
            {
                float t = k / 120.0f, bend = (fbm(t * 3, 0.5f, key, 2) - 0.5f) * 80 * std::sin(3.14159f * t);
                float px = x0 + (x1 - x0) * t + bend, py = y0 + (y1 - y0) * t;
                carve(px, py, 17);
                if (k % 6 == 0) path.push_back({px, py});
            }
        }
        // the side room, through a squeeze in the end wall
        int h = crypt ? 10 : 15; // prone in the crypts; a crouch elsewhere
        int d1 = xe + dir * 40, r1 = d1 + dir * 60;
        airRect(std::min(xe, d1), fy - h, std::max(xe, d1), fy - 1);
        airRect(std::min(d1, r1), fy - (crypt ? 30 : 34), std::max(d1, r1), fy - 1);
        for (int x = std::min(xe, r1); x <= std::max(xe, r1); x++) // its floor holds even over a cavern
            for (int y = fy; y < fy + 5 && y < H; y++) air[(size_t)y * W + x] = 0;
        sideRooms.push_back({std::min(d1, r1), std::max(d1, r1), fy});
    }

    pathFloor.assign(W, deepFloor[N - 1]); // what the haven's approach is cut from: the last level
    for (int x = 0; x < W; x++) if (levelFloor[N - 1][x] > 0) pathFloor[x] = levelFloor[N - 1][x];

    if (d.boss >= 0) // the guardian's arena on the last level, short of the haven
    {
        arenaX = W - wingR - 460;
        arenaFloor = pathFloor[arenaX];
        airRect(arenaX - 150, arenaFloor - 136, arenaX + 150, arenaFloor - 1);
    }
}

// After the rock is laid: planks and ropes over the shafts, the crypts' tiled walls, furnished side rooms.
static void deepFinish(const StageDef& d)
{
    bool crypt = d.kind == SK_CRYPT;
    const Color timber = {112, 76, 44, 255};
    for (auto& sh : shafts)
    {
        for (int x = sh.x - 11; x <= sh.x + 11; x++) { place(x, sh.top, M::Platform); place(x, sh.top + 1, M::Platform); }
        for (int y = sh.top + 2; y < sh.bottom; y++)
            for (int x : {sh.x - 10, sh.x + 10}) bgPut(x, y, shadeC(timber, 0.8f));
        for (int y = sh.top + 20; y < sh.bottom; y += 20)
            for (int x = sh.x - 10; x <= sh.x + 10; x++) bgPut(x, y, shadeC(timber, 0.65f));
        Interact rope{IT_ROPE, (float)sh.x + 4, (float)sh.top + 2};
        rope.data = sh.bottom;
        G.inter.push_back(rope);
    }
    for (auto& p : pitCaps) // the plank floor over a chamber below: drop through with S, jump back up
        for (int x = p.x0; x <= p.x1; x++) { place(x, p.floor, M::Platform); place(x, p.floor + 1, M::Platform); }
    for (auto& h : deepHalls) // a tall room gets a mezzanine: one-way planks to hop up onto
    {
        int w = h.x1 - h.x0, lw = std::max(18, w / 2 - 10), lx = chance(2) ? h.x0 + 6 : h.x1 - 6 - lw;
        for (int x = lx; x < lx + lw; x++) { place(x, h.floor - 24, M::Platform); place(x, h.floor - 23, M::Platform); }
        if (h.h >= 60 && w >= 80) // another ledge on the opposite side
        {
            int rx = lx == h.x0 + 6 ? h.x1 - 6 - 18 : h.x0 + 6;
            for (int x = rx; x < rx + 18; x++) { place(x, h.floor - 24, M::Platform); place(x, h.floor - 23, M::Platform); }
        }
    }
    for (auto& c : corridors) // dressed stone, with burial niches let into the wall
    {
        Color st = shadeC(d.bgB, 0.8f);
        for (int x = c.x0; x <= c.x1; x++) // the floors step up and down, so follow each column's own
        {
            int fl = levelFloor[c.lvl][x] > 0 ? levelFloor[c.lvl][x] : c.floor;
            for (int y = fl - 76; y < fl + 26; y++)
            {
                if (!world.in(x, y) || world.at(x, y).material != M::Empty) continue;
                bgStyle(x, y, shadeC(st, 1.15f + 0.2f * hash2(x / 24, (c.floor - y) / 14, seed + 5)), WALL_TILE);
            }
        }
        for (int x = c.x0 + 10; x + 14 < c.x1; x += 26)
        {
            int ny = (levelFloor[c.lvl][x] > 0 ? levelFloor[c.lvl][x] : c.floor) - 20;
            if (levelFloor[c.lvl][x + 13] != levelFloor[c.lvl][x] || world.at(x, ny).material != M::Empty || world.at(x + 13, ny + 8).material != M::Empty) continue;
            for (int y = ny; y < ny + 9; y++)
                for (int xx = x; xx < x + 14; xx++) world.bgAt(xx, y) = shadeC(d.bgA, 0.35f);
            if (chance(2)) // a skull looking out
            {
                for (int k = 0; k < 3; k++)
                    for (int j = 0; j < 3; j++) world.bgAt(x + 6 + k, ny + 5 + j) = {200, 194, 172, 255};
                world.bgAt(x + 6, ny + 6) = world.bgAt(x + 8, ny + 6) = shadeC(d.bgA, 0.3f);
            }
        }
    }
    for (auto& r : sideRooms)
    {
        int mid = (r.x0 + r.x1) / 2;
        if (crypt) // coffins either side of a chest
        {
            for (int cx : {r.x0 + 4, r.x1 - 13})
                for (int y = r.floor - 4; y < r.floor; y++)
                    for (int x = cx; x < cx + 10; x++) place(x, y, (y == r.floor - 4 || x == cx || x == cx + 9) ? M::Wood : M::Bone);
            addChest(mid, r.floor);
        }
        else if (chance(2)) addChest(mid, r.floor);
        else
        {
            paintSkeleton(mid - 10, r.floor, 1); // someone squeezed in and never out
            addPickupWeapon((float)mid + 8, (float)r.floor - 8, randomWeapon(G.stage + 1));
            addCoins((float)mid, (float)r.floor - 4, irange(2, 4), 1 + G.stage / 3);
        }
        addSconce(mid, r.floor - 18, 1);
    }
}

// Far enough from the main way through that a trap or a burial room won't block it.
static bool offRoad(int x, int y, int gap)
{
    if (!deepLayout) return std::abs(y - pathFloor[x]) > gap;
    for (size_t i = 0; i < path.size(); i += 3)
        if (std::abs(path[i].x - x) <= gap && std::abs(path[i].y + 18 - y) <= gap) return false; // path points run ~18 above the floor
    return true;
}

static void buildStage(int s, int entryFloor)
{
    const StageDef& d = STAGES[s];
    // the first stage opens on the Whispering Dunes: a quiet walk up from the beach before the Greenmarch
    const int D = d.kind == SK_PLAINS && s == 0 ? 760 : 0;
    bool castle = d.kind == SK_CASTLE;
    wingL = wingR = 0;
    if (d.kind == SK_PLAINS) { W = 3000 + D; H = 1800; } // (its caves run as deep as the castle's) // (its last 600 units are the battlefield before Dunmoor) // tall sky for the gatehouse; caves running down as deep as the castle's foundations
    else if (castle) { W = 3200; H = 2000; }
    else { wingL = s >= 3 ? WING : 0; wingR = s + 1 < STAGE_COUNT ? WING : 0; W = 1400 + wingL + wingR; H = 1500; } // the deep biomes: taller than they're wide - you go down through them
    worldInit(W, H);
    seed = irand(1 << 30);
    air.assign((size_t)W * H, 0);
    path.clear();
    surf.assign(W, 0);
    G.duneEnd = D;
    roadValid = !castle;
    lookouts.clear();
    fortZones.clear();
    cellars.clear();
    doorYards.clear();
    keepZones.clear();
    mineX = -1;
    LevelEnds ends{};
    int arenaX = -1, arenaFloor = 0;
    deepLayout = !castle && !d.surface;
    if (castle)
    {
        ends = castleLayout(d, std::max(entryFloor, 420)); // headroom for the towers: the stitcher lines the ground up with the gatehouse
        castleCaves(ends.sy);
    }
    else
    {
    bool plains = d.kind == SK_PLAINS;
    if (d.surface)
        for (int x = 0; x < W; x++) surf[x] = 230 + (int)(fbm(x * 0.004f, 0.5f, seed + 5, 3) * 40) - 20; // gentle rolling hills
    for (int x = 0; x < D + 80; x++) // long dunes with small ripples, sloping down to the sea at the far left
    {
        float dune = 260 - (fbm(x * 0.006f, 9.1f, seed + 15, 3) - 0.5f) * 80 - std::sin(x * 0.02f + seed) * 4;
        float beach = clampf((130 - x) / 110.0f, 0, 1);
        dune += (324 - dune) * beach * beach * (3 - 2 * beach);
        float t = clampf((x - D) / 80.0f, 0, 1);
        surf[x] = (int)(dune + (surf[x] - dune) * t * t * (3 - 2 * t));
    }
    if (deepLayout) deepLevels(d, arenaX, arenaFloor);
    else
    {
    NoiseGrid caveN(W, H, 4, [&](float x, float y) { return fbm(x * 0.0055f, y * 0.009f, seed, 4); }); // low frequencies: caverns on a grand scale
    NoiseGrid caveN2(W, H, 4, [&](float x, float y) { return fbm(x * 0.016f, y * 0.024f, seed + 99, 3); });
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            // big open caverns from low-frequency noise, smaller pockets from a finer layer
            float n = caveN.at(x, y);
            float n2 = caveN2.at(x, y);
            bool cave = plains ? (x > D + 60 && y > surf[x] + 75 && (n > 0.63f || n2 > 0.76f)) : (n > 0.575f || n2 > 0.705f);
            air[(size_t)y * W + x] = cave ? 1 : 0;
            if (d.surface && y < surf[x]) air[(size_t)y * W + x] = 1;
        }

    // The main road: a gently sloped tunnel from entrance to exit with a guaranteed floor,
    // bridging over any caverns it crosses. Side caves branch off it.
    float wx = 12, wy = d.surface ? (float)surf[12] - 14 : 90.0f, vy = 0;
    float startY = wy, endY = H - 120.0f;
    std::vector<int> radii;
    while (wx < W - 12)
    {
        int r = 24 + (int)(fbm(wx * 0.015f, 3.3f, seed + 3, 2) * 16);
        bool onPlains = d.surface; // the plains road stays on the grass the whole way
        if (onPlains) { r = 10; wy = (float)surf[(int)wx] - 10; } // the road simply follows the grass
        carve(wx, wy, r);
        path.push_back({wx, wy});
        radii.push_back(r);
        wx += 2;
        // steer towards a line that sinks from the entrance down to the deep exit
        float flat = d.surface ? clampf((wx - 600) / (W - 600.0f), 0, 1) : wx / W;
        if (onPlains) { vy = 0; continue; }
        if (d.surface && wx < 640) wy += 0.6f; // dive under the mountain
        float target = startY + (endY - startY) * flat + std::sin(wx * 0.012f + seed) * 45 + std::sin(wx * 0.031f + seed * 0.7f) * 20;
        vy += (target - wy) * 0.003f + frange(-0.2f, 0.2f);
        vy = clampf(vy * 0.97f, -0.75f, 0.75f);
        wy += vy;
        if (wy < 60) { wy = 60; vy = std::fabs(vy); }
        if (wy > H - 70) { wy = H - 70.0f; vy = -std::fabs(vy); }
    }
    pathFloor.assign(W, H - 5);
    std::vector<float> bottom(W, -1);
    for (size_t i = 0; i < path.size(); i++)
    {
        int r = radii[i];
        for (int dx = -r; dx <= r; dx++)
        {
            int x = (int)path[i].x + dx;
            if (x < 0 || x >= W) continue;
            bottom[x] = std::max(bottom[x], path[i].y + std::sqrt((float)(r * r - dx * dx)));
        }
    }
    for (int x = 0; x < W; x++)
    {
        if (bottom[x] < 0) continue;
        int b = (int)bottom[x] + 1;
        pathFloor[x] = b;
        for (int y = b; y < b + 4 && y < H; y++) air[(size_t)y * W + x] = 0;
    }

    // narrow crawlways and climbs that thread between caverns
    for (int t = 0; t < (plains ? 0 : 30); t++)
    {
        float tx = (float)irange(40, W - 300), ty = (float)irange(40, H - 40);
        float ang = frange(0, 6.2832f);
        int len = irange(160, 420), r = irange(8, 12);
        for (int k = 0; k < len; k++)
        {
            carve(tx, ty, r);
            ang += frange(-0.25f, 0.25f);
            tx = clampf(tx + std::cos(ang) * 1.5f, 10, W - 10.0f);
            ty = clampf(ty + std::sin(ang) * 1.5f, 10, H - 10.0f);
        }
    }

    // boss arena near the end
    if (d.boss >= 0)
    {
        arenaX = W - 460;
        int ay = (int)path[std::min((size_t)(arenaX / 2), path.size() - 1)].y;
        ay = std::max(ay, 130);
        arenaFloor = ay + 36;
        for (int y = ay - 100; y < arenaFloor; y++)
            for (int x = arenaX - 150; x <= arenaX + 150; x++)
                if (world.in(x, y)) air[(size_t)y * W + x] = 1;
    }

    }
    if (d.surface) plainsCaveJoint(surf[W - 200]); // the caves run on east, under the castle

    NoiseGrid rockN(W, H, 4, [&](float x, float y) { return fbm(x * 0.025f, y * 0.035f, seed + 7, 3); });
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || (y < 4 && !d.surface) || x >= W - 4 || y >= H - 4) { place(x, y, M::Bedrock); continue; }
            if (air[(size_t)y * W + x]) continue;
            float n = rockN.at(x, y);
            M m = n > 0.62f ? d.alt : (n < 0.36f ? d.alt2 : d.base);
            if (x < D + 20 - fbm(y * 0.05f, 4.4f, seed, 2) * 40 && y < surf[x] + 12 + (int)(n * 10)) m = M::Sand; // the dunes
            place(x, y, m);
        }
    for (int x = 4; x < 120 && D; x++) // the sea, lapping the beach
        for (int y = 300; y < surf[x]; y++)
            if (world.at(x, y).material == M::Empty) place(x, y, M::Water);
    if (d.kind == SK_CRYPT) // crypts and citadel: every cave is lined with masonry
    {
        std::vector<uint8_t> near(air);
        for (int pass = 0; pass < 3; pass++)
        {
            std::vector<uint8_t> nx(near);
            for (int y = 1; y < H - 1; y++)
                for (int x = 1; x < W - 1; x++)
                    if (!near[(size_t)y * W + x] && (near[(size_t)y * W + x - 1] || near[(size_t)y * W + x + 1] || near[(size_t)(y - 1) * W + x] || near[(size_t)(y + 1) * W + x]))
                        nx[(size_t)y * W + x] = 1;
            near.swap(nx);
        }
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (near[(size_t)y * W + x] && !air[(size_t)y * W + x] && isRock(x, y)) place(x, y, M::Masonry);
    }
    if (d.surface) openPlainsJoint(surf[W - 200]);
    if (arenaX >= 0)
        for (int y = arenaFloor; y < arenaFloor + 5; y++)
            for (int x = arenaX - 155; x <= arenaX + 155; x++) place(x, y, M::Metal);
    buildBackground(d, d.surface); // decorations below paint over it
    if (deepLayout) deepFinish(d);
    if (plains) // the road to Dunmoor: farmsteads, watchtowers and palisades, then the moat and gatehouse
    {
        enum { B_HOUSE, B_TOWER, B_PALISADE, B_MINE, B_SMITH, B_APOTH, B_ARMORY };
        std::vector<int> plan{B_MINE, B_SMITH, B_APOTH, B_ARMORY}; // the trades scattered among the farmhouses
        for (int k = irange(3, 4); k > 0; k--) plan.push_back(B_HOUSE);
        for (int k = irange(1, 2); k > 0; k--) plan.push_back(B_TOWER);
        for (int k = irange(1, 2); k > 0; k--) plan.push_back(B_PALISADE);
        for (int i = (int)plan.size() - 1; i > 0; i--) std::swap(plan[i], plan[irand(i + 1)]);        int bx = D + irange(80, 110), mineAt = -1;
        for (int b : plan)
        {
            bool house = b == B_HOUSE || b >= B_SMITH;
            int w = house ? (b == B_ARMORY ? irange(112, 156) : irange(84, 156)) : (b == B_TOWER ? irange(48, 64) : 0);
            int span = house ? w + 16 : (b == B_TOWER ? w + 52 : (b == B_PALISADE ? 20 : 52));
            if (bx + span + (mineAt < 0 && b != B_MINE ? 52 + 70 : 0) > W - 1020) continue; // out of road (always leaving room for the mine, after the widest gap, and the gatehouse)
            if (b >= B_SMITH) placeHouse(bx + 8, w, b == B_SMITH ? TH_SMITH : b == B_APOTH ? TH_APOTH : TH_ARMORY);
            else if (b == B_HOUSE) placeHouse(bx + 8, w);
            else if (b == B_TOWER) placeWatchtower(bx + 26, w);
            else if (b == B_PALISADE) placePalisade(bx + 4);
            else mineAt = bx + 26;
            bx += span + irange(24, 70);
        }
        placeMine(mineAt);
    }
    connectPockets((int)path[3].x, (int)path[3].y, d.base, plains ? 300 : 600, plains); // plains: caves join the mine's network
    } // end of cave layout

    // floor dressing
    if (d.top != M::Empty)
        for (int x = 5; x < W - 5; x++)
            for (int y = 5; y < H - 5; y++)
            {
                M gm = world.at(x, y).material;
                if (!(gm == M::Dirt || gm == M::Stone || gm == M::Moss || gm == M::Basalt) || world.at(x, y - 1).material != M::Empty || world.at(x, y - 3).material != M::Empty) continue;
                if (d.top == M::Grass)
                {
                    int depth = irange(1, 3);
                    for (int k = 0; k < depth; k++)
                        if (isRock(x, y + k)) place(x, y + k, M::Grass);
                }
                else if (flatFloor(x, y)) // a powder (snow) only lies where it won't slide: on a slope it avalanches as soon as it's seen
                    for (int k = 1; k <= 2; k++) place(x, y - k, d.top);
            }

    // ore veins
    int oreCount = 0;
    for (auto& o : d.ores) if (o.w) oreCount++;
    for (int v = 0; v < (castle ? 30 : 130); v++)
    {
        int x = irange(10, W - 10), y = irange(10, H - 10);
        if (!isRock(x, y) || ((d.surface || castle) && y < surf[x] + 8) || x > W - 240) continue; // no ore in buildings
        M ore = (M)pickWeighted(d.ores, oreCount);
        int r = irange(2, 6);
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
            {
                if (dx * dx + dy * dy > r * r || hash2(x + dx, y + dy, seed + 3) < 0.25f) continue;
                if (isRock(x + dx, y + dy)) place(x + dx, y + dy, ore);
            }
    }

    // structures and hazards on floors along the level
    int enemyKinds = 0;
    for (auto& e : d.enemies) if (e.w) enemyKinds++;
    auto spots = findSpots(400, D + 120, W - 260, 16, 26);
    size_t si = 0;
    if (castle) // no traps or clutter in the grounds or the great hall
        spots.erase(std::remove_if(spots.begin(), spots.end(), [&](const Spot& p) { return p.y <= ends.sy; }), spots.end());
    auto next = [&](Spot& s) { if (si >= spots.size()) return false; s = spots[si++]; return true; };
    Spot sp;
    if (d.kind == SK_CRYPT)
        for (int i = 0, n = 0; n < 6 && next(sp) && i < 60; i++)
            if (offRoad(sp.x, sp.y, 40)) { placeCryptRoom(sp.x, sp.y); n++; }
    for (int i = 0; i < d.crates && next(sp); i++) placeCrate(sp.x, sp.y);
    for (int i = 0; i < d.kegs && next(sp); i++) placeKegs(sp.x, sp.y);
    for (int i = 0; i < d.vats && next(sp); i++) placeVat(sp.x, sp.y);
    for (int i = 0; i < d.spikes && next(sp); i++) placeSpikePit(sp.x, sp.y);
    for (int i = 0; i < d.arrows && next(sp); i++) placeArrowTrap(sp.x, sp.y);
    for (int i = 0; i < d.flames && next(sp); i++) placeFlameVent(sp.x, sp.y);
    for (int i = 0; i < d.collapses && next(sp); i++)
        if (offRoad(sp.x, sp.y, 50)) placeCollapse(sp.x, sp.y); // never drop the ceiling on the road
    if (d.kind == SK_MINES) for (size_t k = 40; k < path.size(); k += 30) { int fy; if (findFloor((int)path[k].x, (int)path[k].y, fy)) placeMineSupport((int)path[k].x, fy); }
    if (d.kind == SK_CRYPT) for (size_t k = 50; k < path.size(); k += 70) { int fy; if (findFloor((int)path[k].x, (int)path[k].y, fy)) placePillar((int)path[k].x, fy); }
    if (d.hang >= 0) // roots, icicles or chains hang from cave ceilings
        for (int i = 0; i < 500; i++)
        {
            int x = irange(10, W - 10), y = irange(10, H - 10);
            if (!isSolid(x, y) || world.at(x, y + 1).material != M::Empty) continue;
            if (d.surface && y < surf[x] + 10) continue;
            placeHanging(x, y + 1, d.hang);
        }
    dressCaves(d);
    if (d.kind == SK_CRYPT) for (int i = 0; i < 12 && next(sp); i++) paintCircle(sp.x, sp.y - 2, 3, M::Bone, true);
    if (d.surface) for (int x = D + 30; x < W - 340; x += irange(24, 70)) { int fy; if (std::abs(x - mineX) > 34 && findFloor(x, 4, fy) && std::abs(fy - surf[x]) < 4 && std::none_of(doorYards.begin(), doorYards.end(), [&](const std::pair<int, int>& r) { return x >= r.first - 8 && x <= r.second + 8; })) placeTree(x, fy, 15); }

    // liquids pool on floors, gas pockets float anywhere
    for (auto& l : findSpots(d.liquidCount * 3 / 2, D + 60, W - 260, 6, 10))
    {
        int r = irange(5, 11);
        paintCircle(l.x, l.y - r / 2, r, chance(3) ? d.liquid2 : d.liquid1, true);
    }
    for (int i = 0; i < d.miasma; i++)
    {
        int x = irange(30, W - 30), y = irange(20, H - 20);
        if (world.in(x, y) && world.at(x, y).material == M::Empty) paintCircle(x, y, irange(6, 10), M::Miasma, true);
    }

    if (s < STAGE_COUNT - 1) // the haven out of this biome, at its far edge
    {
        int hf = castle ? ends.ey : (d.surface ? surf[W - 200] : pathFloor[W - wingR - 250]);
        placeHaven(s, hf);
    }

    for (int i = 0; i < 20; i++) simulate(0, 0, W, H); // let liquids and loose stuff settle (the rest settles in play)
    if (!castle) connectPockets((int)path[3].x, (int)path[3].y, d.base, 300, d.kind == SK_PLAINS); // structures may have sealed a room or two
    if (d.kind == SK_PLAINS) // carved last, so no tunnel cuts into them
        for (int t = 0, n = 0; t < 600 && n < 2; t++) n += placeSealedPocket();

    if (D) decorateDunes(D);
    if (d.kind == SK_PLAINS) decorateBattlefield(W - 960, W - 232); // right up to the chieftain's hall at the waystone
    else if (castle) decorateBattlefield(14, 350, true, false);   // and on into the castle grounds, thinning out

    // torches mark the road; easy to lose among the side caves, but always there
    for (size_t i = 30; deepLayout && i < path.size(); i += irange(45, 65)) // in the deep biomes, along the way down
    {
        int x = (int)path[i].x, y;
        if (x > W - 260 || !findFloor(x, (int)path[i].y, y) || world.at(x, y - 12).material != M::Empty) continue;
        G.inter.push_back({IT_TORCH, (float)x, (float)y});
    }
    for (int x = D + 60; !castle && !deepLayout && x < W - 260; x += irange(90, 130))
    {
        int y = pathFloor[x];
        if (!isSolid(x, y) || world.at(x, y - 1).material != M::Empty || world.at(x, y - 12).material != M::Empty) continue;
        G.inter.push_back({IT_TORCH, (float)x, (float)y});
    }

    // the way in: from the beach, from the gatehouse, or a tunnel through the left wall from the last haven
    int fy;
    int sx = castle ? ends.sx : (D ? 116 : 24 + wingL);
    if (castle) fy = ends.sy;
    else if (!findFloor(sx, D ? 10 : (int)path[6].y, fy)) fy = (int)path[6].y + 8;
    clearRect(sx - 8, fy - 26, sx + 8, fy - 1);
    if (!D && !castle)
    {
        for (int y = fy - 46; y < fy; y++)
            for (int x = wingL; x < sx + 10; x++) world.at(x, y) = Cell{};
        for (int y = fy; y < fy + 6; y++)
            for (int x = wingL; x < sx + 10; x++) if (!isSolid(x, y)) place(x, y, d.base);
    }
    dressPlatforms();
    shadeBackWall(d.surface);
    pieceEntryX = D ? sx : 8 + wingL;
    pieceEntryFloor = fy;

    if (d.boss >= 0)
    {
        Mob b = makeEnemy(d.boss, (float)arenaX + 40, (float)arenaFloor);
        b.facing = -1;
        G.mobs.push_back(b);
        if (!G.havens.empty()) // the way on stays barred until it falls
        {
            Haven& h = G.havens.back();
            h.locked = true;
            h.bossId = b.id;
            for (int y = h.floor - 140; y < h.floor; y++) // a plug of dressed stone across the way, the gate in its foot
                for (int x = h.x0; x < h.x0 + HAVEN_WALL; x++) { place(x, y, M::Masonry); world.at(x, y).shade = (uint8_t)((y % 6 == 0 || (x + y / 6 * 4) % 8 == 0) ? 30 : 140 + irand(80)); }
            setGate(h.x0, h.x0 + HAVEN_WALL - 1, h.floor - HAVEN_DOOR, h.floor - 1, true);
        }
    }

    // creatures and chests
    spots = findSpots(500, D + 160, W - 260, 16, 30);
    int placed = 0, quota = (int)(d.enemyCount * 0.4f * (W - D) * H / (1200.0f * 640)); // bigger biomes, more to fight
    std::vector<Spot> taken; // spread out: no two groups within 48 of each other
    for (size_t i = 0; i < spots.size() && placed < quota; i++)
    {
        Spot e = spots[i];
        if (arenaX >= 0 && std::abs(e.x - arenaX) < 160) continue;
        if (std::abs(e.x - sx) < 120 && std::abs(e.y - fy) < 80) continue; // leave the way in quiet
        bool crowded = false;
        for (auto& t : taken) crowded = crowded || (std::abs(t.x - e.x) < 48 && std::abs(t.y - e.y) < 48);
        if (crowded) continue;
        taken.push_back(e);
        int type = pickWeighted(d.enemies, enemyKinds);
        int group = type == E_WOLF ? 2 : ((type == E_BAT || type == E_GOBLIN || type == E_SKELETON) && chance(4) ? 2 : 1);
        for (int k = 0; k < group; k++)
        {
            bool fly = type == E_BAT || type == E_IMP || type == E_WRAITH || type == E_BANSHEE;
            Mob m = makeEnemy(type, (float)e.x + k * 14, (float)e.y - (fly ? 30 : 0));
            if (boxSolid(m.x, m.y, m.w, m.h)) continue;
            G.mobs.push_back(m);
            placed++;
        }
    }
    for (auto& lo : lookouts) // archers man the towers
    {
        Mob a = makeEnemy(E_ARCHER, lo.x, lo.y);
        if (!boxSolid(a.x, a.y, a.w, a.h)) G.mobs.push_back(a);
    }
    std::vector<Spot> chestsPlaced;
    for (auto& c : findSpots(80, D + 100, W - 260, 20, 14)) // spread chests out across the level
    {
        bool crowded = false;
        for (auto& o : chestsPlaced) crowded = crowded || std::abs(o.x - c.x) + std::abs(o.y - c.y) < 110;
        if (crowded || (d.kind == SK_PLAINS && c.y < surf[c.x] + 75)) continue; // plains chests lie in the caves, not on the road
        addChest(c.x, c.y);
        chestsPlaced.push_back(c);
        if (chestsPlaced.size() >= (d.kind == SK_PLAINS ? 2u : 6u)) break;
    }
    // this stage's special weapon, displayed in a way that fits the place
    static const int styleFor[] = {DS_TARGET, DS_RACK, DS_GRAVE, DS_CART, DS_ICE, DS_ANVIL, DS_ALTAR};
    int style = styleFor[std::min(s, 6)];
    if (d.kind == SK_PLAINS)
    {
        std::vector<int> ranges; // archery butts out in the fields; the first has the stage's crossbow
        for (int t = 0; t < 300 && ranges.size() < 3; t++)
        {
            int x = irange(D + 140, W - 340), fy2;
            if (inFortZone(x - 26) || inFortZone(x + 20) || !findFloor(x, 10, fy2) || std::abs(fy2 - surf[x]) > 3 || std::abs(surf[x - 26] - surf[x + 20]) > 4) continue; // on open grass, not a roof
            bool near = false;
            for (int o : ranges) near = near || std::abs(o - x) < 80;
            if (near) continue;
            if (ranges.empty()) placeDisplay((float)x, (float)fy2, DS_TARGET, themedWeapon(DS_TARGET, s));
            paintArchery(x, fy2);
            ranges.push_back(x);
        }
        if (!cellars.empty()) // and a kitchen knife down in a cellar
        {
            const Cellar& c = cellars[irand((int)cellars.size())];
            placeDisplay((float)(c.x0 + c.x1) / 2, (float)c.floor, DS_TABLE, themedWeapon(DS_TABLE, s));
        }
    }
    else
        for (auto& st : findSpots(40, 150, W - 300, 24, 32))
            if (castle || offRoad(st.x, st.y, 40))
            {
                placeDisplay((float)st.x, (float)st.y, style, themedWeapon(style, s));
                break;
            }
}

// ---------------------------------------------------------------- upscaling a laid-out level to the play grid
// Levels are laid out at one cell per world unit; play happens on a grid twice as fine. Each cell becomes
// four. Natural ground is smoothed the way Scale2x smooths pixel art - a corner takes its neighbours' stuff
// where two of them agree - so cave walls get diagonal edges instead of stair steps; anything built
// (masonry, timber, planks, iron) keeps its square corners. Then each material gets its grain at the finer
// size: thinner mortar between courses, plank seams, wood grain, speckle in rock and earth.
static bool builtMaterial(M m)
{
    switch (m)
    {
    case M::Brick: case M::Masonry: case M::Wood: case M::Metal: case M::Platform: case M::Thatch:
    case M::Bedrock: case M::Keg: case M::Spikes: case M::Glass:
        return true;
    default: return false;
    }
}

static Cell fineGrain(Cell c, int fx, int fy)
{
    M m = c.material;
    if (m == M::Empty) return c;
    auto jitter = [&](int amp) { return (int)((hash2(fx, fy, seed + 33) - 0.5f) * 2 * amp); };
    auto clamp8 = [](int v) { return (uint8_t)std::max(0, std::min(255, v)); };
    if (m == M::Brick) // courses 4 cells high, bricks 8 long, joints one cell thick
    {
        bool mortar = fy % 4 == 0 || (fx + ((fy / 4) % 2) * 8) % 16 == 0;
        c.shade = mortar ? (uint8_t)irange(0, 25) : clamp8(std::max<int>(c.shade, 110) + jitter(14));
    }
    else if (m == M::Masonry)
    {
        // uneven blocks: each stretch of course sits a little high or low, rows start at random offsets, every block has its own
        // width and height, corners are chipped (a broken one now and then) and the mortar fills the gaps; the odd crack
        int ph = fy + (int)(hash2(fx / 30, 1, seed + 41) * 5), row = ph / 11, v = ph % 11;
        int xo = fx + (int)(hash2(row, 2, seed + 42) * 40) + (int)(3.0f * std::sin(fx * 0.09f + row * 1.7f)), col = xo / 22, u = xo % 22;
        float th = hash2(col, row, seed + 9);
        int bw = 20 - (int)(hash2(col, row, seed + 43) * 4), bh = 10 - (int)(hash2(col, row, seed + 44) * 3);
        int du = std::min(u, bw - 1 - u), dv = std::min(v, bh - 1 - v);
        bool mortar = u >= bw || v >= bh || du + dv < (th > 0.9f ? 5 : 2);
        bool crack = th > 0.8f && v > 2 && u == 4 + (v * 3 / 2 + (int)(hash2(col, row, seed + 45) * 14)) % std::max(1, bw - 8);
        c.shade = mortar ? (uint8_t)irange(0, 26) : crack ? (uint8_t)irange(4, 20) : clamp8((int)(70 + th * 120) + jitter(16) + (v == 1 ? 25 : 0) - (v >= bh - 2 ? 18 : 0));
    }
    else if (m == M::Platform)
        c.shade = (fx % 14 == 0) ? 15 : clamp8(c.shade + jitter(10));
    else if (m == M::Wood || m == M::Thatch)
        c.shade = clamp8(c.shade + ((fy + fx / 5) % 4 == 0 ? -28 : 0) + (int)((hash2(fx, fy / 7, seed + 5) - 0.5f) * 34) + (hash2(fx / 3, fy / 3, seed + 6) > 0.96f ? -40 : 0) + jitter(10)); // grain streaks one cell wide, the odd knot
    else if (props(m).kind == Kind::Liquid || props(m).kind == Kind::Gas || m == M::Fire)
        c.shade = (uint8_t)xr();
    else
        c.shade = clamp8(c.shade + jitter(28));
    return c;
}

// Moves the laid-out `world` onto a grid `k` (2) times finer. With `fineBack`, the back wall comes too, a
// colour per cell, doubled the way sprites are (Scale2x, then a lit rim and shade) - for small places full
// of painted detail, like Hearthwick; elsewhere it stays a colour per unit.
static void upscaleWorld(int k, bool fineBack = false)
{
    if (world.scale == k) return;
    World c = std::move(world);
    world = World{};
    worldInit(c.w * k, c.h * k, c.fill, c.fillBg, k, fineBack ? 0 : -1);
    if (fineBack)
    {
        std::vector<Color> back((size_t)c.w * c.h), fine;
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++) back[(size_t)y * c.w + x] = c.skyOf(x, y) ? Color{0, 0, 0, 0} : c.bgOf(x, y); // the sky isn't part of it
        detail2x(back.data(), c.w, c.h, fine);
        for (int y = 0; y < c.h * k; y++)
            for (int x = 0; x < c.w * k; x++)
            {
                Color f = fine[(size_t)y * c.w * k + x];
                world.bgAt(x, y) = f.a ? f : c.bgOf(x / k, y / k);
                world.skyAt(x, y) = c.skyOf(x / k, y / k);
            }
    }
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++)
        {
            const Cell& P = c.get(x, y);
            if (!fineBack)
            {
                world.bgAt(x * k, y * k) = c.bgOf(x, y);
                world.skyAt(x * k, y * k) = c.skyOf(x, y);
            }
            if (P.material == M::Empty && !c.chunk(x, y)) continue;
            const Cell &A = c.in(x, y - 1) ? c.get(x, y - 1) : P, &B = c.in(x + 1, y) ? c.get(x + 1, y) : P;
            const Cell &C = c.in(x - 1, y) ? c.get(x - 1, y) : P, &D = c.in(x, y + 1) ? c.get(x, y + 1) : P;
            auto same = [](const Cell& a, const Cell& b) { return a.material == b.material; };
            // Scale2x: which neighbour, if any, each quarter takes after
            const Cell* q[4] = {&P, &P, &P, &P};
            if (same(C, A) && !same(C, D) && !same(A, B)) q[0] = &A;
            if (same(A, B) && !same(A, C) && !same(B, D)) q[1] = &B;
            if (same(D, C) && !same(D, B) && !same(C, A)) q[2] = &C;
            if (same(B, D) && !same(B, A) && !same(D, C)) q[3] = &D;
            for (int j = 0; j < k; j++)
                for (int i = 0; i < k; i++)
                {
                    const Cell* src = q[(j * 2 / k) * 2 + (i * 2 / k)];
                    bool roof = P.material == M::Thatch && src->material == M::Thatch; // thatch is smoothed too: a roof pitch is one-cell steps, not two
                    if (!roof && (builtMaterial(src->material) != builtMaterial(P.material) || builtMaterial(P.material) || builtMaterial(src->material))) src = &P;
                    else if (!roof && k == 2 && src == &P)
                    { // ragged ground: now and then a quarter beside a different neighbour takes its stuff, so edges wander by a cell instead of running in two-cell lines
                        const Cell& H = (i ? B : C), &V = (j ? D : A);
                        float r = hash2(x * k + i, y * k + j, seed + 61);
                        auto rag = [&](const Cell& n) { return !builtMaterial(n.material) && (n.material == M::Empty || props(n.material).kind == Kind::Solid); };
                        if (rag(P) && r < 0.28f && !same(H, P) && rag(H)) src = &H;
                        else if (rag(P) && r > 0.72f && !same(V, P) && rag(V)) src = &V;
                    }
                    world.atq(x * k + i, y * k + j) = fineGrain(*src, x * k + i, y * k + j);
                }
        }
}

// ---------------------------------------------------------------- one continuous world
// The world is a row of biomes joined by havens. When a haven's gate drops behind you, the biome after
// next is built and joined on ahead. Nothing behind is ever thrown away: break back through and every
// biome is as you left it (the chunked world store keeps the rock between them free).

struct Piece
{
    World w;
    std::vector<Mob> mobs;
    std::vector<Interact> inter;
    std::vector<Trap> traps;
    std::vector<Lamp> lamps;
    std::vector<Pickup> pickups;
    std::vector<Weapon> stoneLoot;
    std::vector<Haven> havens;
    std::vector<int> road;
    int entryX = 0, entryFloor = 0, duneEnd = 0, lead = 0; // lead: units it reaches west of where it's joined on
    float stormX0 = 0, stormX1 = 0;
};
static Rectangle aheadRect; // the newest biome, in live-world coordinates

// Moves `world` and the per-world lists out into a piece (`live`: the running world, otherwise a freshly built biome).
static Piece takePiece(bool live)
{
    Piece p;
    p.w = std::move(world);
    world = World{};
    p.mobs.swap(G.mobs);
    p.inter.swap(G.inter);
    p.traps.swap(G.traps);
    p.lamps.swap(G.lamps);
    p.pickups.swap(G.pickups);
    p.stoneLoot.swap(G.stoneLoot);
    p.havens.swap(G.havens);
    if (live) p.road.swap(worldRoad);
    else p.road = roadValid && !deepLayout ? pathFloor : std::vector<int>(p.w.wU(), -1); // the deep biomes' way winds back on itself: no single road
    p.entryX = pieceEntryX;
    p.lead = live ? 0 : wingL;
    p.entryFloor = pieceEntryFloor;
    p.duneEnd = G.duneEnd;
    G.duneEnd = 0;
    p.stormX0 = G.stormX0; p.stormX1 = G.stormX1;
    G.stormX0 = G.stormX1 = 0;
    return p;
}

// Builds the new live world from the part of `live` inside `keep` (in cells), plus `next` joined with its
// entry at (attachX, attachFloor) (in units). Returns the offset, in units, applied to everything from `live`.
static Vector2 compose(Piece& live, Rectangle keep, Piece* next, int attachX, int attachFloor)
{
    const int K = live.w.scale; // cells per unit: things, roads and havens are in units
    int kx0 = std::max(0, (int)keep.x), ky0 = std::max(0, (int)keep.y);
    int kx1 = std::min(live.w.w, (int)(keep.x + keep.width)), ky1 = std::min(live.w.h, (int)(keep.y + keep.height));
    int bx0 = kx0, by0 = ky0, bx1 = kx1, by1 = ky1, ox = 0, oy = 0;
    if (next)
    {
        ox = (attachX - next->lead) * K;
        oy = (attachFloor - next->entryFloor) * K;
        bx0 = std::min(bx0, ox); by0 = std::min(by0, oy);
        bx1 = std::max(bx1, ox + next->w.w); by1 = std::max(by1, oy + next->w.h);
    }
    bx0 -= ((bx0 % CS) + CS) % CS; // keep the old world on the chunk grid, so its chunks move over rather than copy
    by0 -= ((by0 % CS) + CS) % CS;
    int NW = bx1 - bx0, NH = by1 - by0;
    Cell rock;
    rock.material = M::Basalt;
    worldInit(NW, NH, rock, {10, 10, 14, 255}, K); // whatever lies between the pieces is plain rock
    // Copies a source rectangle in, chunk by chunk. A whole chunk landing on the chunk grid just moves over,
    // and untouched chunks are skipped when their fill matches ours.
    // Cells landing left of skipX and above skipY are left alone: a wing tucked under the live world never overwrites it.
    auto blit = [&](World& src, int sx0, int sy0, int sx1, int sy1, int dx, int dy, int skipX = -1, int skipY = -1) {
        for (int cy = sy0 / CS; cy * CS < sy1; cy++)
            for (int cx = sx0 / CS; cx * CS < sx1; cx++)
            {
                auto& from = src.chunks[(size_t)cy * src.cw + cx];
                int x0 = std::max(sx0, cx * CS), y0 = std::max(sy0, cy * CS);
                int x1 = std::min(sx1, cx * CS + CS), y1 = std::min(sy1, cy * CS + CS);
                if (!from && src.fill.material == world.fill.material) continue;
                bool clear = x0 + dx >= skipX || y0 + dy >= skipY; // no part of this chunk is skipped
                if (from && clear && x1 - x0 == CS && y1 - y0 == CS && (x0 + dx) % CS == 0 && (y0 + dy) % CS == 0)
                {
                    world.chunks[(size_t)((y0 + dy) / CS) * world.cw + (x0 + dx) / CS] = std::move(from);
                    continue;
                }
                for (int y = y0; y < y1; y++)
                    for (int x = x0; x < x1; x++)
                    {
                        if (x + dx < skipX && y + dy < skipY) continue;
                        world.at(x + dx, y + dy) = src.get(x, y);
                        world.bgAt(x + dx, y + dy) = src.bgOf(x, y);
                        world.skyAt(x + dx, y + dy) = src.skyOf(x, y);
                    }
            }
    };
    float lx = (float)-bx0, ly = (float)-by0, nx = (float)(ox - bx0), ny = (float)(oy - by0);
    if (pieceRects.empty()) pieceRects.push_back({lx + kx0, ly + ky0, (float)(kx1 - kx0), (float)(ky1 - ky0)}); // a run's first biome
    else for (auto& r : pieceRects) { r.x += lx; r.y += ly; }
    if (next) pieceRects.push_back({nx, ny, (float)next->w.w, (float)next->w.h});
    dampOX += (int)lx;
    dampOY += (int)ly;
    blit(live.w, kx0, ky0, kx1, ky1, (int)lx, (int)ly);
    if (next) blit(next->w, 0, 0, next->w.w, next->w.h, (int)nx, (int)ny, (int)nx + next->lead * K, (int)ly + ky1);
    for (int x = 0; x < NW; x++) // above a piece that's open to the sky, the sky simply carries on upwards
    {
        int sx = x - (int)lx, top = -1;
        if (sx >= kx0 && sx < kx1) top = ky0 + (int)ly;
        int qx = x - (int)nx;
        if (next && qx >= 0 && qx < next->w.w) top = (int)ny;
        if (top <= 0 || !world.skyOf(x, top)) continue;
        Color topBg = world.bgOf(x, top);
        for (int y = 0; y < top; y++)
        {
            world.at(x, y) = Cell{};
            world.bgAt(x, y) = lerpColor(Color{4, 6, 16, 255}, topBg, 0.3f * y / top);
            world.skyAt(x, y) = 1;
        }
    }

    if (next && nx >= lx + kx1 - 4 * K) // the seam: each biome's rock and back wall give way to the other's, a few pixels at a time
    {
        int sx = (int)nx, B = 140 * K;
        int y0 = std::max((int)(ly + ky0), (int)ny), y1 = std::min((int)(ly + ky1), (int)ny + next->w.h);
        int s = irand(1 << 20);
        auto foreign = [&](int x, int y) { // how likely a pixel here takes the other side's: a half at the seam, none at the band's edge
            float t = 0.5f * (1 - std::fabs(x + 0.5f - sx) / B);
            return 0.7f * clampf((fbm(x * 0.012f, y * 0.012f, s, 3) - 0.5f) * 3.0f + 0.5f, 0, 1) + 0.3f * hash2(x, y, s + 1) < t; // (fbm bunches round a half: spread it)
        };
        const int R = 8 * K; // first the two pieces' bedrock rims along the seam melt into the rock behind them
        for (int y = std::max(0, y0); y < std::min(NH, y1); y++)
            for (int x = sx - R; x < sx + R; x++)
            {
                if (x < 0 || x >= NW || world.get(x, y).material != M::Bedrock) continue;
                int from = x < sx ? 2 * (sx - R) - 1 - x : 2 * (sx + R) - 1 - x; // mirrored out past the rim
                if (from < 0 || from >= NW) continue;
                world.at(x, y) = world.get(from, y);
                world.bgAt(x, y) = world.bgOf(from, y);
            }
        for (int y = std::max(0, y0); y < std::min(NH, y1); y++)
            for (int x = std::max(sx - B, (int)(lx + kx0)); x < sx; x++)
            {
                int xm = 2 * sx - 1 - x; // its mirror on the other side
                if (xm >= NW) continue;
                bool fa = foreign(x, y), fb = foreign(xm, y);
                if (!fa && !fb) continue;
                Cell a = world.get(x, y), b = world.get(xm, y);
                Color ba = world.bgOf(x, y), bb = world.bgOf(xm, y);
                bool rock = plainRock(a.material) && plainRock(b.material), back = !world.skyOf(x, y) && !world.skyOf(xm, y);
                if (fa) { if (rock) world.at(x, y) = b; if (back) world.bgAt(x, y) = bb; }
                if (fb) { if (rock) world.at(xm, y) = a; if (back) world.bgAt(xm, y) = ba; }
            }
    }

    auto kept = [&](float x, float y) { x *= K; y *= K; return x >= kx0 && x < kx1 && y >= ky0 - 60 * K && y < ky1; };
    auto take = [&](Piece& from, bool fromLive, float dx, float dy, int stoneBase) {
        for (auto& m : from.mobs)
            if (!fromLive || kept(m.cx(), m.cy())) { m.x += dx; m.y += dy; G.mobs.push_back(m); }
        for (auto& it : from.inter)
            if (!fromLive || kept(it.x, it.y))
            {
                it.x += dx; it.y += dy;
                if (it.type == IT_ROPE) it.data += (int)dy;
                if (it.type == IT_STONE) it.data += stoneBase;
                G.inter.push_back(it);
            }
        for (auto& t : from.traps)
            if (!fromLive || kept((float)t.x, (float)t.y) || kept((float)t.rx0, (float)t.ry0))
            {
                t.x += (int)dx; t.y += (int)dy; t.rx0 += (int)dx; t.rx1 += (int)dx; t.ry0 += (int)dy; t.ry1 += (int)dy;
                G.traps.push_back(t);
            }
        for (auto& l : from.lamps)
            if (!fromLive || kept(l.x, l.y)) { l.x += dx; l.y += dy; G.lamps.push_back(l); }
        for (auto& pu : from.pickups)
            if (!fromLive || kept(pu.b.x, pu.b.y)) { pu.b.x += dx; pu.b.y += dy; G.pickups.push_back(pu); }
        for (auto& h : from.havens)
            if (!fromLive || h.x1 * K >= kx0)
            {
                h.x0 += (int)dx; h.x1 += (int)dx; h.top += (int)dy; h.floor += (int)dy;
                G.havens.push_back(h);
            }
        for (auto& w : from.stoneLoot) G.stoneLoot.push_back(w);
    };
    float ulx = lx / K, uly = ly / K, unx = nx / K, uny = ny / K; // the same offsets, in units
    take(live, true, ulx, uly, 0);
    if (next) take(*next, false, unx, uny, (int)live.stoneLoot.size());

    worldRoad.assign(NW / K, -1);
    for (int x = kx0 / K; x < kx1 / K && x < (int)live.road.size(); x++)
        if (live.road[x] >= 0) worldRoad[x + (int)ulx] = live.road[x] + (int)uly;
    if (next)
        for (int x = 0; x < next->w.wU() && x < (int)next->road.size(); x++)
            if (next->road[x] >= 0) worldRoad[x + (int)unx] = next->road[x] + (int)uny;
    G.duneEnd = live.duneEnd > kx0 / K ? live.duneEnd + (int)ulx : 0;
    if (live.stormX1 > 0) { G.stormX0 = live.stormX0 + ulx; G.stormX1 = live.stormX1 + ulx; }
    else if (next && next->stormX1 > 0) { G.stormX0 = next->stormX0 + unx; G.stormX1 = next->stormX1 + unx; }
    if (G.seaEnd) G.seaEnd += (int)ulx;
    if (G.desert.width > 0) G.desert.x += ulx, G.desert.y += uly;
    if (next) aheadRect = {nx, ny, (float)next->w.w, (float)next->w.h};
    else aheadRect.x += lx, aheadRect.y += ly;
    G.playX0 = std::max(0, G.playX0 + (int)ulx);
    G.nearInteract = -1;
    return {ulx, uly};
}

static Piece buildPiece(int s, int entryFloor)
{
    int saved = G.stage;
    G.stage = s; // foes and loot are scaled for the biome they live in
    buildStage(s, entryFloor);
    G.stage = saved;
    upscaleWorld(2);
    return takePiece(false);
}

// ---------------------------------------------------------------- the Drowned Deep: the sea west of the landing
// A shelf off the beach, then a cliff down into the abyss. On the sea floor lie the ruins of a sunken city:
// broken terraces, columns and temples whose domes still hold a pocket of air. Water-filled caves wind off
// into the rock below. Kelpies hunt the open water; the drowned dead walk the terraces.
static Piece buildOcean()
{
    // The open sea west of the landing: shallows and kelp off the beach, the floor falling away westward over long
    // slopes and shelves to an abyss. The deeper it runs, the harder the things living there and the richer what
    // lies on the bottom (a tier per sixth of the way out). Breath is the limit: temples and hollow sea stacks keep
    // pockets of air all the way down.
    // The shallows run a long way out, a gentle shelf under moonlit water; then the floor simply ends. A cliff of
    // ledges falls away nearly a mile into the abyss, and the sunken city's second half lies on the bottom. Sea
    // caves in the cliff face hold pockets of air to breathe in on the way down.
    const int SL = 300; // sea level is the plains' sea row, so the two line up
    W = 4000; H = 2500;
    const int DEEP = H - 150, SHELF = 1900, DROP = 300; // the shelf reaches SHELF units out from the beach, then the floor falls away over DROP more
    worldInit(W, H);
    seed = irand(1 << 30);
    air.assign((size_t)W * H, 0);
    roadValid = false;
    deepLayout = false;
    int saved = G.stage;
    auto uOf = [&](int x) { return W - 1 - x; };                                      // units out from the beach
    auto tOf = [&](int x) { return clampf(uOf(x) / (float)(W - 140), 0, 1); };        // 0 at the beach .. 1 far out
    auto tierXY = [&](int y) { return std::min(5, std::max(0, (y - SL) * 6 / (DEEP - SL))); }; // what lives (and lies) at a depth
    std::vector<int> bed(W);
    for (int x = 0; x < W; x++)
    {
        float uw = uOf(x) + (fbm(x * 0.004f, 3.3f, seed + 5, 2) - 0.5f) * 140; // the lip wanders
        float t = clampf((uw - SHELF) / DROP, 0, 1), k = t * 8, f = k - std::floor(k);
        float stairs = (std::floor(k) + f * f * (3 - 2 * f)) / 8; // ledges with steep faces between
        float b = 330 + 280 * std::pow(clampf(uw / SHELF, 0, 1), 1.3f) + (DEEP - 610) * (0.35f * t * t * (3 - 2 * t) + 0.65f * stairs);
        b += (fbm(x * 0.008f, 5.5f, seed + 2, 3) - 0.5f) * 90 * clampf(uOf(x) / 400.0f, 0, 1) * (1 - t) + (fbm(x * 0.005f, 7.7f, seed, 3) - 0.5f) * 160 * t
             + (fbm(x * 0.03f, 2.2f, seed + 1, 2) - 0.5f) * 18;
        if (x >= W - 140) b = std::min(b, 324 + (W - 1 - x) * 0.6f); // the shelf off the beach
        if (x < 140) { float u = x / 140.0f; b = (SL - 40) + (b - SL + 40) * u * u * (3 - 2 * u); } // a headland walls it off in the west
        bed[x] = std::max(SL - 40, std::min(H - 30, (int)b));
    }
    auto onCliff = [&](int x) { return uOf(x) > SHELF - 60 && uOf(x) < SHELF + DROP + 60; };
    auto flatAt = [&](int x, int r, int tol) { return x - r >= 0 && x + r < W && std::abs(bed[x - r] - bed[x + r]) < tol && !onCliff(x - r) && !onCliff(x + r); };
    for (int k = 0; k < 16; k++) // jagged spires and ridges off the floor, taller the deeper
    {
        int x = irange(200, W - 400), r = irange(10, 22), h = (int)(irange(60, 200) * (0.4f + tOf(x)));
        if (onCliff(x)) continue;
        for (int dx = -r * 2; dx <= r * 2; dx++)
        {
            if (x + dx < 0 || x + dx >= W) continue;
            float u = 1 - std::fabs((float)dx) / (r * 2);
            bed[x + dx] = std::max(SL + 30, std::min(bed[x + dx], bed[x] - (int)(h * u * u)));
        }
    }
    // caves: worms burrowing from the sea floor down into the rock, a chamber at the end of each
    std::vector<Vector2> caveEnds;
    for (int k = 0; k < 14; k++)
    {
        float x = (float)irange(200, W - 400), y = (float)bed[(int)x] + 2, ang = frange(0.6f, 2.5f);
        int len = irange(160, 320);
        for (int i = 0; i < len; i++)
        {
            ang += frange(-0.25f, 0.25f);
            ang = clampf(ang, 0.3f, PI - 0.3f); // always downward-ish
            x = clampf(x + std::cos(ang) * 2, 20, (float)W - 20);
            y = clampf(y + std::sin(ang) * 2, 20, (float)H - 40);
            carve(x, y, 8 + (int)(fbm(i * 0.05f, (float)k, seed + 3, 2) * 6));
            if (i % 90 == 89) carve(x, y, irange(16, 24)); // a grotto on the way
        }
        carve(x, y, irange(22, 30));
        caveEnds.push_back({x, y});
    }
    // sea caves in the cliff face: a tunnel in from open water ending in a round chamber whose upper half holds air
    struct Dome { int cx, cy, r, level; };
    std::vector<Dome> domes;
    for (int k = 0; k < 9; k++)
    {
        int yc = SL + 420 + k * (DEEP - SL - 560) / 8 + irange(-40, 40), x0 = std::max(30, W - 1 - SHELF - DROP - 250);
        while (x0 < W - 200 && bed[x0] > yc) x0++; // the cliff face at this depth
        float x = (float)x0 + 2, y = (float)yc, ang = frange(-0.12f, 0.12f);
        for (int i = 0, len = irange(150, 230); i < len; i++)
        {
            ang = clampf(ang + frange(-0.08f, 0.08f), -0.3f, 0.3f);
            x += std::cos(ang) * 2; y += std::sin(ang) * 2;
            carve(x, y, 8);
        }
        int r = irange(24, 30), cy = (int)y - 10;
        carve(x, (float)cy, r);
        domes.push_back({(int)x, cy, r, cy - 2});
        caveEnds.push_back({x, (float)cy + r - 6});
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            bool rock = y >= bed[x] && !air[(size_t)y * W + x];
            if (x < 4 || y >= H - 4) { place(x, y, M::Bedrock); continue; }
            if (rock)
            {
                float n = fbm(x * 0.03f, y * 0.04f, seed + 7, 3);
                // sand only lies where it can rest: on gentle ground with rock beneath, or it pours down the slopes for ever
                bool rests = x > 0 && x < W - 1 && std::abs(bed[x + 1] - bed[x - 1]) <= 2;
                for (int k = 1; k < 16 && rests; k++) rests = y + k >= H || !air[(size_t)(y + k) * W + x];
                place(x, y, rests && y < bed[x] + 4 + (int)(n * 6) ? (y > SL ? M::WetSand : M::Sand) : (n > 0.6f ? M::Basalt : M::Stone));
                if (y >= bed[x] + 4 && hash2(x / 5, y / 5, seed + 33) > 0.985f - 0.01f * tOf(x)) place(x, y, M::Glowmoss); // more of it in the dark
            }
            else if (y >= SL) place(x, y, M::Water);
            else world.at(x, y) = Cell{};
        }
    for (auto& d : domes) // the air in a sea cave's chamber: dry above its water line, a glowing roof over it
    {
        for (int y = d.cy - d.r; y < d.level; y++)
            for (int x = d.cx - d.r; x <= d.cx + d.r; x++)
                if (world.in(x, y) && air[(size_t)y * W + x] && (x - d.cx) * (x - d.cx) + (y - d.cy) * (y - d.cy) <= d.r * d.r) world.at(x, y) = Cell{};
        for (int a = 0; a < 60; a++) // moss in the ceiling
        {
            float t = PI + a * (PI / 59);
            for (int r = d.r - 2; r < d.r + 6; r++)
            {
                int x = d.cx + (int)(std::cos(t) * r), y = d.cy + (int)(std::sin(t) * r);
                if (world.in(x, y) && isRock(x, y)) { place(x, y, M::Glowmoss); break; }
            }
        }
        G.lamps.push_back({(float)d.cx, (float)d.cy - 8, 96, {110, 220, 190, 255}}); // seen from afar in the dark water
    }
    for (auto& e : caveEnds) // glowing moss lines the grottoes at the bottom of the caves
        for (int a = 0; a < 40; a++)
        {
            float t = a * 0.157f;
            for (int r = 20; r < 40; r++)
            {
                int x = (int)(e.x + std::cos(t) * r), y = (int)(e.y + std::sin(t) * r);
                if (world.in(x, y) && isRock(x, y)) { place(x, y, M::Glowmoss); break; }
            }
        }
    // the back wall: night sky above the waves; below, green-blue water darkening to black in the abyss
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            float n = fbm(x * 0.05f, y * 0.05f, seed + 51, 2);
            if (y < SL && y < bed[x])
            {
                float k = clampf((float)y / SL, 0, 1);
                world.bgAt(x, y) = lerpColor(Color{4, 6, 16, 255}, Color{30, 38, 68, 255}, k * k);
                world.skyAt(x, y) = 1;
                continue;
            }
            float d = clampf((y - SL) / (float)(DEEP - SL), 0, 1);
            world.bgAt(x, y) = brighten(lerpColor(lerpColor(Color{26, 70, 74, 255}, Color{14, 40, 58, 255}, std::min(1.0f, d * 2)), Color{3, 6, 12, 255}, std::sqrt(d)), (int)((n - 0.5f) * 14));
            world.skyAt(x, y) = 0;
        }
    for (int x = 150; x < W - 20; x += irange(2, 9)) // kelp swaying up off the bottom: a forest in the shallows, wisps further out
    {
        if (uOf(x) > SHELF - 100 && !chance(4)) continue;
        int len = irange(20, std::max(21, std::min(220, bed[x] - SL - 10)));
        for (int k = 0; k < len; k++)
            bgPut(x + (int)(std::sin(k * 0.12f + x) * 3), bed[x] - k, shadeC(Color{40, 110, 60, 255}, 0.6f + 0.4f * (k % 5 == 0)));
    }
    auto block = [&](int x0, int y0, int x1, int y1, M m) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (world.in(x, y) && world.at(x, y).material != M::Bedrock) place(x, y, m); };
    std::vector<Spot> floors; // where things stand
    // the sunken city: terraces on columns, first out on the shelf and then, far below, on the abyss floor
    struct Terrace { int x0, x1, y; };
    std::vector<Terrace> terr;
    for (int band = 0; band < 2; band++)
        for (int x = W - 1 - (band ? SHELF + DROP + 260 : 420); x > W - 1 - (band ? SHELF + DROP + 1500 : SHELF - 150); x -= irange(150, 260))
        {
            int w = irange(90, 170);
            if (x < 0 || x + w >= W || onCliff(x) || onCliff(x + w)) continue;
            int y = std::min(bed[x], bed[x + w]) - (band ? irange(60, 200) : irange(40, 110));
            if (y < SL + 70) continue;
            terr.push_back({x, x + w, y});
        }
    for (auto& tr : terr)
    {
        for (int x = tr.x0; x <= tr.x1; x++) // the slab, its ends broken off
            for (int y = tr.y; y < tr.y + 6; y++)
                if (!((x - tr.x0 < 6 || tr.x1 - x < 6) && hash2(x, y, seed + 4) > 0.55f)) place(x, y, M::Masonry);
        for (int cx = tr.x0 + 10; cx < tr.x1 - 8; cx += irange(28, 44)) // columns down to whatever is below
        {
            int y = tr.y + 6;
            while (y < H - 5 && world.at(cx, y).material == M::Water) y++;
            block(cx, tr.y + 6, cx + 5, y, M::Masonry);
            if (chance(2)) block(cx - 2, tr.y + 6, cx + 7, tr.y + 8, M::Masonry); // a capital
        }
        for (int cx = tr.x0 + 14; cx < tr.x1 - 14; cx += irange(26, 40)) // broken columns standing on it
            if (chance(2)) { int h = irange(8, 40); block(cx, tr.y - h, cx + 5, tr.y - 1, M::Masonry); if (h > 30) block(cx - 2, tr.y - h, cx + 7, tr.y - h + 2, M::Masonry); }
        if (chance(2)) block(tr.x0 + 6, tr.y - 1, tr.x0 + 14, tr.y - 1, M::Glowmoss);
        floors.push_back({(tr.x0 + tr.x1) / 2, tr.y});
    }
    // air: a temple or a hollow sea stack every few hundred units of the way out. Water can't climb, so a chamber
    // above its doorway keeps its air; a dais (or ledge) just under the water line lets you stand with your head in it.
    auto temple = [&](int cx, int fl) {
        int hw = 30, wallH = 50, door = 26;
        block(cx - hw - 4, fl, cx + hw + 4, fl + 6, M::Masonry); // its footing
        for (int y = fl - wallH - hw; y < fl; y++)
            for (int x = cx - hw; x <= cx + hw; x++)
            {
                float dy = (float)(y - (fl - wallH)), dx = (float)(x - cx);
                bool inDome = y >= fl - wallH || dx * dx + dy * dy < (hw - 4) * (hw - 4);
                bool shell = !inDome && dx * dx + dy * dy < hw * hw;
                bool wall = (std::abs(x - cx) >= hw - 4) && y >= fl - wallH;
                if (wall) place(x, y, y < fl - door ? M::Masonry : M::Water);
                else if (shell) place(x, y, M::Masonry);
                else if (inDome) place(x, y, y >= fl - door ? M::Water : M::Empty);
            }
        block(cx - 12, fl - door + 6, cx + 12, fl - 1, M::Masonry);
        block(cx - 14, fl - wallH - hw + 2, cx + 14, fl - wallH - hw + 3, M::Glowmoss);
        addChest(cx, fl - door + 6);
        addSconce(cx - hw + 5, fl - wallH + 10, 1);
    };
    auto stack = [&](int cx) { // a sea stack: a rock pillar off the floor with a cave of air inside, its mouth at the foot
        int fl = bed[cx], hw = irange(26, 36), h = irange(120, 200);
        for (int y = fl - h; y < fl + 6; y++)
            for (int x = cx - hw; x <= cx + hw; x++)
            {
                float u = (float)(x - cx) / hw, v = (float)(fl - y) / h;
                if (u * u < 1 - v * v * 0.6f) place(x, y, hash2(x / 3, y / 3, seed + 90) > 0.7f ? M::Basalt : M::Stone);
            }
        int top = fl - h + 30, door = 22;
        for (int y = top; y < fl; y++) // the hollow: air above the doorway's top, water below it
            for (int x = cx - hw + 9; x <= cx + hw - 9; x++)
            {
                float u = (float)(x - cx) / (hw - 9), v = (float)(y - top) / (fl - top);
                if (u * u + (v < 0.3f ? (0.3f - v) * (0.3f - v) * 11 : 0) > 1) continue;
                place(x, y, y >= fl - door ? M::Water : M::Empty);
            }
        for (int y = fl - door; y < fl; y++) for (int x = cx + hw - 10; x <= cx + hw + 1; x++) place(x, y, M::Water); // its mouth, facing the beach
        block(cx - 10, fl - door + 5, cx + 4, fl - 1, M::Stone); // a ledge under the air
        for (int x = cx - hw + 12; x < cx + hw - 12; x += 3) if (isRock(x, top - 1)) place(x, top - 1, M::Glowmoss); // its roof glows
        if (chance(2)) addChest(cx - 3, fl - door + 5);
    };
    int nextAir = W - 260, temples = 0;
    while (nextAir > 300) // (the cliff has its own air, in the sea caves)
    {
        for (int pass = 0; pass < 3; pass++) // never under a sunken terrace: its slab and columns would wall the doorways in
            for (auto& t : terr)
                if (nextAir + 70 > t.x0 && nextAir - 70 < t.x1) nextAir = (nextAir - t.x0 < t.x1 - nextAir) ? t.x0 - 75 : t.x1 + 75;
        if (nextAir <= 300 || nextAir >= W - 100) break;
        G.stage = tierXY(bed[nextAir]);
        if (!onCliff(nextAir) && flatAt(nextAir, 34, 90))
        {
            if (flatAt(nextAir, 34, 30) && temples < 6 && chance(2)) { temple(nextAir, std::min(bed[nextAir - 34], bed[nextAir + 34])); temples++; }
            else stack(nextAir);
            floors.push_back({nextAir, bed[nextAir]});
        }
        nextAir -= irange(260, 380);
    }
    for (auto& e : caveEnds) { int fy; G.stage = tierXY((int)e.y); if (findFloor((int)e.x, (int)e.y - 10, fy)) addChest((int)e.x, fy); }
    // wrecks: a longship that never made the shore, and others sunk further out
    std::vector<int> wrecks;
    auto wreck = [&](int cx) {
        wrecks.push_back(cx);
        G.stage = tierXY(bed[cx]);
        int hw = irange(34, 48), depth = irange(13, 18);
        float tilt = frange(-0.12f, 0.12f);
        int deck = bed[cx] - depth + 4; // settled into the sand
        for (int dx = -hw; dx <= hw; dx++)
        {
            float u = (float)dx / hw;
            int lift = (int)(dx * tilt), yb = deck + (int)(depth * std::sqrt(std::max(0.0f, 1 - u * u))) + lift;
            int y0 = std::fabs(u) > 0.88f ? deck - 6 + lift - (int)((std::fabs(u) - 0.88f) * 60) : yb - 2; // prow and stern rear up
            for (int y = y0; y <= yb; y++)
                if (hash2(dx / 3, y / 4, seed + 70) > 0.18f) place(cx + dx, y, M::Wood); // sprung planks
        }
        int sdir = chance(2) ? -1 : 1; // an anchor lost over the side, half sunk in the sand at 45 degrees
        int ax = std::max(8, std::min(W - 8, cx + sdir * (hw + irange(8, 14))));
        {
            const float ang = sdir * 0.785f, cs = std::cos(ang), sn = std::sin(ang);
            auto cell = [&](float u, float v) { // u up the shank from the crown, v across it; only the part standing out of the sand is built
                int x = ax + (int)std::lround(u * sn + v * cs), y = bed[ax] + 3 - (int)std::lround(u * cs - v * sn);
                if (world.in(x, y) && (world.at(x, y).material == M::Empty || world.at(x, y).material == M::Water)) { place(x, y, M::Metal); world.at(x, y).shade = (uint8_t)(120 + irand(80)); }
            };
            for (float u = 0; u <= 17; u += 0.5f) { cell(u, 0); cell(u, 1); }                       // the shank
            for (float v = -5; v <= 5; v += 0.5f) { cell(13, v); cell(14, v); }                      // the stock, a bar across the top
            cell(13, -6); cell(14, -6); cell(13, 6); cell(14, 6);                                    // its knobs
            for (float a2 = 0; a2 < 6.3f; a2 += 0.25f) cell(19.5f + 2.2f * std::sin(a2), 0.5f + 2.2f * std::cos(a2)); // the ring
            for (int sd = -1; sd <= 1; sd += 2)                                                      // the arms: a crescent sweeping up from the crown to a fluke
                for (float t = 0; t <= 1; t += 0.04f)
                {
                    float v = sd * (7.5f * t) + 0.5f, u = 6.5f * t * t;
                    cell(u, v); cell(u + 1, v);
                    if (t > 0.88f) { cell(u + 2, v - sd); cell(u + 3, v - sd * 2); cell(u + 2, v - sd * 2); } // the fluke's point
                }
        }
        { // a sea chest on the sand beside the wreck: it is drawn half buried (entities.cpp:drawEntities)
            int chx = std::max(12, std::min(W - 12, cx - sdir * (hw + irange(10, 18))));
            addChest(chx, bed[chx]);
        }
        addCoins((float)cx, (float)deck, irange(2, 5) + G.stage, 1);
    };
    wreck(W - 80); // in the shallows, a stone's throw from where you land
    for (int k = 0; k < 5; k++)
        for (int tries = 0; tries < 60; tries++) { int cx = irange(300, W - 700); if (flatAt(cx, 48, 22)) { wreck(cx); break; } }
    { // broken planks adrift on the surface, the wrecks' remains: rigid bodies that float, bob and drift (entities.cpp:rigidStep with buoyancy), clustered round each wreck
        std::vector<int> spots;
        for (int w : wrecks) for (int k = 0; k < 6; k++) spots.push_back(w + irange(-120, 120));
        for (int k = 0; k < 16; k++) spots.push_back(irange(300, W - 200));
        for (int x : spots)
        {
            if (x < 20 || x >= W - 20 || bed[x] < SL + 8) continue;
            addProp(x, SL + 2, 3 * irange(1, 3));
            G.inter.back().ang = frange(-0.18f, 0.18f);
        }
    }
    for (int k = 0; k < 3; k++)
    {
        int cx = 0;
        for (int tries = 0; tries < 60 && !cx; tries++) { int c = irange(320, W - 1000); if (flatAt(c + 70, 90, 20)) cx = c; }
        if (!cx) continue;
        int fy = bed[cx]; // a whale's bones, picked clean
        for (int dx = -70; dx <= 70; dx++)
        {
            int sy = fy - 16 + (int)(std::sin(dx * 0.03f) * 4);
            for (int t = 0; t < 3; t++) place(cx + dx, sy + t, M::Bone); // the spine
            if ((dx + 70) % 9 == 0 && std::abs(dx) < 56) // ribs arching down to the sand
                for (int k2 = 0; k2 < 18; k2++) place(cx + dx + (int)(std::sin(k2 * 0.12f) * 6), sy + 2 + k2, M::Bone);
        }
        for (int dy = -9; dy <= 9; dy++) // the skull
            for (int dx = 0; dx < 26; dx++)
                if (dx * dx / 676.0f + dy * dy / 81.0f < 1 && !(dx > 14 && dx < 20 && dy < -2)) place(cx + 70 + dx, fy - 14 + dy, M::Bone);
        addCoins((float)cx, (float)fy - 24, irange(2, 4), 1);
    }
    // the dead walk the bottom; the hungry swim. What lives here gets worse the further out you go.
    for (auto& f : floors)
    {
        G.stage = tierXY(f.y);
        int type = G.stage >= 3 ? (chance(2) ? E_DRAUGR : E_SERPENT) : (chance(2) ? E_DRAUGR : E_SKELETON);
        Mob m = makeEnemy(type, (float)f.x + irange(-40, 40), (float)f.y - (type == E_SERPENT ? 30 : 0));
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
    }
    for (int x = W - 260; x > 200; x -= irange(70, 130)) // the hungry swim: out over the shelf, and thick along the cliff and the abyss
    {
        int y = irange(SL + 40, std::max(SL + 41, bed[x] - 30));
        G.stage = tierXY(y);
        int t = G.stage;
        int type = t >= 2 && chance(t >= 4 ? 2 : 4) ? E_SERPENT : E_KELPIE;
        Mob m = makeEnemy(type, (float)x, (float)y);
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
    }
    for (int k = 0; k < 18; k++) { int x = irange(160, W - 320); G.stage = tierXY(bed[x]); addCoins((float)x, (float)bed[x] - 10, irange(1, 3) + G.stage, 1); }
    { // nothing may be sealed away: every chest must be swimmable from the beach. Whatever isn't gets the shortest tunnel (water-filled) to open water
        std::vector<uint8_t> seen((size_t)W * H, 0);
        std::vector<int> q;
        auto open = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && !isSolid(x, y); };
        q.push_back(SL * W + W - 6);
        seen[q[0]] = 1;
        for (size_t i = 0; i < q.size(); i++)
        {
            int x = q[i] % W, y = q[i] / W;
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; d++)
            {
                int nx = x + dx[d], ny = y + dy[d];
                if (open(nx, ny) && !seen[(size_t)ny * W + nx]) { seen[(size_t)ny * W + nx] = 1; q.push_back(ny * W + nx); }
            }
        }
        for (auto& it : G.inter)
        {
            if (it.type != IT_CHEST) continue;
            int cx = (int)it.x, cy = (int)it.y - 4;
            bool ok = false;
            for (int dy = -4; dy <= 4 && !ok; dy++) for (int dx = -5; dx <= 5 && !ok; dx++) ok = open(cx + dx, cy + dy) && seen[(size_t)(cy + dy) * W + cx + dx];
            if (ok || !world.in(cx, cy)) continue;
            std::vector<int> from((size_t)W * H, -1), bq{cy * W + cx}; // breadth-first through anything, to the nearest open water
            from[bq[0]] = bq[0];
            int hit = -1;
            for (size_t i = 0; i < bq.size() && hit < 0; i++)
            {
                int x = bq[i] % W, y = bq[i] / W;
                const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4 && hit < 0; d++)
                {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 5 || ny < 5 || nx >= W - 5 || ny >= H - 5 || from[(size_t)ny * W + nx] >= 0) continue;
                    from[(size_t)ny * W + nx] = bq[i];
                    bq.push_back(ny * W + nx);
                    if (seen[(size_t)ny * W + nx]) hit = ny * W + nx;
                }
            }
            for (int k = hit; k >= 0 && from[k] != k; k = from[k])
                for (int dy = -3; dy <= 3; dy++)
                    for (int dx = -3; dx <= 3; dx++)
                        if (dx * dx + dy * dy <= 10 && world.in(k % W + dx, k / W + dy) && isSolid(k % W + dx, k / W + dy) && world.at(k % W + dx, k / W + dy).material != M::Bedrock) place(k % W + dx, k / W + dy, M::Water);
        }
    }
    for (auto& m : G.mobs) m.sea = true; // what lives out here may carry Önd
    for (auto& it : G.inter) if (it.type == IT_CHEST) it.style = 1; // and the sea's chests too
    G.stage = saved;
    pieceEntryX = W - 1;
    pieceEntryFloor = SL;
    upscaleWorld(2);
    return takePiece(false);
}

// ---------------------------------------------------------------- the Scorched Reach: the desert east of Dunmoor
// Wind-shaped dunes over banded sandstone, mesas you can walk up, a stepped temple with a chamber inside, an oasis
// under palms, the bones of something enormous, and a raider camp. Giant scorpions hunt the surface and lair in
// burrows under it. Stitched onto Dunmoor's crag at ground level; it runs a long way east and ends at a cliff.
static Piece buildDesert()
{
    W = 3200; H = 1650; // (the crypts lie below its west end, from y 1700 on: stop short of them)
    const int G0 = 380; // the ground at the west edge, where it meets the crag
    worldInit(W, H);
    seed = irand(1 << 30);
    air.assign((size_t)W * H, 0);
    roadValid = false;
    deepLayout = false;
    int saved = G.stage;
    G.stage = 2;
    std::vector<int> s(W);
    float ph = frange(0, 6.28f);
    for (int x = 0; x < W; x++)
    {
        float r = clampf(x / 220.0f, 0, 1); // level where it meets the crag
        float dune = std::sin(x * 0.011f + ph) * 24 + std::sin(x * 0.0047f + ph * 2) * 36 + (fbm(x * 0.004f, 3.3f, seed, 3) - 0.5f) * 70;
        s[x] = G0 + (int)(dune * r);
    }
    auto raise = [&](int mx, int hw, int mh) { // a mesa: a flat top, flanks no steeper than you can walk
        int base = s[mx];
        for (int x = std::max(0, mx - hw - mh * 2); x < std::min(W, mx + hw + mh * 2); x++)
        {
            float k = clampf((hw + mh * 1.4f - std::abs(x - mx)) / (mh * 1.4f), 0, 1);
            s[x] = std::min(s[x], base + 10 - (int)(mh * k));
        }
    };
    for (int k = 0; k < 3; k++) raise(irange(500 + k * 850, 800 + k * 850), irange(60, 130), irange(50, 110));
    for (int x = W - 160; x < W; x++) s[x] = std::min(s[x], s[W - 161] - (int)((x - (W - 160)) * 1.6f)); // the cliff it ends at
    auto flatten = [&](int x0, int x1) { int y = s[(x0 + x1) / 2]; for (int x = std::max(0, x0); x < std::min(W, x1); x++) s[x] = y; return y; };
    int templeX = irange(1150, 1450), campX = irange(1900, 2200), oasisX = irange(650, 900);
    int templeY = flatten(templeX - 130, templeX + 130), campY = flatten(campX - 140, campX + 140);
    int oasisY = flatten(oasisX - 90, oasisX + 90);
    for (int pass = 0; pass < 2; pass++) // nothing steeper than a walkable 45 degrees: the high side gives way
    {
        for (int x = 1; x < W - 160; x++) s[x] = std::max(s[x], s[x - 1] - 1);
        for (int x = W - 162; x >= 0; x--) s[x] = std::max(s[x], s[x + 1] - 1);
    }
    templeY = s[templeX]; campY = s[campX]; oasisY = s[oasisX];
    for (int x = oasisX - 70; x < oasisX + 70; x++) { float u = (x - oasisX) / 70.0f; s[x] = oasisY + (int)(26 * (1 - u * u)); } // the oasis' hollow
    for (auto& v : s) v = std::max(80, std::min(H - 220, v));
    // scorpion burrows: tunnels in from the surface, winding down to a lair (connected by construction)
    std::vector<Vector2> lairs;
    for (int k = 0; k < 6; k++)
    {
        int x0 = irange(300, W - 300);
        if (std::abs(x0 - templeX) < 200 || std::abs(x0 - campX) < 200 || std::abs(x0 - oasisX) < 150) continue;
        float x = (float)x0, y = (float)s[x0] - 4, ang = frange(0.9f, 2.2f);
        int len = irange(140, 260);
        for (int i = 0; i < len; i++)
        {
            ang = clampf(ang + frange(-0.2f, 0.2f), 0.35f, PI - 0.35f);
            x = clampf(x + std::cos(ang) * 2, 30, (float)W - 200);
            y = clampf(y + std::sin(ang) * 1.6f, 0, (float)H - 60);
            carve(x, y, 9 + (int)(fbm(i * 0.04f, (float)k, seed + 5, 2) * 4));
            if (i == len / 2) { carve(x, y, irange(18, 24)); lairs.push_back({x, y + 16}); }
        }
        carve(x, y, irange(24, 32));
        lairs.push_back({x, y + 24});
    }
    // The underground desert: wind-carved galleries and wide caverns under the dunes, down to the foot of the map,
    // and two dressed-stone vaults deep in it. The caves are joined to the burrows above by connectPockets.
    struct Vault { int x0, y0, x1, y1; };
    std::vector<Vault> vaults;
    surf = s;
    keepZones.clear();
    {
        NoiseGrid tube(W, H, 4, [&](float x, float y) { // long winding galleries: ridges of warped noise
            float wx = (fbm(x * 0.004f, y * 0.004f, seed + 300, 2) - 0.5f) * 260, wy = (fbm(x * 0.004f, y * 0.004f, seed + 301, 2) - 0.5f) * 260;
            return 1 - std::fabs(2 * fbm((x + wx) * 0.005f, (y + wy) * 0.011f, seed + 310, 3) - 1);
        });
        NoiseGrid cav(W, H, 4, [&](float x, float y) { return fbm(x * 0.0045f, y * 0.0095f, seed + 320, 4); }); // wide, flat-bottomed halls
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (y > s[x] + 60 && y < H - 40 && x > 24 && x < W - 40 && (tube.at(x, y) > 0.935f || cav.at(x, y) > 0.625f)) air[(size_t)y * W + x] = 1;
        for (int k = 0; k < 2; k++) // the vaults: one in the middle depths, one near the bottom
        {
            int w = irange(190, 250), h = irange(64, 80), x0 = k ? irange(1900, W - 600) : irange(500, 1400), y0 = k ? H - 190 : irange(760, 980);
            vaults.push_back({x0, y0, x0 + w, y0 + h});
            for (int y = y0; y <= y0 + h; y++) for (int x = x0; x <= x0 + w; x++) air[(size_t)y * W + x] = 1;
        }
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x >= W - 4 || y >= H - 4 || (x < 4 && y >= s[x] + 14)) { place(x, y, M::Bedrock); continue; }
            if (y < s[x]) { world.at(x, y) = Cell{}; continue; }
            if (air[(size_t)y * W + x]) { world.at(x, y) = Cell{}; continue; } // (the burrows' mouths open onto the sand)
            int depth = y - s[x];
            bool gentle = x > 0 && x < W - 1 && std::abs(s[x + 1] - s[x - 1]) <= 2;
            if (depth < 4 + (int)(fbm(x * 0.05f, 1.1f, seed + 3, 2) * 4) && gentle) { place(x, y, M::Sand); continue; }
            place(x, y, depth > 300 && fbm(x * 0.01f, y * 0.01f, seed + 9, 2) > (depth > 800 ? 0.45f : 0.55f) ? M::Stone : M::Sandstone);
            if (world.at(x, y).material == M::Sandstone) // strata: bands that wander a little, darker the deeper they lie
                world.at(x, y).shade = (uint8_t)clampf((130 + 70 * std::sin(y * 0.33f + fbm(x * 0.006f, 0.5f, seed + 4, 2) * 9) + irand(40) - 20) * (1 - 0.3f * clampf(depth / 1200.0f, 0, 1)), 0, 255);
        }
    for (int x = oasisX - 70; x < oasisX + 70; x++) // the oasis
        for (int y = oasisY + 3; y < s[x]; y++) place(x, y, M::Water);
    // the back wall: the night sky, and below ground the sandstone's own darker bands
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (y < s[x])
            {
                float k = clampf((float)y / G0, 0, 1);
                world.bgAt(x, y) = lerpColor(Color{4, 6, 16, 255}, Color{40, 34, 52, 255}, k * k);
                world.skyAt(x, y) = 1;
                continue;
            }
            float band = 0.5f + 0.5f * std::sin(y * 0.2f + fbm(x * 0.01f, 0.3f, seed + 6, 2) * 6);
            world.bgAt(x, y) = lerpColor(Color{56, 38, 24, 255}, Color{86, 60, 36, 255}, band * 0.6f);
            world.skyAt(x, y) = 0;
        }
    // far dunes, silhouetted against the sky
    for (int x = 0; x < W; x++)
    {
        int top = G0 - 60 + (int)(std::sin(x * 0.003f + ph) * 40 + (fbm(x * 0.002f, 9.9f, seed, 3) - 0.5f) * 80);
        for (int y = top; y < s[x]; y++) bgPut(x, y, lerpColor(Color{30, 24, 34, 255}, Color{46, 36, 40, 255}, clampf((y - top) / 120.0f, 0, 1)));
    }
    auto sandstoneBlock = [&](int x, int y) { // dressed blocks for the temple
        place(x, y, M::Sandstone);
        int row = y / 6, col = (x + (row % 2) * 7) / 14;
        bool mortar = y % 6 == 0 || (x + (row % 2) * 7) % 14 == 0;
        world.at(x, y).shade = mortar ? (uint8_t)irange(0, 25) : (uint8_t)(90 + hash2(col, row, seed + 12) * 120 + irand(30));
    };
    { // the temple: four stepped tiers, a door on its west face into a chamber, a shrine on top
        for (int k = 0; k < 4; k++)
        {
            int hw = 120 - k * 26, y0 = templeY - 26 * (k + 1), y1 = templeY - 26 * k;
            for (int y = y0; y < y1; y++)
                for (int x = templeX - hw; x <= templeX + hw; x++) sandstoneBlock(x, y);
        }
        clearRect(templeX - 70, templeY - 46, templeX + 70, templeY - 1); // the chamber
        clearRect(templeX - 125, templeY - 26, templeX - 70, templeY - 1); // its door and passage
        addChest(templeX + 20, templeY);
        G.stage = 3; addChest(templeX - 20, templeY); G.stage = 2; // an older offering, richer
        addSconce(templeX - 60, templeY - 30, 1);
        addSconce(templeX + 60, templeY - 30, -1);
        G.lamps.push_back({(float)templeX, (float)(templeY - 26 * 4 - 6), 70, {255, 190, 110, 255}, true}); // a fire on the summit
        for (int k = 0; k < 2; k++) { Mob m = makeEnemy(E_SCORPION, (float)(templeX + irange(-50, 50)), (float)templeY); if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m); }
    }
    { // the oasis: palms leaning over the water
        for (int k = 0; k < 3; k++)
        {
            int px = oasisX + irange(-80, 80), base = s[std::max(0, std::min(W - 1, px))];
            float lean = frange(-0.4f, 0.4f);
            int h = irange(50, 80);
            for (int j = 0; j < h; j++) for (int t = 0; t < 3; t++) bgPut(px + (int)(lean * j * j / h) + t, base - j, shadeC({110, 78, 46, 255}, 0.7f + 0.3f * (j % 4 == 0)));
            int tx = px + (int)(lean * h), ty = base - h;
            for (int f = 0; f < 7; f++)
            {
                float a = -2.8f + f * 0.93f;
                for (int j = 0; j < 26; j++) bgPut(tx + (int)(std::cos(a) * j), ty + (int)(std::sin(a) * j * 0.6f + j * j * 0.02f), shadeC({60, 120, 52, 255}, 0.8f + 0.2f * (j % 3 == 0)));
            }
        }
    }
    { // the raider camp: hide tents, a fire, banners on poles, their stores
        for (int t = 0; t < 3; t++)
        {
            int tx = campX - 100 + t * 90, hw = irange(26, 36), th = irange(30, 40);
            Color cloth = t % 2 ? Color{140, 96, 60, 255} : Color{120, 70, 50, 255};
            for (int y = campY - th; y < campY; y++)
            {
                int half = (int)((y - (campY - th)) * hw / (float)th);
                for (int x = tx - half; x <= tx + half; x++) bgPut(x, y, shadeC(cloth, 0.75f + 0.25f * ((x / 5) % 2) - ((x - tx) > 0 ? 0.15f : 0)));
            }
            for (int y = campY - th - 6; y < campY; y++) bgPut(tx, y, {70, 50, 34, 255}); // the tent pole
        }
        G.lamps.push_back({(float)campX, (float)campY - 3, 90, {255, 150, 70, 255}, true}); // the fire
        for (int x = campX - 6; x <= campX + 6; x++) place(x, campY - 1, M::Gravel);
        for (int b = 0; b < 2; b++) // banners
        {
            int bx = campX + (b ? 125 : -135);
            for (int y = campY - 70; y < campY; y++) bgPut(bx, y, {80, 60, 40, 255});
            for (int y = campY - 68; y < campY - 40; y++) for (int x = bx + 1; x < bx + 14 - (y % 7 == 0); x++) bgPut(x, y, {150, 40, 34, 255});
        }
        placeStores(campX + 40, campY, 50);
        for (int k = 0; k < 5; k++) { Mob m = makeEnemy(E_RAIDER, (float)(campX + irange(-120, 120)), (float)campY); if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m); }
        addChest(campX - 40, campY);
    }
    { // the bones of something enormous, half in the sand
        int cx = irange(2350, 2800), fy = s[cx];
        for (int dx = -90; dx <= 90; dx++)
        {
            int sy = fy - 24 + (int)(std::sin(dx * 0.025f) * 6);
            for (int t = 0; t < 4; t++) place(cx + dx, sy + t, M::Bone);
            if ((dx + 90) % 12 == 0 && std::abs(dx) < 76)
                for (int k = 0; k < 30; k++) place(cx + dx + (int)(std::sin(k * 0.1f) * 8), sy + 3 + k, M::Bone);
        }
        addCoins((float)cx, (float)fy - 30, irange(3, 6), 1);
    }
    // ---- under the dunes: the vaults' dressed stone, then the caves joined up, then what lives and lies in them
    for (auto& v : vaults)
    {
        for (int y = v.y0 - 4; y <= v.y1 + 4; y++)
            for (int x = v.x0 - 4; x <= v.x1 + 4; x++)
                if (world.in(x, y) && isRock(x, y)) sandstoneBlock(x, y);
        for (int y = v.y0; y <= v.y1; y++) // the vault's own back wall, tiled in dark sandstone
            for (int x = v.x0; x <= v.x1; x++) { world.bgAt(x, y) = Color{58, 42, 34, 255}; world.bgAt(x, y).a = WALL_TILE; }
        for (int x = v.x0 + 4; x < v.x1 - 8; x += irange(26, 40)) // pillars: stumps off the floor, and stubs hanging from the roof
        {
            int top = v.y1 - irange(12, 30), len = irange(10, 24);
            for (int dx = 0; dx < 7; dx++)
            {
                for (int y = top; y <= v.y1; y++) sandstoneBlock(x + dx, y);
                if (chance(2)) for (int y = v.y0; y < v.y0 + len; y++) sandstoneBlock(x + 14 + dx, y);
            }
        }
        for (int x = v.x0 + 8; x < v.x1 - 12; x += irange(40, 70)) // a ledge of planks to climb by
        {
            int ly = v.y1 - irange(20, 30);
            for (int dx = 0; dx < 24; dx++) { place(x + dx, ly, M::Platform); place(x + dx, ly + 1, M::Platform); }
        }
    }
    connectPockets(8, s[8] - 6, M::Sandstone, 250, true);
    for (int k = 0; k < (int)vaults.size(); k++)
    {
        const Vault& v = vaults[k];
        G.stage = k ? 4 : 3;
        for (int x = v.x0 + 14; x < v.x1 - 30; x += irange(48, 70)) sarcophagus(x, v.y1 + 1);
        for (int c = 0; c < 3; c++) addChest(irange(v.x0 + 20, v.x1 - 20), v.y1 + 1);
        for (int x = v.x0 + 12; x < v.x1 - 8; x += irange(50, 70)) G.inter.push_back({IT_TORCH, (float)x, (float)(v.y1 + 1)});
        paintSkeleton(irange(v.x0 + 30, v.x1 - 40), v.y1 + 1, chance(2) ? 1 : -1);
        for (int m = 0; m < 5; m++)
        {
            Mob e = makeEnemy(m < 3 ? E_DRAUGR : (m == 3 ? E_SCORPION : E_RAIDER), (float)irange(v.x0 + 20, v.x1 - 20), (float)(v.y1 + 1));
            if (!boxSolid(e.x, e.y, e.w, e.h)) G.mobs.push_back(e);
        }
    }
    auto tierOf = [&](int x, int y) { int d = y - s[x]; return d < 350 ? 2 : (d < 800 ? 3 : 4); };
    { // columns of the cave floors and ceilings: stalactites, stalagmites, drifts of sand, hanging lanterns
        std::vector<int> lanternAt;
        for (int x = 40; x < W - 40; x++)
        {
            int skip = 0;
            for (int y = s[x] + 70; y < H - 60; y++)
            {
                if (skip > 0) { skip--; continue; }
                bool here = world.get(x, y).material != M::Empty && isRock(x, y), below = world.get(x, y + 1).material == M::Empty, above = world.get(x, y - 1).material == M::Empty;
                if (here && below && hash2(x, y, seed + 330) < 0.045f) // a ceiling: a stalactite
                {
                    int L = 5 + (int)(hash2(x, y, seed + 331) * 14), clear = 0;
                    while (clear < L + 30 && world.get(x, y + 1 + clear).material == M::Empty) clear++;
                    if (clear < L + 30) continue;
                    int hw = 2 + (int)(hash2(x, y, seed + 332) * 2.5f);
                    for (int k = 0; k <= L; k++)
                        for (int dx = -hw; dx <= hw; dx++)
                            if (std::abs(dx) <= (int)std::lround(hw * (1 - k / (float)(L + 1))) && world.get(x + dx, y + 1 + k).material == M::Empty) place(x + dx, y + 1 + k, M::Sandstone);
                    skip = 6;
                    continue;
                }
                if (here && above && hash2(x, y, seed + 340) < 0.035f) // a floor: a stalagmite
                {
                    int L = 4 + (int)(hash2(x, y, seed + 341) * 11), clear = 0;
                    while (clear < L + 30 && world.get(x, y - 1 - clear).material == M::Empty) clear++;
                    if (clear < L + 30) continue;
                    int hw = 2 + (int)(hash2(x, y, seed + 342) * 2.5f);
                    for (int k = 0; k <= L; k++)
                        for (int dx = -hw; dx <= hw; dx++)
                            if (std::abs(dx) <= (int)std::lround(hw * (1 - k / (float)(L + 1))) && world.get(x + dx, y - 1 - k).material == M::Empty) place(x + dx, y - 1 - k, M::Sandstone);
                    skip = 8;
                    continue;
                }
                if (here && above && world.get(x - 3, y).material != M::Empty && world.get(x + 3, y).material != M::Empty && world.get(x - 3, y - 1).material == M::Empty
                    && world.get(x + 3, y - 1).material == M::Empty && world.get(x, y - 1).material == M::Empty && hash2(x / 24, y / 9, seed + 350) < 0.55f) // a flat floor: a drift of sand
                {
                    bool solidUnder = true;
                    for (int k = 1; k < 8 && solidUnder; k++) solidUnder = isRock(x, y + k);
                    if (solidUnder) for (int k = 0; k < 2 + (int)(fbm(x * 0.05f, 4.4f, seed, 2) * 3); k++) place(x, y + k, M::Sand);
                }
                if (here && below && x % 240 == 11 && hash2(x, y, seed + 360) < 0.5f) lanternAt.push_back(x * H + y);
            }
        }
        for (int k : lanternAt) { int x = k / H, y = k % H; hangLantern(x, y + 1, irange(3, 9)); }
    }
    { // gold in the walls, glinting, and the light it throws back
        int veins = 0;
        for (int tries = 0; tries < 20000 && veins < 70; tries++)
        {
            int x = irange(60, W - 60), y = irange(s[x] + 110, H - 60);
            if (!isRock(x, y) || (world.get(x - 7, y).material != M::Empty && world.get(x + 7, y).material != M::Empty && world.get(x, y - 7).material != M::Empty && world.get(x, y + 7).material != M::Empty)) continue;
            int r = irange(2, 4);
            for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) if (dx * dx + dy * dy <= r * r + 1 && isRock(x + dx, y + dy) && hash2(x + dx, y + dy, seed + 370) < 0.8f) place(x + dx, y + dy, M::GoldOre);
            G.lamps.push_back({(float)x, (float)y, 34, {255, 196, 96, 255}});
            veins++;
        }
    }
    auto floorSpot = [&](int minDepth, int& fx, int& fy) { // somewhere to stand in the caves, with headroom
        for (int t = 0; t < 400; t++)
        {
            int x = irange(60, W - 60), y = irange(s[x] + minDepth, H - 70);
            if (findFloor(x, y, fy) && fy > s[x] + minDepth && !boxSolid(x - 8, fy - 28, 16, 25) && !isLiquidAt(x, fy - 1)) { fx = x; return true; }
        }
        return false;
    };
    for (int k = 0; k < 44; k++) // scorpions, thicker and meaner the deeper you go; bats in the dark between
    {
        int fx, fy;
        if (!floorSpot(100, fx, fy)) continue;
        G.stage = tierOf(fx, fy);
        Mob m = makeEnemy(E_SCORPION, (float)fx, (float)fy - 2);
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
    }
    for (int k = 0; k < 16; k++)
    {
        int fx, fy;
        if (!floorSpot(120, fx, fy)) continue;
        G.stage = tierOf(fx, fy);
        Mob m = makeEnemy(E_BAT, (float)fx, (float)fy - 30);
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
    }
    for (int k = 0; k < 16; k++) // what was left behind: caches, bones, a few coins
    {
        int fx, fy;
        if (!floorSpot(110, fx, fy)) continue;
        G.stage = tierOf(fx, fy);
        if (k < 11) { addChest(fx, fy); G.inter.push_back({IT_TORCH, (float)fx + 14, (float)fy}); }
        else paintSkeleton(fx, fy, chance(2) ? 1 : -1);
        addCoins((float)fx, (float)fy - 8, irange(1, 3) + G.stage - 2, 1);
    }
    for (int k = 0; k < 3; k++) // a fossil beast, half out of the wall of a great cavern
    {
        int fx, fy;
        for (int t = 0; t < 60; t++)
            if (floorSpot(160, fx, fy) && !boxSolid(fx - 10, fy - 70, 140, 66)) { paintGiantBones(fx, fy); break; }
    }
    G.stage = 2;
    // scorpions in their lairs and out on the sand; raiders on the prowl
    for (auto& l : lairs) { int fy; if (findFloor((int)l.x, (int)l.y - 20, fy)) { Mob m = makeEnemy(E_SCORPION, l.x, (float)fy); if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m); if (chance(2)) addChest((int)l.x + 14, fy); } }
    for (int k = 0; k < 8; k++)
    {
        int x = irange(400, W - 300), type = chance(2) ? E_SCORPION : E_RAIDER;
        if (std::abs(x - campX) < 200) continue;
        G.stage = x > W / 2 ? 3 : 2; // the far end is meaner
        Mob m = makeEnemy(type, (float)x, (float)s[x]);
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
    }
    G.stage = 2;
    for (int k = 0; k < 10; k++) { int x = irange(300, W - 300); addCoins((float)x, (float)s[x] - 6, irange(1, 3), 1); }
    for (int x = 120; x < W - 200; x += irange(50, 130)) // cacti, scrub, and the ones who didn't cross it
    {
        if (std::abs(x - templeX) < 140 || std::abs(x - campX) < 150 || std::abs(x - oasisX) < 80 || std::abs(s[x + 3] - s[x - 3]) > 3) continue;
        int r = irand(10);
        if (r < 6) paintCactus(x, s[x]); else if (r < 9) paintDeadBush(x, s[x]); else paintSkeleton(x, s[x], chance(2) ? 1 : -1);
    }
    shadeBackWall(true);
    G.stage = saved;
    pieceEntryX = 8;
    pieceEntryFloor = s[8];
    upscaleWorld(2);
    return takePiece(false);
}

void startRun()
{
    resetLevelState();
    pieceRects.clear();
    dampOX = dampOY = 0;
    dampSeed = irand(1 << 30);
    loadStep(0.02f, "Laying the Greenmarch...");
    Piece first = buildPiece(0, -1);
    loadStep(0.22f, "Raising Castle Dunmoor...");
    const Haven out = first.havens.back();
    Piece second = buildPiece(1, out.floor);
    loadStep(0.40f, "Joining the road to the castle...");
    Vector2 o = compose(first, {0, 0, (float)first.w.w, (float)first.w.h}, &second, out.x1 + 1, out.floor);
    Piece live = takePiece(true); // and the open sea, west of where you land
    Rectangle ahead = aheadRect;
    loadStep(0.46f, "Filling the Drowned Deep...");
    Piece sea = buildOcean();
    loadStep(0.70f, "Joining sea and shore...");
    Vector2 o2 = compose(live, {0, 0, (float)live.w.w, (float)live.w.h}, &sea, (int)o.x - sea.w.wU() + 4, 300 + (int)o.y);
    aheadRect = {ahead.x + o2.x * world.scale, ahead.y + o2.y * world.scale, ahead.width, ahead.height};
    G.seaEnd = (int)(o.x + o2.x) + 110; // the beach: west of here is open water, not dunes
    { // where sea meets beach the back walls blend: the hills fade out over the water, and the shallows take the sea's colour
        int K = world.scale, x0 = (int)(o.x + o2.x) * K, sea = (300 + (int)(o.y + o2.y)) * K, rx = x0 + 4 * K, xs = std::max(0, x0 - 300 * K);
        for (int y = 0; y < sea; y++)
            for (int x = xs; x < rx; x++)
            {
                float t = (float)(x - xs) / (rx - xs);
                t = t * t * (3 - 2 * t);
                world.bgAt(x, y) = lerpColor(world.bgOf(x, y), world.bgOf(rx, y), t);
                world.skyAt(x, y) = t > 0.5f ? world.skyOf(rx, y) : world.skyOf(x, y);
            }
        for (int x = x0; x < x0 + 140 * K; x++)
            for (int y = sea; y < sea + 40 * K; y++)
                if (props(world.get(x, y).material).kind == Kind::Liquid)
                {
                    world.bgAt(x, y) = lerpColor(Color{22, 52, 70, 255}, Color{14, 34, 48, 255}, (float)(y - sea) / (40 * K));
                    world.skyAt(x, y) = 0;
                }
    }
    { // the Scorched Reach: east of Dunmoor's crag, at ground level
        int K = world.scale;
        Rectangle cr = aheadRect; // the castle, in cells
        int ex = (int)((cr.x + cr.width) / K) - 1, gy = (int)(cr.y / K);
        while (gy < world.hU() - 1 && !isSolid(ex - 6, gy)) gy++;
        Piece live2 = takePiece(true);
        loadStep(0.76f, "Burying the Scorched Reach...");
        Piece sand = buildDesert();
        loadStep(0.92f, "Stitching the desert on...");
        Vector2 o3 = compose(live2, {0, 0, (float)live2.w.w, (float)live2.w.h}, &sand, ex + 1, gy);
        aheadRect = {cr.x + o3.x * K, cr.y + o3.y * K, cr.width, cr.height}; // the next biome still joins at the castle
        Rectangle d = pieceRects.back();
        G.desert = {d.x / K, d.y / K, d.width / K, d.height / K};
        o2.x += o3.x; o2.y += o3.y;
    }
    loadStep(1.0f, "Landing...");
    placePlayer(first.entryX + (int)(o.x + o2.x), first.entryFloor + (int)(o.y + o2.y));
    G.stage = 0;
    G.sanctuary = false;
    G.inVillage = false;
    G.bannerTimer = 240;
    message("You run aground on a moonlit shore. Somewhere ahead, past the dunes, lies the Greenmarch.");
}

void advanceWorld(Haven& sealed)
{
    Haven h = sealed; // the reference won't survive the world being rebuilt
    Piece live = takePiece(true);
    Rectangle keep = {0, 0, (float)live.w.w, (float)live.w.h}; // all of it
    const Haven* far = nullptr; // the haven at the end of the biome ahead: the next biome joins on there
    for (auto& o : live.havens)
        if (o.stage == h.stage + 1) far = &o;
    int nextStage = h.stage + 2;
    Vector2 o;
    if (far && nextStage < STAGE_COUNT)
    {
        int ax = far->x1 + 1, af = far->floor;
        Piece next = buildPiece(nextStage, -1);
        o = compose(live, keep, &next, ax, af);
    }
    else
        o = compose(live, keep, nullptr, 0, 0);
    shiftEntities(o.x, o.y);
}

// Hearthwick: the village between runs. Spend banked coins at the stalls, then sail from the harbour.
void generateVillage()
{
    resetLevelState();
    pieceRects.clear();
    roadValid = false;
    W = 1400; H = 800; // bigger than a fullscreen view, with ground enough below the street that it sits mid-screen at 1440p
    worldInit(W, H);
    seed = irand(1 << 30);
    surf.assign(W, 0);
    const int quay = W - 300, top = 260; // the land ends at the quay; past it is the harbour. `top`: the extra sky
    for (int x = 0; x < W; x++)
    {
        surf[x] = top + 228 + (int)(fbm(x * 0.01f, 2.0f, seed, 2) * 6);
        if (x > quay) surf[x] = top + 232 + (int)(clampf((x - quay) / 16.0f, 0, 1) * 40); // the sea bed shelves away
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || x >= W - 4 || y >= H - 4) place(x, y, M::Bedrock);
            else if (y >= surf[x]) place(x, y, x > quay ? (y < surf[x] + 4 ? M::Sand : M::Stone) : (y < surf[x] + 2 ? M::Grass : (y < surf[x] + 14 ? M::Dirt : M::Stone)));
            else if (x > quay && y >= top + 236) place(x, y, M::Water);
        }
    StageDef bgd = STAGES[0];
    buildBackground(bgd, true);
    // great halls you walk into: a hearth under a cauldron in the high middle, galleries at either end over
    // furnished rooms, banners, an elk skull, oil lanterns swinging on long chains from the ridge
    std::vector<std::pair<int, int>> halls;
    std::vector<int> stalls; // the gaps between the halls are the stalls'
    for (int hx = 50, i = 0; i < 3; i++)
    {
        const int S = 26;
        int w = irange(110, 140), base = surf[hx + w / 2], wallTop = base - 2 * S - irange(6, 14);
        levelGround(hx - 6, hx + w + 6, base);
        Hall hall = paintHall(hx, w, base, wallTop, true);
        halls.push_back({hx - 30, hx + w + 30});
        int g0 = hx + w / 4, g1 = hx + w - w / 4; // the galleries' inner edges
        paintRoom(g0, wallTop + 1, g1, base);
        furnishRoom(hx + 3, g0, base, base - S + 2, false);
        furnishRoom(g1, hx + w - 2, base, base - S + 2, false);
        furnishRoom(hx + 3, g0, base - S, wallTop + 1, false);
        furnishRoom(g1, hx + w - 2, base - S, wallTop + 1, false);
        for (int x : {hx + 3, g0 - 2, g1, hx + w - 5}) paintPost(x, wallTop + 2, base);
        hearthCrane(hall.mid, base);
        for (int sd : {-1, 1}) hutWindow(hall.mid + sd * 46, wallTop + 9, 11, 16, std::min(base - S - (wallTop + 25), 30)); // tall windows in the high middle
        antlerSkull(hall.mid, wallTop + 8);
        for (int sd : {-1, 1}) triskeleBanner(hall.mid + sd * 24, wallTop + 4, 20);
        for (int sd : {-1, 1}) // the galleries: plank floors, open to the middle where you jump up
        {
            int a = sd < 0 ? hx + 3 : g1, b = sd < 0 ? g0 : hx + w - 3;
            for (int x = a; x <= b; x++)
                for (int t = 0; t < 2; t++)
                {
                    bool open = sd < 0 ? x > b - 14 : x < a + 14;
                    place(x, base - S + t, open ? M::Platform : M::Wood);
                    if (!open) world.at(x, base - S + t).shade = (uint8_t)(t ? 30 : (x % 7 == 0 ? 40 : 150 + irand(70)));
                }
            paintLadder(sd < 0 ? g0 - 11 : g1 + 4, base - S + 2, base);
            hangLantern((a + b) / 2, base - S + 2, 3);     // in the room under the gallery
            hangLantern((a + b) / 2, wallTop + 1, 4);      // and the one on it
            hangLantern(hall.mid + sd * 14, hall.apexY + 8, std::max(3, base - S - 14 - (hall.apexY + 8))); // over the hearth
        }
        for (int y = wallTop; y < base; y++) // end walls: doorways below, windows onto the galleries
        {
            if (y >= base - 24 || (y >= base - S - 18 && y < base - S - 7)) continue;
            for (int k = 0; k < 3; k++) { place(hx + k, y, M::Wood); place(hx + w - k, y, M::Wood); }
        }
        Lamp hearth{(float)hall.mid, (float)hall.apexY + 2, 0, BLANK};
        hearth.smoke = true; // the hearth, smoking out through the roof
        G.lamps.push_back(hearth);
        int sd = chance(2) ? -1 : 1;
        paintPorch(sd < 0 ? hx : hx + w, sd, base, true);
        paintYardProp(sd < 0 ? hx + w + 3 : hx - 13, base);
        int gap = irange(70, 84);
        stalls.push_back(hx + w + gap / 2);
        hx += w + gap;
    }
    // the rest of the land: a cottage or two, then the port building by the quay (a chandlery with its stores, and rooms above)
    villageHouses = true;
    const int limit = quay - 300;
    for (int cx = halls.back().second + 40; cx + 100 < limit;)
    {
        int w = irange(84, 104);
        placeHouse(cx, w);
        halls.push_back({cx - 12, cx + w + 12});
        cx += w + irange(22, 40);
        if (cx + 30 < limit) { paintYardProp(cx - 18, surf[cx - 14]); }
    }
    {
        int px = quay - 250, pw = 132;
        placeHouse(px, pw, TH_PORT);
        halls.push_back({px - 14, px + pw + 14});
        paintYardProp(px - 22, surf[px - 18]);
    }
    villageHouses = false;
    // on the open ground: a well at the near end, a runestone by the pier, and fish drying on a rack
    for (int g = 0; g < 2; g++)
    {
        int gx = g == 0 ? 24 : quay - 26, base = surf[gx]; // (the gaps between halls are the stalls')
        if (g == 0) // the well: a ring of fieldstone, a little roof, a bucket on its rope
        {
            for (int y = base - 7; y < base; y++)
                for (int x = gx - 7; x <= gx + 7; x++)
                {
                    bool joint = (y - base) % 3 == 0 || (x + (y / 3) * 3) % 5 == 0;
                    bgPut(x, y, joint ? Color{46, 44, 48, 255} : shadeC({120, 116, 112, 255}, 0.7f + 0.3f * hash2(x / 2, y, seed)));
                }
            for (int y = base - 20; y < base - 7; y++) { bgPut(gx - 6, y, shadeC(HEART, 0.8f)); bgPut(gx + 6, y, shadeC(HEART, 0.6f)); }
            for (int k = 0; k <= 9; k++) { bgPut(gx - 9 + k, base - 21 - k / 2, shadeC(OLDWOOD, 0.9f)); bgPut(gx + 9 - k, base - 21 - k / 2, shadeC(OLDWOOD, 0.7f)); }
            for (int x = gx - 6; x <= gx + 6; x++) bgPut(x, base - 18, shadeC(HEART, 0.5f)); // the windlass
            for (int y = base - 17; y < base - 11; y++) bgPut(gx, y, {170, 140, 90, 255});
            for (int y = base - 11; y < base - 8; y++) for (int x = gx - 1; x <= gx + 1; x++) bgPut(x, y, {92, 64, 40, 255});
        }
        else // a runestone, a serpent carved round its face
        {
            for (int y = base - 18; y < base; y++)
                for (int x = gx - 5; x <= gx + 5; x++)
                {
                    float u = (x - gx) / 5.5f, v = (base - y) / 18.0f;
                    if (u * u + (v > 0.7f ? (v - 0.7f) * (v - 0.7f) * 11 : 0) > 1) continue; // a rounded top
                    bool band = std::fabs((x - gx) - std::sin((base - y) * 0.45f) * 3) < 1.0f;
                    bgPut(x, y, band ? Color{170, 46, 40, 255} : shadeC({128, 126, 120, 255}, 0.7f + 0.3f * hash2(x, y / 2, seed + 3)));
                }
        }
        halls.push_back({gx - 12, gx + 12}); // no tree on top of it
    }
    {
        int rx = quay - 70, base = surf[rx];
        for (int y = base - 20; y < base; y++) { bgPut(rx, y, shadeC(HEART, 0.8f)); bgPut(rx + 30, y, shadeC(HEART, 0.8f)); }
        for (int x = rx; x <= rx + 30; x++) bgPut(x, base - 19, shadeC(HEART, 0.6f));
        for (int x = rx + 3; x < rx + 29; x += 4) // fish, hung by the tail
            for (int y = 0; y < 6; y++)
                for (int k = -1; k <= 1; k++)
                    if (y > 0 || k == 0) bgPut(x + k * (y > 1 && y < 5), base - 18 + y, y == 5 ? Color{90, 96, 104, 255} : shadeC({172, 178, 184, 255}, 0.8f + 0.1f * k));
        halls.push_back({rx - 6, rx + 36});
    }
    for (int x = 40; x < quay - 50; x += irange(60, 110)) // spears with pennons stood along the street
    {
        bool clear = true;
        for (auto& h : halls) clear = clear && (x < h.first - 4 || x > h.second + 4);
        if (clear) addDecor(DK_SPEARPOST, (float)x, (float)surf[x] + 1, irand(32) | (chance(3) ? 32 : 0), irange(24, 32), chance(2));
    }
    for (int x = 30; x < quay - 40; x += irange(16, 34)) // trees in the gaps between the buildings
    {
        bool clear = true;
        for (auto& h : halls) clear = clear && (x < h.first || x > h.second);
        if (clear) placeTree(x, surf[x]);
    }
    // the pier: planks on posts out over the water, the longship moored at its end
    int deck = surf[quay];
    for (int x = quay - 10; x < quay + 170; x++)
    {
        place(x, deck, M::Wood);
        place(x, deck + 1, M::Wood);
        world.at(x, deck).shade = (x % 6 == 0) ? 20 : (uint8_t)irange(150, 220);
        if ((x - quay) % 24 == 0)
            for (int y = deck + 2; y < surf[x]; y++)
                for (int k = 0; k < 3; k++) bgPut(x + k, y, {70, 48, 30, 255});
    }
    G.inter.push_back({IT_BOAT, (float)(quay + 126), (float)(deck + 9)});
    G.inter.push_back({IT_TORCH, (float)(quay - 6), (float)deck});
    G.inter.push_back({IT_TORCH, (float)(quay + 166), (float)deck});

    placePlayer(40, surf[40]);
    for (int i = 0; i < 3; i++)
    {
        float sx = (float)stalls[i], sy = (float)surf[stalls[i]];
        G.inter.push_back({IT_SHOP, sx, sy, false, i});
        G.lamps.push_back({sx - 21, sy - 25, 44, LAMP_WARM}); // the stall's lantern
    }
    for (int x : stalls) G.inter.push_back({IT_TORCH, (float)x + 32, (float)surf[x + 32]});
    G.inter.erase(std::remove_if(G.inter.begin(), G.inter.end(), [](const Interact& it) { return it.type == IT_LANTERN || it.type == IT_PROP; }), G.inter.end()); // a tidy village: nothing hung to swing into or left lying about
    upscaleWorld(2, true); // the village keeps its painted back wall at full detail
    G.roamX0 = 6; // the world runs on past these, so a big screen never shows its edge
    G.roamX1 = (float)(quay + 172);
    G.sanctuary = false;
    G.inVillage = true;
    G.bannerTimer = 0;
    message("Hearthwick. You have " + std::to_string(META.bank) + " coins to spend. Board the longship at the pier when ready.");
}

void generateSandbox()
{
    resetLevelState();
    pieceRects.clear();
    roadValid = false;
    W = 900; H = 360;
    worldInit(W, H);
    seed = irand(1 << 30);
    surf.assign(W, 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || y < 4 || x >= W - 4 || y >= H - 4) place(x, y, M::Bedrock);
            else if (y > 300 + (int)(fbm(x * 0.01f, 0, seed, 3) * 20)) place(x, y, y < 312 ? M::Dirt : M::Stone);
        }
    placePlayer(60, 300);
    int fy;
    if (findFloor(110, 200, fy)) G.inter.push_back({IT_ANVIL, 110, (float)fy});
    StageDef bgd = STAGES[0];
    buildBackground(bgd, false);
    upscaleWorld(2);
    G.sanctuary = false;
    G.inVillage = false;
    message("Sandbox: CTRL+mouse paints, [ ] change material, -/= brush size, E spawns a foe, C drops a chest");
}

// Dev tool: renders every stage to a PNG (no window needed). `sand.exe --dump <dir>`
void dumpStages(const char* dir)
{
    auto GetTime = [] { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
    G.vw = 455; G.vh = 256;
    newGameKit(false);
    { // Hearthwick
        generateVillage();
        Image img = GenImageColor(W, H, BLACK);
        renderWorld((Color*)img.data, 0, 0, W, H);
        ExportImage(img, (std::string(dir) + "/village.png").c_str());
        UnloadImage(img);
    }
    for (int s = 0; s < STAGE_COUNT; s++)
    {
        double t0 = GetTime();
        G.stage = s;
        resetLevelState();
        buildStage(s, s == 1 ? 130 : -1);
        G.p.m.x = pieceEntryX - G.p.m.w / 2.0f;
        G.p.m.y = (float)(pieceEntryFloor - G.p.m.h);
        double t1 = GetTime();
        for (int i = 0; i < 60; i++) simulate(0, 0, W, H);
        double t2 = GetTime();
        Image img = GenImageColor(W, H, BLACK);
        std::vector<Color> px((size_t)W * H);
        renderWorld(px.data(), 0, 0, W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) ImageDrawPixel(&img, x, y, px[(size_t)y * W + x]);
        for (auto& m : G.mobs) ImageDrawRectangle(&img, (int)m.x, (int)m.y, m.w, m.h, m.boss ? MAGENTA : RED);
        for (auto& it : G.inter) ImageDrawRectangle(&img, (int)it.x - 3, (int)it.y - 6, 6, 6, it.type == IT_SHRINE ? VIOLET : YELLOW);
        for (auto& t : G.traps) ImageDrawRectangle(&img, t.type == TR_COLLAPSE ? t.rx0 : t.x - 1, t.type == TR_COLLAPSE ? t.ry1 : t.y - 1, t.type == TR_COLLAPSE ? 24 : 3, 3, ORANGE);
        ImageDrawRectangle(&img, (int)G.p.m.x, (int)G.p.m.y, G.p.m.w, G.p.m.h, GREEN);
        std::string fn = std::string(dir) + "/stage" + std::to_string(s) + ".png";
        ExportImage(img, fn.c_str());
        UnloadImage(img);
        { // connectivity check: open cells unreachable from the start
            std::vector<uint8_t> seen((size_t)W * H, 0);
            std::vector<int> q{(int)(G.p.m.cy()) * W + (int)G.p.m.cx()};
            seen[q[0]] = 1;
            for (size_t h = 0; h < q.size(); h++)
            {
                int k = q[h], x = k % W, y = k / W;
                const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int d2 = 0; d2 < 4; d2++)
                {
                    int nx = x + dx[d2], ny = y + dy[d2];
                    if (!world.in(nx, ny) || isSolid(nx, ny) || seen[(size_t)ny * W + nx]) continue;
                    seen[(size_t)ny * W + nx] = 1;
                    q.push_back(ny * W + nx);
                }
            }
            int lost = 0;
            for (int y = 5; y < H - 5; y++)
                for (int x = 5; x < W - 5; x++)
                    if (!isSolid(x, y) && !seen[(size_t)y * W + x]) lost++;
            std::printf("  unreachable open cells: %d", lost);
            for (auto& h : G.havens) std::printf("  haven %s%s", seen[(size_t)(h.floor - 10) * W + h.x0 + 100] ? "reachable" : "UNREACHABLE", h.locked ? " (locked)" : "");
            std::printf("\n");
        }
        std::printf("stage %d %-22s gen %.2fs  sim60 %.2fs (%.1fms/frame full map)  mobs %zu  traps %zu  objects %zu\n",
                    s, STAGES[s].name, t1 - t0, t2 - t1, (t2 - t1) / 60 * 1000, G.mobs.size(), G.traps.size(), G.inter.size());
    }
    // the continuous world: start a run, then walk into each haven in turn and save the live world after each cut
    G.state = GS_PLAY;
    startRun();
    { // frame cost on the play grid: the update (simulation round the camera included) and drawing the view
        G.camX = G.p.m.cx() + 900; G.camY = G.p.m.cy() + 120; // in among the Greenmarch caves
        G.p.m.x = G.camX + G.vw / 2; G.p.m.y = G.camY + G.vh / 2;
        std::vector<Color> view((size_t)G.vw * 2 * G.vh * 2);
        double t0 = GetTime();
        for (int i = 0; i < 120; i++) updateGame();
        double t1 = GetTime();
        for (int i = 0; i < 120; i++) renderWorld(view.data(), (int)G.camX * 2, (int)G.camY * 2, G.vw * 2, G.vh * 2);
        double t2 = GetTime();
        std::printf("frame cost: update %.2f ms, draw world %.2f ms\n", (t1 - t0) / 120 * 1000, (t2 - t1) / 120 * 1000);
        startRun();
    }
    for (int step = 0;; step++)
    {
        int ww = world.w / 4, wh = world.h / 4; // a quarter of the cells across: half the world's units
        Image img = GenImageColor(ww, wh, BLACK);
        std::vector<Color> strip((size_t)world.w * 4);
        for (int y = 0; y < wh; y++)
        {
            renderWorld(strip.data(), 0, y * 4, world.w, 1);
            for (int x = 0; x < ww; x++)
            {
                Color c = strip[(size_t)x * 4];
                if (!world.chunk(x * 4, y * 4)) dampPreview(x * 4, y * 4, c); // the damp caves, as they'll grow
                ((Color*)img.data)[(size_t)y * ww + x] = c;
            }
        }
        size_t chunks = 0;
        for (auto& c : world.chunks) chunks += c != nullptr;
        std::printf("  world memory: %zu of %zu chunks (%.0f MB)\n", chunks, world.chunks.size(), chunks * sizeof(Chunk) / 1048576.0);
        for (auto& h : G.havens) ImageDrawRectangleLines(&img, {h.x0 / 2.0f, h.top / 2.0f, (h.x1 - h.x0) / 2.0f, (h.floor - h.top) / 2.0f}, 2, h.sealed ? GREEN : (h.locked ? RED : YELLOW));
        ImageDrawRectangle(&img, (int)G.p.m.x / 2 - 2, (int)G.p.m.y / 2 - 2, 8, 14, GREEN);
        ExportImage(img, (std::string(dir) + "/world" + std::to_string(step) + ".png").c_str());
        UnloadImage(img);
        std::printf("world %d: stage %d  %dx%d  mobs %zu  havens %zu  player %.0f,%.0f\n", step, G.stage, ww, wh, G.mobs.size(), G.havens.size(), G.p.m.x, G.p.m.y);
        for (auto& r : pieceRects) std::printf("    piece x %.0f..%.0f  y %.0f..%.0f (units)\n", r.x / 2, (r.x + r.width) / 2, r.y / 2, (r.y + r.height) / 2);
        Haven* h = nullptr;
        for (auto& o : G.havens)
            if (!o.sealed && o.stage == G.stage) h = &o;
        if (!h) break;
        if (h->locked) { h->locked = false; setGate(h->x0, h->x0 + HAVEN_WALL - 1, h->floor - HAVEN_DOOR, h->floor - 1, false); } // dev: as if the guardian fell
        for (int i = 0; i < 3; i++) updateGame();
        G.p.m.x = h->x0 + 60.0f;
        G.p.m.y = (float)(h->floor - G.p.m.h);
        int before = G.stage;
        updateGame();
        if (G.stage == before) { std::printf("  haven didn't seal!\n"); break; }
    }
}
