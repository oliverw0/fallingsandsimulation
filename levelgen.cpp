#include "game.h"
#include "util.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <chrono>
#include <cstring>

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
            for (int yy = ty - 1; yy <= ty + 1; yy++)
                for (int xx = 0; xx < 3; xx++) place(wx + sd * xx, yy, M::Metal);
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

static void addChest(int x, int fy) { G.inter.push_back({IT_CHEST, (float)x, (float)fy}); }

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

// A skeleton sprawled on the floor, painted behind (dir flips which end the skull is).
static void paintSkeleton(int x, int fy, int dir)
{
    const Color bone = {216, 210, 188, 255}, dark = {120, 114, 100, 255};
    auto p = [&](int dx, int dy, Color c) { if (world.in(x + dir * dx, fy - 1 - dy) && world.at(x + dir * dx, fy - 1 - dy).material == M::Empty) bgPut(x + dir * dx, fy - 1 - dy, c); };
    for (int dx = 0; dx < 3; dx++)
        for (int dy = 0; dy < 3; dy++) p(dx, dy, bone); // skull
    p(1, 1, dark);
    p(0, 0, dark);
    for (int dx = 3; dx <= 11; dx++) p(dx, 0, bone); // spine
    for (int dx : {5, 7, 9}) { p(dx, 1, bone); p(dx, 2, bone); } // ribs
    p(6, 3, bone); p(7, 4, bone); p(8, 5, bone);                     // an arm flung up
    p(12, 1, bone); p(13, 1, bone); p(12, 0, bone); p(13, 0, bone);  // pelvis
    for (int dx = 14; dx <= 21; dx++) p(dx, 0, bone); // legs, one knee drawn up
    p(16, 1, bone); p(17, 2, bone); p(18, 1, bone);
}

// Cobweb strung across a ceiling corner: spokes fanning down and towards `dir`, joined by threads.
static void paintCobweb(int x, int y, int dir)
{
    const Color silk = {196, 196, 206, 255};
    int L = irange(7, 12);
    auto put = [&](float r, float a) {
        int px = x + (int)std::lround(dir * std::cos(a) * r), py = y + (int)std::lround(std::sin(a) * r);
        if (world.in(px, py) && world.at(px, py).material == M::Empty) bgPut(px, py, silk);
    };
    for (int s = 0; s <= 4; s++)
        for (int r = 1; r <= L; r++) put((float)r, s * 0.3927f);
    for (int ring = 1; ring <= 3; ring++)
        for (float a = 0; a <= 1.571f; a += 0.06f) put(L * ring / 3.0f - std::sin(std::fmod(a, 0.3927f) / 0.3927f * 3.14159f) * ring * 0.4f, a); // threads sag between spokes
}

static void placeTree(int x, int fy)
{
    int h = irange(26, 50), tw = irange(4, 6);
    Color bark = {78, 54, 34, 255};
    for (int y = fy - h; y < fy + 2; y++)
        for (int xx = -tw / 2; xx <= tw / 2; xx++)
        {
            float k = 0.75f + 0.25f * hash2(x + xx, y / 3, seed + 77) - (xx == -tw / 2 ? 0.2f : 0) + (xx == tw / 2 ? 0.1f : 0);
            bgPut(x + xx, y, shadeC(bark, k));
        }
    for (int b = 0; b < 3; b++) // branches
    {
        int by = fy - h / 2 - b * h / 6, dir = (b % 2) ? 1 : -1;
        for (int k = 0; k < 8; k++) bgPut(x + dir * (tw / 2 + k), by - k / 2, shadeC(bark, 0.8f));
    }
    for (int blob = 0; blob < 5; blob++)
    {
        int cx = x + irange(-10, 10), cy = fy - h + irange(-8, 6), r = irange(7, 12);
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
            {
                if (dx * dx + dy * dy > r * r || hash2(cx + dx, cy + dy, seed) < 0.12f) continue;
                float lit = clampf(0.55f + (-dx - dy) / (2.5f * r) + 0.2f * hash2(cx + dx, cy + dy, seed + 9), 0.4f, 1.1f);
                bgPut(cx + dx, cy + dy, shadeC(Color{70, 130, 50, 255}, lit * 0.85f));
            }
    }
}

static const Color LAMP_WARM = {255, 168, 84, 255};

// An oil lantern on a chain from (x, top): it swings, snaps off, and breaks into burning oil (entities.cpp).
static void hangLantern(int x, int top, int len)
{
    Interact it{IT_LANTERN, (float)x, (float)top};
    it.data = std::max(3, len + 1);
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

static void farCastle(int cx, int base, int kind, const std::vector<int>& nearRidge);
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
                Color sky = lerpColor(Color{4, 6, 16, 255}, Color{30, 38, 68, 255}, k * k); // night, paling to the horizon
                float mh = ls - 10 - fbm(x * 0.004f, 1.7f, seed + 60, 3) * 90;
                float mh2 = ls + 10 - fbm(x * 0.009f, 3.1f, seed + 61, 3) * 60;
                bool open = false;
                if (y > mh2) sky = Color{12, 13, 22, 255};
                else if (y > mh) sky = lerpColor(Color{22, 25, 42, 255}, Color{30, 34, 54, 255}, hash2(x / 3, y / 5, seed) * 0.4f);
                else open = true;
                if (y > surf[x] - 2) { sky = lerpColor(sky, c, (y - surf[x] + 2) / 8.0f); open = false; }
                world.skyAt(x, y) = open;
                c = sky;
            }
            else
                c = Color{(unsigned char)(c.r * 0.62f), (unsigned char)(c.g * 0.62f), (unsigned char)(c.b * 0.62f), 255};
            world.bgAt(x, y) = c;
        }
    if (skies && (d.kind == SK_CASTLE || d.kind == SK_PLAINS))
    {
        std::vector<int> ridge(W);
        for (int x = 0; x < W; x++)
            ridge[x] = std::min(surf[x] - 2, (int)(localSurf[x] + 10 - fbm(x * 0.009f, 3.1f, seed + 61, 3) * 60));
        auto farHill = [&](int x) { return (int)(localSurf[x] - 10 - fbm(x * 0.004f, 1.7f, seed + 60, 3) * 90) + 10; };
        std::vector<std::pair<int, int>> spots;
        if (d.kind == SK_CASTLE) spots = {{200, 0}, {1480, 1}, {1700, 0}};
        else spots = {{W - 760, 0}, {std::min(W - 1000, 1250), 1}};
        for (auto& sp : spots)
            if (sp.first > 60 && sp.first < W - 60) farCastle(sp.first, farHill(sp.first), sp.second, ridge);
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

static Color oldWood(int x, int y, int board) // every board has weathered its own way
{
    float grey = hash2(board, 7, seed), k = 0.76f + 0.24f * hash2(x, y / 5, seed + board);
    return shadeC(lerpColor(OLDWOOD, GREYWOOD, grey * 0.8f), k);
}

// Upright boards with dark seams and the odd knot.
static void paintBoards(int x0, int y0, int x1, int y1, int bw)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            bool seam = (x - x0) % bw == 0, knot = hash2(x / 2, y / 2, seed + 41) > 0.985f;
            bgPut(x, y, seam ? SEAM : (knot ? shadeC(SEAM, 1.7f) : oldWood(x, y, (x - x0) / bw)));
        }
}

// A carved post: rounded by light and shadow, with a zig-zag engraved down its face and a block capital.
static void paintPost(int x, int y0, int y1)
{
    static const char* ZIG[6] = {"#..", ".#.", "..#", "..#", ".#.", "#.."};
    for (int y = y0; y < y1; y++)
        for (int k = 0; k < 3; k++)
        {
            Color c = shadeC(HEART, k == 0 ? 1.1f : (k == 2 ? 0.7f : 0.9f));
            bgPut(x + k, y, ZIG[(y - y0) % 6][k] == '#' ? shadeC(HEART, 0.5f) : c);
        }
    for (int k = -1; k <= 3; k++) { bgPut(x + k, y0, shadeC(HEART, 0.8f)); bgPut(x + k, y0 + 1, shadeC(HEART, 0.6f)); }
}

// A round shield hung on the wall: iron rim, painted halves or quarters, a boss in the middle.
static void paintShield(int cx, int cy, int r)
{
    static const Color paint[3][2] = {{{150, 40, 34, 255}, {214, 200, 170, 255}}, {{40, 70, 130, 255}, {206, 166, 60, 255}}, {{46, 92, 52, 255}, {196, 186, 156, 255}}};
    const Color* p = paint[irand(3)];
    bool quarters = chance(2);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            int d2 = dx * dx + dy * dy;
            if (d2 > r * r) continue;
            Color c = quarters ? ((dx >= 0) == (dy >= 0) ? p[0] : p[1]) : (dx < 0 ? p[0] : p[1]);
            if (d2 > (r - 1) * (r - 1)) c = {74, 70, 66, 255};
            else if (d2 <= 1) c = {160, 160, 166, 255};
            bgPut(cx + dx, cy + dy, shadeC(c, 0.8f + 0.2f * hash2(cx + dx, cy + dy, seed)));
        }
}

struct Hall { int mid, apexY, roofBase; };

// The shared part of every hall: back wall, plinth, posts, shields, and the A-frame roof with its gable,
// ridge spikes and crossed horn-headed boards. `solid` builds the roof as real thatch / shingle cells.
static Hall paintHall(int x, int w, int g, int wallTop, bool solid)
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
        for (int xx = x; xx <= x + w; xx++)
        {
            bool joint = (y - g) % 3 == 0 || (xx + ((y - g + 9) / 3) * 4) % 7 == 0;
            bgPut(xx, y, joint ? Color{46, 44, 48, 255} : shadeC({110, 108, 112, 255}, 0.7f + 0.3f * hash2(xx / 3, y, seed)));
        }
    int bays = std::max(1, w / 26);
    for (int b = 0; b <= bays; b++) paintPost(x + (w - 3) * b / bays, wallTop, g - 5);
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
            for (int k = 1; k <= 2; k++) bgPut(xx, top - k, shadeC(HEART, 0.7f));
        for (int y = top + T; y < wallTop; y++) // the gable: boards fanning out from the ridge
            if (xx >= x && xx <= x + w)
            {
                float a = std::atan2((float)(xx - h.mid), (float)(y - h.apexY) + 0.01f) * 7;
                bool seam = std::fabs(a - std::round(a)) < 0.12f;
                bgPut(xx, y, seam ? SEAM : oldWood(xx, y, (int)std::floor(a) + 20));
            }
    }
    // the gable boards run on past the ridge and cross, ending in carved heads
    static const char* HEAD[5] = {"..##.", ".#..#", "##...", "#.#..", ".#..."};
    int e = 7;
    for (int sd : {-1, 1})
    {
        for (int k = 0; k <= e; k++)
            for (int t = 0; t < 2; t++)
                bgPut(h.mid + sd * (k + t), h.apexY + T - 1 - (int)(k * slope), shadeC(HEART, t ? 0.6f : 0.95f));
        int tx = h.mid + sd * e, ty = h.apexY + T - 2 - (int)(e * slope) - 4;
        for (int j = 0; j < 5; j++)
            for (int i = 0; i < 5; i++)
                if (HEAD[j][i] == '#') bgPut(tx + sd * i, ty + j, shadeC(HEART, 0.85f));
    }
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

// Firewood stacked log-ends out, or a bound hay bale.
static void paintYardProp(int x, int g)
{
    if (chance(2))
        for (int row = 0; row < 3; row++)
            for (int i = 0; i < 3 - row % 2; i++)
            {
                int cx = x + i * 3 + (row % 2) + 1, cy = g - 2 - row * 3;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        bgPut(cx + dx, cy + dy, (dx == 0 && dy == 0) ? Color{176, 132, 84, 255} : Color{92, 62, 38, 255});
            }
    else
        for (int y = g - 7; y < g; y++)
            for (int xx = x; xx < x + 10; xx++)
            {
                bool corner = (y == g - 7 || y == g - 1) && (xx == x || xx == x + 9), band = xx == x + 3 || xx == x + 6;
                if (!corner) bgPut(xx, y, band ? Color{110, 80, 40, 255} : shadeC({204, 170, 96, 255}, 0.75f + 0.25f * hash2(xx, y / 2, seed)));
            }
}

// ---------------------------------------------------------------- indoors: walls, and the clutter of living
static void feastTable(int x0, int x1, int fy);
static void hearthCrane(int cx, int fy);
static void antlerSkull(int cx, int cy);
static void triskeleBanner(int cx, int top, int len);

// Rounded fieldstones bedded in mortar, a row at a time, each stone lit from the top left.
static void paintCobble(int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++)
    {
        int row = (y - y0) / 5, ry = (y - y0) % 5;
        for (int x = x0; x < x1; x++)
        {
            int u = x - x0 + row * 3 + (int)(hash2(row, 3, seed) * 5), stone = u / 6, lx = u % 6;
            if (ry == 4 || lx == 5 || ((lx == 0 || lx == 4) && (ry == 0 || ry == 3))) { bgPut(x, y, {38, 36, 36, 255}); continue; } // mortar, and the stones' rounded corners
            Color c = lerpColor({124, 118, 110, 255}, {106, 90, 76, 255}, hash2(stone, row, seed + 17));
            bgPut(x, y, shadeC(c, 1.08f - 0.09f * ry - 0.04f * lx + 0.08f * hash2(x, y, seed + 5)));
        }
    }
}

// A room's back wall: boards darkening into the corners and up under the ceiling, a cobbled footing, a beam.
static void paintRoom(int x0, int y0, int x1, int y1) // y0 the ceiling, y1 the floor
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
        {
            int board = (y - y0) / 4;
            float edge = std::min(1.0f, std::min({x - x0, x1 - 1 - x, (y - y0) * 2}) / 10.0f); // shadow gathers in the corners
            float k = (0.55f + 0.45f * edge) * (0.82f + 0.18f * hash2(x / 7 + board * 13, board, seed + 8));
            bool seam = (y - y0) % 4 == 3 || (x + board * 11) % 23 == 0;
            bgPut(x, y, seam ? SEAM : shadeC(lerpColor({118, 86, 58, 255}, {100, 84, 70, 255}, hash2(board, x / 23, seed)), k));
        }
    paintCobble(x0, y1 - 6, x1, y1);
    for (int x = x0; x < x1; x++) { bgPut(x, y0, shadeC(HEART, 0.45f)); bgPut(x, y0 + 1, shadeC(HEART, 0.7f)); }
}

static void paintLadder(int x, int y0, int y1)
{
    for (int y = y0; y < y1; y++)
    {
        bgPut(x, y, shadeC(HEART, 0.8f));
        bgPut(x + 6, y, shadeC(HEART, 0.6f));
        if ((y - y0) % 4 == 2) for (int k = 1; k < 6; k++) bgPut(x + k, y, shadeC(HEART, 0.7f));
    }
}

// A little painting in a gilt frame: a longship under sail at sundown, or green hills under a pale sun.
static void paintPicture(int cx, int cy)
{
    int w = irange(5, 7), h = 4, kind = irand(2);
    for (int dy = -h - 1; dy <= h + 1; dy++)
        for (int dx = -w - 1; dx <= w + 1; dx++)
        {
            int x = cx + dx, y = cy + dy;
            if (std::abs(dx) == w + 1 || std::abs(dy) == h + 1) { bgPut(x, y, (dx + dy) % 2 ? Color{204, 162, 72, 255} : Color{140, 104, 44, 255}); continue; }
            float t = (dy + h) / (2.0f * h);
            Color c;
            if (kind == 0)
            {
                c = dy < 1 ? lerpColor({120, 70, 110, 255}, {240, 156, 92, 255}, clampf(t * 1.6f, 0, 1)) : Color{40, 60, 96, 255};
                if (dy == 1 && (dx + cx) % 3 == 0) c = {90, 110, 150, 255};
                if (std::abs(dx - w / 3) <= 1 && dy >= -2 && dy <= 0) c = {226, 216, 190, 255}; // the sail
                if (std::abs(dx - w / 3) <= 2 && dy == 1) c = {70, 44, 30, 255};                 // the hull
            }
            else
            {
                float hill = std::sin(dx * 0.6f + cx) * 1.2f + 1;
                c = dy > hill ? lerpColor({70, 112, 56, 255}, {44, 78, 40, 255}, t) : lerpColor({150, 180, 210, 255}, {214, 222, 226, 255}, t);
                if ((dx + w / 2) * (dx + w / 2) + (dy + 2) * (dy + 2) <= 1) c = {250, 236, 170, 255};
            }
            bgPut(x, y, c);
        }
}

// A tool hung on a wooden peg: an axe, a saw, a sickle, a hammer or a drinking horn on its strap.
static void paintTool(int x, int y, int kind)
{
    const Color haft = {134, 92, 54, 255}, iron = {156, 156, 164, 255}, ironD = {88, 88, 96, 255}, strap = {80, 50, 30, 255};
    bgPut(x, y, shadeC(HEART, 0.45f)); // the peg
    switch (kind)
    {
    case 0: // an axe, head up
        for (int k = 1; k < 14; k++) bgPut(x, y + k, haft);
        for (int dy = 1; dy <= 5; dy++)
        {
            int reach = 2 + (dy > 1 && dy < 5);
            for (int dx = 1; dx <= reach; dx++) bgPut(x + dx, y + dy, dx == reach ? iron : ironD);
        }
        break;
    case 1: // a saw
        for (int dy = 1; dy <= 4; dy++) for (int dx = -1; dx <= 1; dx++) bgPut(x + dx, y + dy, dy == 2 && dx == 0 ? SEAM : haft);
        for (int k = 0; k < 11; k++) { bgPut(x, y + 5 + k, iron); bgPut(x + 1, y + 5 + k, ironD); if (k % 2) bgPut(x + 2, y + 5 + k, ironD); }
        break;
    case 2: // a sickle
        for (int k = 1; k < 6; k++) bgPut(x, y + k, haft);
        for (int a = 0; a < 14; a++) { float t = a / 13.0f * 3.0f; bgPut(x + (int)std::lround(std::sin(t) * 4), y + 6 + (int)std::lround((1 - std::cos(t)) * 3), a > 10 ? ironD : iron); }
        break;
    case 3: // a hammer
        for (int k = 1; k < 11; k++) bgPut(x, y + k, haft);
        for (int dx = -2; dx <= 2; dx++) { bgPut(x + dx, y + 11, iron); bgPut(x + dx, y + 12, ironD); }
        break;
    default: // a drinking horn
        for (int k = 1; k <= 3; k++) { bgPut(x - k, y + k, strap); bgPut(x + k, y + k, strap); }
        for (int k = 0; k < 9; k++)
            for (int t = 0; t <= (k < 6) + (k < 3); t++)
                bgPut(x - 4 + k, y + 4 + (int)(k * k * 0.06f) + t, k < 2 ? Color{220, 200, 150, 255} : lerpColor({206, 176, 124, 255}, {90, 60, 40, 255}, k / 9.0f));
        break;
    }
}

// A dresser: two rows of drawers on stubby feet, with a jug, a crock and a candle on top.
static void paintDresser(int x, int fy)
{
    const Color wood = {112, 74, 44, 255};
    const int w = 16, h = 12;
    for (int y = fy - h; y < fy - 1; y++)
        for (int xx = x; xx < x + w; xx++)
        {
            int ly = y - (fy - h);
            bool top = ly == 0, line = xx == x || xx == x + w - 1 || ly == 5 || ly == 10 || (xx == x + w / 2 && ly > 0);
            Color c = top ? shadeC(wood, 1.3f) : (line ? shadeC(wood, 0.5f) : shadeC(wood, 0.85f + 0.15f * hash2(xx / 3, ly, seed)));
            if ((ly == 3 || ly == 8) && (xx == x + w / 4 || xx == x + 3 * w / 4)) c = {204, 170, 92, 255}; // brass pulls
            bgPut(xx, y, c);
        }
    bgPut(x + 1, fy - 1, shadeC(wood, 0.5f)); bgPut(x + w - 2, fy - 1, shadeC(wood, 0.5f));
    for (int y = fy - h - 4; y < fy - h; y++) for (int k = 0; k < 3; k++) if (y > fy - h - 4 || k == 1) bgPut(x + 2 + k, y, shadeC({150, 98, 58, 255}, 1.1f - 0.15f * k)); // jug
    for (int y = fy - h - 2; y < fy - h; y++) for (int k = 0; k < 4; k++) bgPut(x + 7 + k, y, shadeC({186, 176, 156, 255}, 1.0f - 0.1f * k)); // crock
    for (int y = fy - h - 3; y < fy - h; y++) bgPut(x + 13, y, {232, 222, 194, 255}); // candle
    bgPut(x + 13, fy - h - 4, {255, 214, 120, 255});
    bgPut(x + 13, fy - h - 5, {255, 160, 60, 255});
}

// A plank shelf on brackets: bowls, jars, a wheel of cheese.
static void paintShelf(int x, int y, int w)
{
    for (int xx = x; xx < x + w; xx++) { bgPut(xx, y, shadeC(HEART, 1.0f)); bgPut(xx, y + 1, shadeC(HEART, 0.55f)); }
    for (int k = 0; k < 3; k++) { bgPut(x + 1 + k, y + 2 + k, shadeC(HEART, 0.6f)); bgPut(x + w - 2 - k, y + 2 + k, shadeC(HEART, 0.6f)); }
    for (int xx = x + 1; xx < x + w - 3; xx += irange(4, 6))
    {
        int k = irand(3);
        Color c = k == 0 ? Color{176, 164, 140, 255} : (k == 1 ? Color{96, 120, 90, 255} : Color{226, 196, 100, 255});
        int hh = k == 0 ? 2 : (k == 1 ? 4 : 3);
        for (int dy = 1; dy <= hh; dy++)
            for (int dx = 0; dx < 3; dx++) bgPut(xx + dx, y - dy, shadeC(c, (dx == 0 ? 1.15f : 0.9f) - (k == 2 && dy == 2 ? 0.25f : 0)));
    }
}

// Spears and a shield stood in a rack.
static void paintRack(int x, int fy)
{
    for (int k = 0; k < 3; k++)
    {
        int sx = x + 1 + k * 3;
        for (int y = fy - 20; y < fy; y++) bgPut(sx, y, {124, 86, 50, 255});
        bgPut(sx, fy - 23, {170, 170, 178, 255}); bgPut(sx, fy - 22, {150, 150, 158, 255}); bgPut(sx, fy - 21, {110, 110, 118, 255});
    }
    for (int xx = x - 1; xx <= x + 9; xx++) { bgPut(xx, fy - 12, shadeC(HEART, 0.7f)); bgPut(xx, fy - 2, shadeC(HEART, 0.6f)); }
    paintShield(x + 13, fy - 6, 4);
}

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
static void paintArrows(int x, int fy)
{
    for (int k = 0; k < 9; k++) { bgPut(x + k / 2 + 1, fy - 6 - k, {150, 116, 70, 255}); bgPut(x + 5 - k / 3, fy - 6 - k, {150, 116, 70, 255}); }
    for (int k : {0, 2, 4, 5}) { bgPut(x + k, fy - 15, {226, 226, 220, 255}); bgPut(x + k, fy - 16, {196, 60, 50, 255}); } // fletching
    for (int y = fy - 6; y < fy; y++) for (int xx = x - 1; xx < x + 8; xx++) bgPut(xx, y, (xx + y) % 2 ? Color{150, 120, 70, 255} : Color{120, 92, 52, 255});
}

// Bunks against the wall: a straw mattress and a rolled blanket on each.
static void paintBunk(int x, int fy, int tall)
{
    int top = fy - std::min(22, tall - 2);
    for (int y = top; y < fy; y++) { bgPut(x, y, shadeC(HEART, 0.8f)); bgPut(x + 19, y, shadeC(HEART, 0.6f)); }
    for (int by : {fy - 4, top + 8})
    {
        if (by > fy - 2) continue;
        for (int xx = x; xx <= x + 19; xx++) { bgPut(xx, by, shadeC(HEART, 0.7f)); bgPut(xx, by + 1, shadeC(HEART, 0.45f)); }
        for (int xx = x + 1; xx < x + 19; xx++) for (int y = by - 2; y < by; y++) bgPut(xx, y, shadeC({196, 168, 104, 255}, 0.85f + 0.15f * hash2(xx, y, seed)));
        for (int y = by - 4; y < by - 1; y++) for (int xx = x + 2; xx < x + 6; xx++) bgPut(xx, y, (y + xx) % 3 ? Color{150, 46, 40, 255} : Color{110, 30, 28, 255});
    }
}

// A fireplace of fieldstone with a mantel, its chimney breast going up into the beams.
static void paintFireplace(int cx, int fy, int ceil)
{
    paintCobble(cx - 6, ceil + 2, cx + 7, fy - 14);
    paintCobble(cx - 11, fy - 14, cx + 12, fy);
    for (int x = cx - 12; x <= cx + 12; x++) { bgPut(x, fy - 15, shadeC(HEART, 1.0f)); bgPut(x, fy - 14, shadeC(HEART, 0.5f)); }
    for (int y = fy - 10; y < fy; y++)
        for (int x = cx - 6; x <= cx + 6; x++)
        {
            int ly = y - (fy - 10);
            if (ly == 0 && std::abs(x - cx) > 4) continue; // an arched mouth
            Color c = {22, 16, 14, 255};
            if (ly >= 7) c = (x + y) % 3 ? Color{110, 70, 40, 255} : Color{200, 80, 30, 255}; // logs and embers
            bgPut(x, y, c);
        }
    for (int k = 0; k < 2; k++) for (int y = fy - 18; y < fy - 15; y++) bgPut(cx - 8 + k * 14, y, k ? Color{176, 170, 156, 255} : Color{140, 100, 60, 255}); // things on the mantel
    G.lamps.push_back({(float)cx, (float)(fy - 4), 84, LAMP_WARM, true});
}

// Fill a storey's back wall from x0 to x1: furniture standing on the floor and, between it, things hung up.
static void furnishRoom(int x0, int x1, int fy, int ceil, bool hearth)
{
    paintRoom(x0, ceil, x1, fy);
    int hx = hearth ? (x0 + x1) / 2 + irange(-8, 8) : -1000;
    if (hearth) paintFireplace(hx, fy, ceil);
    int tall = fy - ceil; // room for things hung up high
    for (int x = x0 + irange(3, 8); x < x1 - 12;)
    {
        if (x + 26 > hx - 13 && x < hx + 13) { x = hx + 14; continue; } // keep clear of the fireplace
        int room = std::min(x1 - 3, hearth && x < hx ? hx - 13 : x1 - 3) - x, k = irand(9), used = 0;
        if (k == 0 && room >= 16) { paintDresser(x, fy); used = 16; }
        else if (k == 1 && room >= 26) { feastTable(x + 4, x + 21, fy); used = 26; }
        else if (k == 2 && room >= 14 && tall >= 20) { paintPicture(x + 7, fy - tall / 2 - 4); used = 14; }
        else if (k == 3 && room >= 14 && tall >= 20) { for (int t = 0; t < 2 + (room >= 20); t++) paintTool(x + 2 + t * 6, fy - tall + 5, irand(5)); used = room >= 20 ? 18 : 12; }
        else if (k == 4 && room >= 14) { paintShelf(x, fy - tall / 2 - 3, 14); used = 14; }
        else if (k == 5 && room >= 18) { paintRack(x + 1, fy); used = 18; }
        else if (k == 6 && room >= 13) used = placeStores(x + 1, fy, room - 1) + 1; // loose: knock them about
        else if (k == 7 && room >= 10) { paintArrows(x + 1, fy); used = 10; }
        else if (k == 8 && room >= 22 && tall >= 20) { paintBunk(x, fy, tall); used = 21; }
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
        int fy = g - lv * S, a = lv == levels ? x - ov : x + 4, b = lv == levels ? x + TW - 1 + ov : x + TW - 5, h0 = hatchX(lv);
        for (int xx = a; xx <= b; xx++)
            for (int t = 0; t < 2; t++)
            {
                bool hatch = xx >= h0 && xx < h0 + 11;
                place(xx, fy + t, hatch ? M::Platform : M::Wood);
                if (!hatch) world.at(xx, fy + t).shade = (uint8_t)(t ? 30 : (xx % 6 == 0 ? 40 : 150 + irand(70)));
            }
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
    for (int cx = x - ov + 8; cx < x + TW + ov - 6; cx += 9) paintShield(cx, deck - 6, 4);
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
static void placeHouse(int x, int w)
{
    int g = surf[x + w / 2];
    levelGround(x - 8, x + w + 8, g);
    const int S = 26;
    int storeys = w < 100 ? irange(1, 2) : irange(2, 3), wallTop = g - storeys * S - irange(3, 7);
    Hall hall = paintHall(x, w, g, wallTop, true);
    int first = chance(2); // which side the first stairwell is on
    auto wellX = [&](int k) { return (k + first) % 2 ? x + 8 : x + w - 26; };
    for (int k = 0; k < storeys; k++)
    {
        int fy = g - k * S, ceil = k == storeys - 1 ? wallTop + 1 : fy - S + 2;
        furnishRoom(x + 3, x + w - 2, fy, ceil, k == 0);
        paintPost(x + 3, ceil + 2, fy);
        paintPost(x + w - 5, ceil + 2, fy);
        if (k + 1 < storeys) // the floor above, and its stairwell
        {
            int up = fy - S, h0 = wellX(k + 1);
            paintLadder(h0 + 5, up + 2, fy);
            for (int xx = x + 3; xx <= x + w - 3; xx++)
                for (int t = 0; t < 2; t++)
                {
                    bool well = xx >= h0 && xx < h0 + 18;
                    place(xx, up + t, well ? M::Platform : M::Wood);
                    if (!well) world.at(xx, up + t).shade = (uint8_t)(t ? 30 : (xx % 7 == 0 ? 40 : 150 + irand(70)));
                }
            for (int lx = x + 16 + irand(10); lx < x + w - 14; lx += irange(34, 50)) // lanterns hung from its beams
                if (lx + 3 < h0 || lx - 3 > h0 + 18) hangLantern(lx, up + 2, irange(2, 4));
        }
    }
    hangLantern(hall.mid, hall.apexY + 8, std::max(3, wallTop + 4 - (hall.apexY + 8))); // from the ridge, into the top room
    Lamp hearthSmoke{(float)hall.mid, (float)hall.apexY + 2, 0, BLANK};
    hearthSmoke.smoke = true;
    G.lamps.push_back(hearthSmoke);
    for (int y = wallTop; y < g; y++) // end walls: doorways below, a window on every floor above
    {
        bool door = y >= g - 24, window = false;
        for (int k = 1; k < storeys; k++) window = window || (y >= g - k * S - 18 && y < g - k * S - 7);
        if (door || window) continue;
        for (int k = 0; k < 3; k++) { place(x + k, y, M::Wood); place(x + w - k, y, M::Wood); }
    }
    for (int xx = x; xx <= x + w; xx++) { place(xx, g, M::Wood); place(xx, g + 1, M::Wood); } // floorboards
    bool blockL = chance(2), blockR = chance(2);
    if (blockL) placeObstacle(x + 3, g, 12, 22); // clutter stacked against the doors: smash through
    if (blockR) placeObstacle(x + w - 15, g, 12, 22);
    int sd = chance(2) ? -1 : 1;
    paintPorch(sd < 0 ? x : x + w, sd, g, true);
    paintYardProp(sd < 0 ? x + w + 2 : x - 12, g);
    if (chance(2)) paintCobweb(x + 3, wallTop + 2, 1); // up in the rafters
    G.inter.push_back({IT_TORCH, (float)x + 10, (float)g});
    if (chance(3)) return; // no cellar under this one
    // the cellar, reached by a plank trapdoor
    int cf = g + irange(24, 32);
    for (int y = g + 2; y < cf; y++)
        for (int xx = x + 4; xx <= x + w - 4; xx++) world.at(xx, y) = Cell{};
    for (int y = g + 2; y < cf; y++)
        for (int xx = x + 4; xx <= x + w - 4; xx++)
            bgPut(xx, y, ((y - g) % 6 == 0 || ((xx + ((y - g) / 6) * 5) % 10 == 0)) ? Color{40, 36, 40, 255} : shadeC(Color{110, 104, 100, 255}, 0.45f));
    for (int k = irange(1, 4); k > 0; k--) // barrels
    {
        int bx = x + 6 + irand(std::max(1, w - 18));
        for (int y = cf - 10; y < cf; y++)
            for (int xx = bx; xx < bx + 7; xx++) bgPut(xx, y, (y == cf - 7 || y == cf - 3) ? Color{60, 60, 64, 255} : Color{110, 72, 40, 255});
    }
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

// A window of leaded lattice under a round arch, pale with moonlight.
static void latticeWindow(int cx, int top, int w, int h)
{
    for (int y = top; y < top + h; y++)
        for (int dx = -w / 2; dx <= w / 2; dx++)
        {
            int ay = top + w / 2 - y;
            if (ay > 0 && dx * dx + ay * ay > (w / 2) * (w / 2)) continue;
            int x = cx + dx;
            bool frame = std::abs(dx) >= w / 2 - 1 || y >= top + h - 2;
            bool lead = ((x + y) % 4 == 0) || ((x - y + 400) % 4 == 0);
            Color c = frame ? Color{44, 30, 20, 255} : (lead ? Color{34, 24, 18, 255} : shadeC({150, 160, 186, 255}, 0.75f + 0.25f * (1 - (float)(y - top) / h)));
            bgPut(x, y, c);
        }
}

// A red pillar carved with interlaced dragons, on a dark plinth under a dark capital.
static void dragonPillar(int cx, int y0, int y1, int hw = 4)
{
    for (int y = y0; y <= y1; y++)
        for (int dx = -hw; dx <= hw; dx++)
        {
            int x = cx + dx;
            bool capital = y < y0 + 4 || y > y1 - 5;
            float edge = 1.0f - 0.35f * std::fabs(dx + 1.0f) / hw; // lit from the left
            if (capital) { bgPut(x, y, shadeC({70, 46, 30, 255}, edge * (y == y0 + 3 || y == y1 - 4 ? 0.6f : 1.0f))); continue; }
            float a = std::sin((y - y0) * 0.26f + dx * 0.55f), b = std::sin((y - y0) * 0.26f - dx * 0.55f + 1.6f);
            bool carved = std::fabs(a) < 0.18f || std::fabs(b) < 0.18f; // two strands weaving round each other
            Color c = carved ? Color{70, 14, 14, 255} : Color{156, 38, 30, 255};
            if (std::abs(dx) == hw) c = Color{90, 20, 18, 255};
            bgPut(x, y, shadeC(c, edge));
        }
}

// A post carved into a bearded god's face near its top.
static void idolPillar(int cx, int y0, int y1)
{
    paintPost(cx - 1, y0 + 26, y1);
    for (int y = y0; y < y0 + 26; y++)
        for (int dx = -5; dx <= 5; dx++)
        {
            int x = cx + dx, ly = y - y0;
            Color c = shadeC(OLDWOOD, 1.05f - 0.06f * std::abs(dx + 1));
            if (ly < 3 && std::abs(dx) > 3) continue; // a rounded crown
            bool brow = ly == 6 && std::abs(dx) <= 4, eye = ly >= 7 && ly <= 8 && (dx == -2 || dx == 2), nose = ly >= 8 && ly <= 12 && dx == 0;
            bool mouth = ly == 14 && std::abs(dx) <= 2, beard = ly > 15 && (dx + ly) % 3 == 0;
            if (brow || nose) c = shadeC(HEART, 1.1f);
            if (eye || mouth || beard) c = SEAM;
            bgPut(x, y, c);
        }
}

// A black banner with a white triskele and a fringe.
static void triskeleBanner(int cx, int top, int len)
{
    static const char* TRI[7] = {"..##...", ".#..#..", "....#..", "##.#.##", "#..#..#", ".#...#.", "..###.."};
    for (int y = top; y < top + len; y++)
        for (int dx = -4; dx <= 4; dx++)
        {
            if (y >= top + len - 3 && (dx + 4) % 2) continue; // fringe
            bgPut(cx + dx, y, std::abs(dx) == 4 ? Color{40, 36, 34, 255} : Color{22, 20, 22, 255});
        }
    for (int j = 0; j < 7; j++)
        for (int i = 0; i < 7; i++)
            if (TRI[j][i] == '#') bgPut(cx - 3 + i, top + 6 + j, {214, 210, 200, 255});
    for (int dx = -5; dx <= 5; dx++) bgPut(cx + dx, top - 1, {70, 46, 30, 255}); // its pole
}

// An elk skull with antlers, hung on the wall.
static void antlerSkull(int cx, int cy)
{
    const Color bone = {214, 206, 184, 255};
    for (int y = cy; y < cy + 6; y++)
        for (int dx = -2 + (y - cy) / 3; dx <= 2 - (y - cy) / 3; dx++) bgPut(cx + dx, y, bone);
    bgPut(cx - 1, cy + 1, SEAM); bgPut(cx + 1, cy + 1, SEAM);
    for (int sd : {-1, 1})
    {
        for (int k = 0; k < 9; k++) bgPut(cx + sd * (2 + k), cy - k / 2, bone);
        for (int k = 0; k < 4; k++) { bgPut(cx + sd * 5, cy - 2 - k, bone); bgPut(cx + sd * 9, cy - 4 - k, bone); }
    }
}

// A trestle table laid for a feast, with benches either side.
static void feastTable(int x0, int x1, int fy)
{
    const Color top = {120, 82, 50, 255}, leg = {80, 54, 32, 255};
    for (int x = x0; x <= x1; x++) { bgPut(x, fy - 9, shadeC(top, 1.15f)); bgPut(x, fy - 8, top); bgPut(x, fy - 7, shadeC(top, 0.6f)); }
    for (int lx : {x0 + 3, x1 - 3})
        for (int k = 0; k < 7; k++) { bgPut(lx - 2 + k * 4 / 7, fy - 7 + k, leg); bgPut(lx + 2 - k * 4 / 7, fy - 7 + k, leg); } // X trestles
    for (int x = x0 - 4; x <= x1 + 4; x++) if (x < x0 + 2 || x > x1 - 2) bgPut(x, fy - 4, shadeC(top, 0.8f)); // bench ends showing
    for (int x = x0 + 5; x < x1 - 4; x += irange(5, 9)) // cups, plates and bread
    {
        int k = irand(3);
        if (k == 0) { bgPut(x, fy - 11, {150, 150, 158, 255}); bgPut(x, fy - 10, {110, 110, 118, 255}); bgPut(x + 1, fy - 10, {110, 110, 118, 255}); }
        else if (k == 1) { bgPut(x, fy - 10, {170, 166, 156, 255}); bgPut(x + 1, fy - 10, {170, 166, 156, 255}); bgPut(x + 2, fy - 10, {140, 136, 128, 255}); }
        else { bgPut(x, fy - 10, {176, 128, 70, 255}); bgPut(x + 1, fy - 10, {150, 104, 56, 255}); bgPut(x, fy - 11, {196, 150, 90, 255}); }
    }
}

// A hearth pit with a cauldron hung from a crane of lashed poles over the flames.
static void hearthCrane(int cx, int fy)
{
    const Color pole = {96, 66, 40, 255}, iron = {44, 44, 50, 255};
    for (int x = cx - 9; x <= cx + 9; x++) // the stone ring, and embers
    {
        bgPut(x, fy - 1, (x + cx) % 3 ? Color{100, 96, 92, 255} : Color{70, 66, 64, 255});
        bgPut(x, fy - 2, std::abs(x - cx) > 7 ? Color{100, 96, 92, 255} : Color{150, 60, 24, 255});
    }
    for (int sd : {-1, 1}) // an A-frame of two lashed poles at each end
        for (int k = 0; k < 30; k++)
        {
            bgPut(cx + sd * (11 + k / 6), fy - 1 - k, pole);
            bgPut(cx + sd * (21 - k / 6), fy - 1 - k, pole);
        }
    for (int x = cx - 15; x <= cx + 15; x++) bgPut(x, fy - 30, pole); // crossbar
    for (int y = fy - 29; y < fy - 15; y++) bgPut(cx, y, iron); // chain
    for (int y = fy - 15; y < fy - 8; y++) // the cauldron
        for (int dx = -5; dx <= 5; dx++)
        {
            int ry = y - (fy - 15);
            if (std::abs(dx) > 5 - (ry > 4 ? ry - 4 : 0)) continue;
            bgPut(cx + dx, y, ry == 0 ? Color{70, 70, 76, 255} : shadeC(iron, 1.1f - 0.08f * std::abs(dx + 2)));
        }
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

// ---------------------------------------------------------------- the Norse castle: far off in the night

// A castle on the far hills: drum towers and curtain walls, or a broken chapel by a square tower, with a
// few windows lit by candles. Painted only over sky and the far hills, behind the nearer ridge.
static void farCastle(int cx, int base, int kind, const std::vector<int>& nearRidge)
{
    const Color stone = {46, 48, 64, 255};
    auto put = [&](int x, int y, Color c) {
        if (!world.in(x, y) || x < 0 || x >= (int)nearRidge.size() || y >= nearRidge[x]) return;
        world.bgAt(x, y) = c;
        world.skyAt(x, y) = 0;
    };
    auto block = [&](int x0, int x1, int top, bool round, bool crenel) {
        for (int x = x0; x <= x1; x++)
        {
            float u = (x - x0) / (float)std::max(1, x1 - x0); // drums are lit on the left, shadowed on the right
            float k = round ? 1.15f - 0.4f * u : (x == x1 ? 0.75f : 1.0f);
            int t = top - (crenel && ((x - x0) / 3) % 2 == 0 ? 3 : 0);
            for (int y = t; y < base; y++) put(x, y, shadeC(stone, k * ((y - t) % 4 == 0 ? 0.9f : 1.0f)));
        }
    };
    auto windows = [&](int x0, int x1, int top) {
        for (int y = top + 6; y < base - 6; y += irange(9, 13))
            for (int x = x0 + 2; x < x1 - 2; x += irange(5, 9))
            {
                bool lit = chance(3);
                Color c = lit ? Color{255, 190, 104, 255} : Color{24, 24, 34, 255};
                put(x, y, c); put(x, y + 1, lit ? Color{214, 136, 64, 255} : c);
                if (lit && chance(2)) put(x + 1, y + 1, Color{170, 100, 50, 255});
            }
    };
    if (kind == 0) // drum towers along a curtain wall, a tall keep behind with a flagpole
    {
        block(cx - 22, cx + 12, base - 62, false, true);
        for (int y = base - 82; y < base - 62; y++) put(cx - 4, y, stone);
        windows(cx - 22, cx + 12, base - 62);
        block(cx - 60, cx + 52, base - 34, false, true);
        for (int t : {-58, -36, 18, 40})
        {
            block(cx + t, cx + t + 13, base - 44 - irand(6), true, true);
            windows(cx + t, cx + t + 13, base - 44);
        }
        windows(cx - 60, cx + 52, base - 34);
    }
    else // a ruined chapel: a broken gable with a tall arched window, and its square tower
    {
        for (int x = cx - 24; x <= cx + 4; x++) // the gable
        {
            int top = base - 30 - (int)(16 - std::abs(x - (cx - 10)) * 1.1f);
            if ((x * 7) % 11 == 0) top += 3; // weathered, broken edge
            for (int y = top; y < base; y++)
            {
                bool arch = std::abs(x - (cx - 10)) < 4 && y > base - 34 && y < base - 8;
                put(x, y, arch ? Color{14, 14, 22, 255} : shadeC(stone, x < cx - 10 ? 1.05f : 0.85f));
            }
        }
        block(cx + 5, cx + 21, base - 56, false, true);
        windows(cx + 5, cx + 21, base - 56);
        block(cx - 46, cx - 25, base - 12, false, false); // tumbled walls
        for (int x = cx - 46; x < cx - 25; x += 2) put(x, base - 13 - irand(3), stone);
    }
}

static void decorateRoom(const Room& r, bool grand)
{
    int w = r.x1 - r.x0, h = r.y1 - r.y0;
    // back wall: lattice windows, black banners, and in the deep halls the dead in their stone beds
    bool crypt = r.y0 > castleGround + 260;
    for (int wx = r.x0 + 18 + irand(12); wx < r.x1 - 12; wx += irange(36, 60))
    {
        if (!crypt && (grand || chance(3))) latticeWindow(wx, r.y0 + 6, grand ? 15 : 11, std::min(h - 14, grand ? 44 : 28));
        else if (chance(2)) triskeleBanner(wx, r.y0 + 8, std::min(30, h - 16));
    }
    if (crypt)
        for (int k = irange(1, 2), sx = r.x0 + irange(10, 40); k > 0 && sx < r.x1 - 30; k--, sx += irange(40, 70)) sarcophagus(sx, r.y1 + 1);
    else if (!grand && w > 120 && chance(3)) feastTable(r.x0 + w / 2 - 20, r.x0 + w / 2 + 20, r.y1 + 1);
    for (int tx = r.x0 + 12; tx < r.x1 - 8; tx += irange(50, 80)) G.inter.push_back({IT_TORCH, (float)tx, (float)(r.y1 + 1)});
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
    const int keepX0 = 380, keepX1 = 1220;
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
    for (int x = keepX0; x < keepX1; x += 37) // arrow slits in the curtain wall
        for (int y = surf[x] + 18; y < surf[x] + 34; y++) world.at(x, y).shade = 0;
    buildBackground(d, true);
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
        feastTable(lobby.x0 + 50, lobby.x0 + 120, lobby.y1 + 1);
        feastTable(lobby.x1 - 120, lobby.x1 - 50, lobby.y1 + 1);
        for (int tx = lobby.x0 + 12; tx < lobby.x1 - 8; tx += 60) G.inter.push_back({IT_TORCH, (float)tx, (float)(lobby.y1 + 1)});
    }
    rooms.push_back(lobby);
    G.inter.push_back({IT_TORCH, (float)keepX0 - 10, (float)g});
    G.inter.push_back({IT_TORCH, (float)lobby.x0 + 8, (float)g});
    for (int x = 70; x < keepX0 - 40; x += irange(70, 110)) placeTree(x, g);
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

    int dir = 1;
    for (int tries = 0; tries < 600 && rooms.size() < 16; tries++)
    {
        const Room a = rooms.back();
        int w = irange(150, 300), h = irange(70, 150);
        Room b;
        bool vertical = rooms.size() == 1 || chance(3); // the great hall opens straight down into the cellars
        if (!vertical)
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
        if (vertical)
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
    // down from the last hall to the crypt gate at the castle's foot
    const Room& last = rooms.back();
    int hf = std::min(H - 24, std::max(last.y1 + 1, (int)(H * 0.6f)));
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
    const Color ink = {40, 30, 24, 255}, straw = {200, 176, 116, 255}, red = {176, 46, 42, 255}, cream = {230, 224, 204, 255},
                gold = {224, 182, 62, 255}, wood = {104, 70, 40, 255}, bow = {140, 92, 48, 255};
    for (int k = 0; k < 15; k++) { bgPut(x + 6 + k * 4 / 15, fy - 1 - k, wood); bgPut(x + 16 - k * 4 / 15, fy - 1 - k, wood); } // stand
    for (int dy = -9; dy <= 9; dy++)
        for (int dx = -9; dx <= 9; dx++)
        {
            int d2 = dx * dx + dy * dy;
            if (d2 > 81) continue;
            bgPut(x + 11 + dx, fy - 19 + dy, d2 > 64 ? ink : (d2 > 36 ? straw : (d2 > 16 ? red : (d2 > 4 ? cream : gold))));
        }
    for (int a = 0; a < 2; a++) // arrows stuck in it
    {
        int ax = x + 11 + irange(-5, 3), ay = fy - 19 + irange(-5, 5);
        for (int k = 0; k < 6; k++) bgPut(ax + k, ay - k / 2, wood);
        bgPut(ax + 6, ay - 3, cream);
        bgPut(ax + 6, ay - 2, cream);
    }
    int rx = x - 26;
    for (int y = fy - 22; y < fy; y++) { bgPut(rx, y, wood); bgPut(rx + 12, y, wood); }
    for (int xx = rx; xx <= rx + 12; xx++) bgPut(xx, fy - 21, wood);
    for (int b = 0; b < 2; b++)
    {
        int bx = rx + 3 + b * 5;
        for (int t = -8; t <= 8; t++)
        {
            bgPut(bx + (int)std::lround(2.5f * (1 - (t / 8.0f) * (t / 8.0f))), fy - 12 + t, bow);
            if (std::abs(t) < 8) bgPut(bx, fy - 12 + t, cream); // string
        }
    }
}

// ---------------------------------------------------------------- the Whispering Dunes

static void paintCactus(int x, int fy)
{
    const Color base = {64, 104, 58, 255}, rib = {44, 74, 42, 255}, lit = {96, 140, 80, 255}, spine = {196, 192, 150, 255};
    auto stalk = [&](int x0, int y0, int y1, int w) { // a ribbed column from y0 (top) to y1, rounded at the top
        for (int y = y0; y <= y1; y++)
            for (int k = 0; k < w; k++)
            {
                if (y == y0 && (k == 0 || k == w - 1)) continue;
                Color c = k == w - 1 ? lit : (k == 0 ? rib : ((k + y / 3) % 2 ? base : shadeC(base, 0.85f)));
                if (hash2(x0 + k, y, seed + 41) > 0.93f) c = spine;
                bgPut(x0 + k, y, c);
            }
    };
    int h = irange(16, 30);
    stalk(x - 2, fy - h, fy + 1, 5);
    for (int sd : {-1, 1})
    {
        if (chance(3)) continue;
        int ay = fy - irange(h / 3, h * 2 / 3), reach = irange(3, 5), up = irange(5, 10);
        for (int k = 1; k <= reach; k++)
            for (int yy = ay; yy < ay + 3; yy++) bgPut(x + sd * (2 + k), yy, yy == ay + 2 ? rib : base);
        int ax = sd > 0 ? x + 2 + reach : x - 4 - reach;
        stalk(ax, ay - up, ay + 2, 3);
    }
}

static void paintDeadBush(int x, int fy)
{
    const Color twig = {110, 84, 56, 255};
    for (int b = 0; b < 5; b++)
    {
        float a = -PI / 2 + frange(-1.0f, 1.0f), len = frange(3, 7);
        for (int k = 0; k < len; k++) bgPut(x + (int)std::lround(std::cos(a) * k), fy - 1 + (int)std::lround(std::sin(a) * k), shadeC(twig, 0.8f + 0.1f * (k % 2)));
    }
}

// The bones of a giant, half swallowed by the sand: a skull, a spine and a cage of ribs.
static void paintGiantBones(int x, int fy)
{
    const Color bone = {196, 190, 166, 255}, dark = {130, 124, 108, 255};
    auto put = [&](int px, int py, Color c) { if (world.in(px, py) && world.at(px, py).material == M::Empty) bgPut(px, py, c); };
    for (int k = 0; k < 90; k++) // spine, sagging into the dune
        for (int t = 0; t < 3; t++) put(x + k, fy - 3 + (int)(std::sin(k * 0.035f) * 6) + t, k % 5 == 0 ? dark : bone);
    for (int r = 0; r < 7; r++) // ribs arch up and over
    {
        int rx = x + 14 + r * 10, rh = 34 - std::abs(r - 2) * 4, base = fy - 2 + (int)(std::sin((rx - x) * 0.035f) * 6);
        for (float a = 0; a < PI; a += 0.02f)
        {
            int px2 = rx - (int)(std::cos(a) * 8) - (int)(a * 3), py = base - (int)(std::sin(a) * rh);
            put(px2, py, bone);
            put(px2 + 1, py, a > PI / 2 ? dark : bone);
        }
    }
    for (int dy = -12; dy <= 0; dy++) // the skull, eye socket to the sky
        for (int dx = -16; dx <= 0; dx++)
        {
            float u = (dx + 8) / 9.0f, v = (dy + 6) / 7.0f;
            if (u * u + v * v > 1) continue;
            bool socket = (dx + 6) * (dx + 6) + (dy + 7) * (dy + 7) < 6;
            put(x + dx, fy - 2 + dy, socket ? Color{20, 18, 22, 255} : (dy > -3 ? dark : bone));
        }
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
    if (chance(3)) paintShield(x - dir * irange(5, 8), g - 3, 4);
}

// The battlefield before Dunmoor: the dead lie thicker the nearer the walls, the ground drinks their blood,
// and the levy that fell here has risen to hold the field.
static void decorateBattlefield(int x0, int x1)
{
    int nextBody = x0;
    for (int x = x0; x < x1; x++)
    {
        float k = clampf((x - x0) / (float)(x1 - x0) * 1.4f, 0, 1);
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
        if (k > 0.05f && irand(110) == 0) // the risen levy, with the odd skeleton among them
        {
            Mob mb = makeEnemy(chance(4) ? E_SKELETON : E_RISEN, (float)x, (float)gy);
            if (!boxSolid(mb.x, mb.y, mb.w, mb.h)) G.mobs.push_back(mb);
        }
    }
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

// No walls and no gates: the way simply runs on into the next biome, whose rock and back wall bleed into
// this one's (see compose). At the seam stands a runestone, glowing in the stage's colour, an anvil, and a
// horn of mead that heals you whole. Walking past it builds the biome after next. Only a guardian's
// waystone is barred (buildStage), by a portcullis in a stone plug, until the guardian falls.
static void placeHaven(int s, int floor)
{
    const HavenTheme& t = HAVEN_THEMES[std::min(s, 5)];
    const StageDef& d = STAGES[s];
    const int x0 = W - 220, x1 = W - 1;
    floor = std::max(120, std::min(floor, H - 16));
    int top = floor - 76;
    if (d.surface) levelGround(x0 - 120, W - 1, floor);
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
        for (int x = W - 6; x < W; x++) world.at(x, y) = Cell{};
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
                if (frost && chance(3)) place(x, y - 1, M::Snow);
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
            world.bgAt(x, y) = shadeC(c, std::min(1.0f, k));
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
                cave[(size_t)y * W + x] = 1 + (caveN.at(x, y) > 0.64f || caveN2.at(x, y) > 0.77f);
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
static const int DEEP_LEVELS = 5;
static int deepFloor[DEEP_LEVELS];               // each level's nominal floor row
static std::vector<int> levelFloor[DEEP_LEVELS]; // each level's actual floor per column (-1 where it doesn't run)
struct Shaft { int x, top, bottom; };            // mines: plank-capped, with a rope down
static std::vector<Shaft> shafts;
struct SideRoom { int x0, x1, floor; };
static std::vector<SideRoom> sideRooms;
struct Corridor { int x0, x1, floor; };          // crypts: for the tiled back wall
static std::vector<Corridor> corridors;

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
    // tunnels: 1 - |2n - 1| peaks along the creases of the noise, and warping its input bends them into
    // long winding tubes; caverns: the old low-frequency blobs, rarer than before. The crypts' stone is
    // mostly sound, the caves riddled.
    NoiseGrid tube(W, H, 4, [&](float x, float y) {
        float wx = (fbm(x * 0.003f, y * 0.003f, seed + 200, 2) - 0.5f) * 220, wy = (fbm(x * 0.003f, y * 0.003f, seed + 201, 2) - 0.5f) * 220;
        return 1 - std::fabs(2 * fbm((x + wx) * 0.006f, (y + wy) * 0.008f, seed + 210, 3) - 1);
    });
    NoiseGrid cavern(W, H, 4, [&](float x, float y) { return fbm(x * 0.0055f, y * 0.009f, seed, 4); });
    float tt = crypt ? 0.975f : (mines ? 0.95f : 0.93f), ct = crypt ? 0.67f : (mines ? 0.64f : 0.61f);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) air[(size_t)y * W + x] = tube.at(x, y) > tt || cavern.at(x, y) > ct;

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
        int xs = i == 0 ? 12 : (dir > 0 ? xL : xR), xe = i == N - 1 ? W - 250 : (dir > 0 ? xR : xL);
        std::fill(bottom.begin(), bottom.end(), -1.0f);
        int nextHall = xs + dir * irange(120, 200);
        for (int x = xs; dir > 0 ? x <= xe : x >= xe; x += 2 * dir)
        {
            float cy;
            if (crypt) // a flat corridor, opening now and then into a tall hall
            {
                bool hall = std::abs(x - nextHall) < 40;
                if (dir > 0 ? x > nextHall + 40 : x < nextHall - 40) nextHall += dir * irange(150, 220);
                airRect(x - 1, fy - (hall ? 76 : 38), x + 1, fy - 1);
                cy = (float)(fy - 19);
                for (int dx = -1; dx <= 1; dx++)
                    if (x + dx >= 0 && x + dx < W) bottom[x + dx] = (float)fy - 1;
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
        if (crypt) corridors.push_back({std::min(xs, xe), std::max(xs, xe), fy});
        for (int x = 0; x < W; x++) // a guaranteed floor under the level, bridging any cavern it crosses
        {
            if (bottom[x] < 0) continue;
            int b = (int)bottom[x] + 1;
            levelFloor[i][x] = b;
            for (int y = b; y < b + 5 && y < H; y++) air[(size_t)y * W + x] = 0;
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
        arenaX = W - 460;
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
    for (auto& c : corridors) // dressed stone, with burial niches let into the wall
    {
        Color st = shadeC(d.bgB, 0.8f);
        for (int y = c.floor - 76; y < c.floor; y++)
            for (int x = c.x0; x <= c.x1; x++)
            {
                if (!world.in(x, y) || world.at(x, y).material != M::Empty) continue;
                int row = (c.floor - y) / 9;
                bool mortar = (c.floor - y) % 9 == 0 || (x + row % 2 * 8) % 16 == 0;
                world.bgAt(x, y) = mortar ? shadeC(st, 0.55f) : shadeC(st, 0.8f + 0.2f * hash2((x + row % 2 * 8) / 16, row, seed + 5));
            }
        for (int x = c.x0 + 10; x + 14 < c.x1; x += 26)
        {
            int ny = c.floor - 26;
            if (world.at(x, ny).material != M::Empty || world.at(x + 13, ny + 8).material != M::Empty) continue;
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
    if (d.kind == SK_PLAINS) { W = 1800 + D; H = 1200; } // (its last 600 units are the battlefield before Dunmoor) // tall sky for the gatehouse; caves running down as deep as the castle's foundations
    else if (castle) { W = 1800; H = 1300; }
    else { W = 1100; H = 1400; } // the deep biomes: taller than they're wide - you go down through them
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
            bool cave = plains ? (x > D + 60 && y > surf[x] + 75 && (n > 0.64f || n2 > 0.77f)) : (n > 0.585f || n2 > 0.715f);
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
        enum { B_HOUSE, B_TOWER, B_PALISADE, B_MINE };
        std::vector<int> plan{B_MINE};
        for (int k = irange(2, 3); k > 0; k--) plan.push_back(B_HOUSE);
        for (int k = irange(1, 2); k > 0; k--) plan.push_back(B_TOWER);
        for (int k = irange(1, 2); k > 0; k--) plan.push_back(B_PALISADE);
        for (int i = (int)plan.size() - 1; i > 0; i--) std::swap(plan[i], plan[irand(i + 1)]);
        int bx = D + irange(80, 110), mineAt = -1;
        for (int b : plan)
        {
            int w = b == B_HOUSE ? irange(84, 156) : (b == B_TOWER ? irange(48, 64) : 0);
            int span = b == B_HOUSE ? w + 16 : (b == B_TOWER ? w + 52 : (b == B_PALISADE ? 20 : 52));
            if (bx + span + (mineAt < 0 && b != B_MINE ? 52 + 70 : 0) > W - 1020) continue; // out of road (always leaving room for the mine, after the widest gap, and the gatehouse)
            if (b == B_HOUSE) placeHouse(bx + 8, w);
            else if (b == B_TOWER) placeWatchtower(bx + 26, w);
            else if (b == B_PALISADE) placePalisade(bx + 4);
            else mineAt = bx + 26;
            bx += span + irange(24, 70);
        }
        linkCellars();
        placeMine(mineAt); // dug after the cellars are linked, so their tunnels can't bridge its shaft
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
                else
                    for (int k = 1; k <= irange(2, 4); k++) place(x, y - k, d.top);
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
    if (d.surface) for (int x = D + 30; x < W - 340; x += irange(24, 70)) { int fy; if (std::abs(x - mineX) > 34 && findFloor(x, 4, fy) && std::abs(fy - surf[x]) < 4) placeTree(x, fy); }

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
        int hf = castle ? ends.ey : (d.surface ? surf[W - 200] : pathFloor[W - 250]);
        placeHaven(s, hf);
    }

    for (int i = 0; i < 20; i++) simulate(0, 0, W, H); // let liquids and loose stuff settle (the rest settles in play)
    if (!castle) connectPockets((int)path[3].x, (int)path[3].y, d.base, 300, d.kind == SK_PLAINS); // structures may have sealed a room or two
    if (d.kind == SK_PLAINS) // carved last, so no tunnel cuts into them
        for (int t = 0, n = 0; t < 600 && n < 2; t++) n += placeSealedPocket();

    if (D) decorateDunes(D);
    if (d.kind == SK_PLAINS) decorateBattlefield(W - 960, W - 350);

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
    int sx = castle ? ends.sx : (D ? 116 : 24);
    if (castle) fy = ends.sy;
    else if (!findFloor(sx, D ? 10 : (int)path[6].y, fy)) fy = (int)path[6].y + 8;
    clearRect(sx - 8, fy - 26, sx + 8, fy - 1);
    if (!D && !castle)
    {
        for (int y = fy - 46; y < fy; y++)
            for (int x = 0; x < sx + 10; x++) world.at(x, y) = Cell{};
        for (int y = fy; y < fy + 6; y++)
            for (int x = 0; x < sx + 10; x++) if (!isSolid(x, y)) place(x, y, d.base);
    }
    dressPlatforms();
    shadeBackWall(d.surface);
    pieceEntryX = D ? sx : 8;
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
        if (chestsPlaced.size() >= (d.kind == SK_PLAINS ? 1u : 4u)) break;
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
        int row = fy / 10, col = (fx + (row % 2) * 12) / 24;
        bool mortar = fy % 10 == 0 || (fx + (row % 2) * 12) % 24 == 0;
        c.shade = mortar ? (uint8_t)irange(0, 18) : clamp8((int)(70 + hash2(col, row, seed + 9) * 120) + jitter(16) + (fy % 10 == 1 ? 25 : 0));
    }
    else if (m == M::Platform)
        c.shade = (fx % 14 == 0) ? 15 : clamp8(c.shade + jitter(10));
    else if (m == M::Wood || m == M::Thatch)
        c.shade = clamp8(c.shade + ((fy + fx / 5) % 4 == 0 ? -28 : 0) + jitter(10));
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
                    if (builtMaterial(src->material) != builtMaterial(P.material) || builtMaterial(P.material) || builtMaterial(src->material)) src = &P;
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
    int entryX = 0, entryFloor = 0, duneEnd = 0;
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
        ox = attachX * K;
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
    auto blit = [&](World& src, int sx0, int sy0, int sx1, int sy1, int dx, int dy) {
        for (int cy = sy0 / CS; cy * CS < sy1; cy++)
            for (int cx = sx0 / CS; cx * CS < sx1; cx++)
            {
                auto& from = src.chunks[(size_t)cy * src.cw + cx];
                int x0 = std::max(sx0, cx * CS), y0 = std::max(sy0, cy * CS);
                int x1 = std::min(sx1, cx * CS + CS), y1 = std::min(sy1, cy * CS + CS);
                if (!from && src.fill.material == world.fill.material) continue;
                if (from && x1 - x0 == CS && y1 - y0 == CS && (x0 + dx) % CS == 0 && (y0 + dy) % CS == 0)
                {
                    world.chunks[(size_t)((y0 + dy) / CS) * world.cw + (x0 + dx) / CS] = std::move(from);
                    continue;
                }
                for (int y = y0; y < y1; y++)
                    for (int x = x0; x < x1; x++)
                    {
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
    if (next) blit(next->w, 0, 0, next->w.w, next->w.h, (int)nx, (int)ny);
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
    const int SL = 300, FLOOR = 1300; // sea level is the plains' sea row, so the two line up
    W = 1000; H = 1500;
    worldInit(W, H);
    seed = irand(1 << 30);
    air.assign((size_t)W * H, 0);
    roadValid = false;
    deepLayout = false;
    int saved = G.stage;
    G.stage = 2; // the deep is no place for a beginner
    std::vector<int> bed(W);
    for (int x = 0; x < W; x++)
    {
        float n = fbm(x * 0.012f, 7.7f, seed, 3) - 0.5f, b;
        if (x >= W - 140) b = 324 + (W - 1 - x) * 0.6f; // the shelf off the beach
        else if (x >= W - 300) { float t = (W - 140 - x) / 160.0f; b = 408 + (FLOOR - 408) * t * t * (3 - 2 * t) + n * 90 * t * (1 - t) * 4; } // over the edge
        else if (x < 150) { float t = x / 150.0f; b = SL - 40 + (FLOOR - SL + 40) * t * t * (3 - 2 * t) + n * 40; } // a headland walls it off in the west
        else b = FLOOR + n * 140;
        bed[x] = std::min(H - 30, (int)b);
    }
    for (int k = 0; k < 7; k++) // jagged spires off the abyss floor
    {
        int x = irange(180, W - 340), h = irange(80, 260), r = irange(8, 18);
        for (int dx = -r * 2; dx <= r * 2; dx++)
        {
            if (x + dx < 0 || x + dx >= W) continue;
            float t = 1 - std::fabs((float)dx) / (r * 2);
            bed[x + dx] = std::min(bed[x + dx], bed[x] - (int)(h * t * t));
        }
    }
    // caves: worms burrowing from the sea floor down into the rock, a chamber at the end of each
    std::vector<Vector2> caveEnds;
    for (int k = 0; k < 5; k++)
    {
        float x = (float)irange(170, W - 330), y = (float)bed[(int)x] + 2, ang = frange(0.6f, 2.5f);
        int len = irange(160, 320);
        for (int i = 0; i < len; i++)
        {
            ang += frange(-0.25f, 0.25f);
            ang = clampf(ang, 0.3f, PI - 0.3f); // always downward-ish
            x = clampf(x + std::cos(ang) * 2, 20, (float)W - 20);
            y = clampf(y + std::sin(ang) * 2, 20, (float)H - 40);
            carve(x, y, 7 + (int)(fbm(i * 0.05f, (float)k, seed + 3, 2) * 6));
            if (i % 90 == 89) carve(x, y, irange(16, 24)); // a grotto on the way
        }
        carve(x, y, irange(22, 30));
        caveEnds.push_back({x, y});
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
                if (y >= bed[x] + 4 && hash2(x / 5, y / 5, seed + 33) > 0.985f) place(x, y, M::Glowmoss);
            }
            else if (y >= SL) place(x, y, M::Water);
            else world.at(x, y) = Cell{};
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
    // the back wall: night sky above the waves, darkening blue-green water below
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            float t = fbm(x * 0.05f, y * 0.05f, seed + 51, 2);
            if (y < SL && y < bed[x])
            {
                float k = clampf((float)y / SL, 0, 1);
                world.bgAt(x, y) = lerpColor(Color{4, 6, 16, 255}, Color{30, 38, 68, 255}, k * k);
                world.skyAt(x, y) = 1;
                continue;
            }
            float d = clampf((y - SL) / (float)(FLOOR - SL), 0, 1);
            world.bgAt(x, y) = brighten(lerpColor(Color{22, 52, 70, 255}, Color{4, 10, 18, 255}, std::sqrt(d)), (int)((t - 0.5f) * 14));
            world.skyAt(x, y) = 0;
        }
    for (int x = 170; x < W - 320; x += irange(60, 140)) // far-off towers of the drowned city, ghostly on the back wall
    {
        int w = irange(14, 30), top = irange(FLOOR - 520, FLOOR - 220);
        Color c = {14, 34, 48, 255};
        for (int y = top; y < bed[x]; y++)
            for (int dx = 0; dx < w; dx++)
                if (!(y - top < 10 && dx > w / 3 && dx < w * 2 / 3) && hash2(x + dx, y / 3, seed) > 0.08f) bgPut(x + dx, y, c);
    }
    for (int x = 150; x < W - 20; x += irange(3, 9)) // kelp swaying up off the bottom
    {
        int len = irange(20, std::max(21, std::min(160, bed[x] - SL - 10)));
        for (int k = 0; k < len; k++)
            bgPut(x + (int)(std::sin(k * 0.12f + x) * 3), bed[x] - k, shadeC(Color{40, 110, 60, 255}, 0.6f + 0.4f * (k % 5 == 0)));
    }
    // the sunken city: terraces stepping down into the abyss, each on columns, some bearing a domed temple
    struct Terrace { int x0, x1, y; };
    std::vector<Terrace> terr;
    for (int t = 0, y = FLOOR - 470; t < 7 && y < FLOOR - 40; t++, y += irange(55, 90))
    {
        int w = irange(90, 170), band = (W - 530 - w) / 3, x0 = 190 + (t * 2 % 3) * band + irange(0, band); // zig-zag across the abyss
        terr.push_back({x0, x0 + w, y});
    }
    auto block = [&](int x0, int y0, int x1, int y1, M m) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (world.in(x, y) && world.at(x, y).material != M::Bedrock) place(x, y, m); };
    std::vector<Spot> floors; // where things stand
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
    // temples: walls with a door at the foot and a dome on top. Water can't climb, so the dome keeps its air
    int temples = 0;
    for (auto& tr : terr)
    {
        if (tr.x1 - tr.x0 < 110 || temples >= 3) continue;
        int cx = (tr.x0 + tr.x1) / 2, hw = 30, wallH = 50, door = 26, fl = tr.y;
        for (int y = fl - wallH - hw; y < fl; y++) // clear inside, to air above the doorway's top
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
        block(cx - 12, fl - door + 6, cx + 12, fl - 1, M::Masonry); // a dais just under the water line: stand on it and your head is in the air
        block(cx - 14, fl - wallH - hw + 2, cx + 14, fl - wallH - hw + 3, M::Glowmoss);
        addChest(cx, fl - door + 6);
        addSconce(cx - hw + 5, fl - wallH + 10, 1);
        temples++;
    }
    for (auto& e : caveEnds) { int fy; if (findFloor((int)e.x, (int)e.y - 10, fy)) addChest((int)e.x, fy); }
    // ledges jut from the cliffs on the way down: somewhere to stand and get your bearings
    for (int y = SL + 70; y < FLOOR - 80; y += irange(70, 120))
        for (int side : {0, 1})
        {
            int xc = -1;
            if (side) { for (int x = W - 300; x < W - 140 && xc < 0; x++) if (bed[x] <= y) xc = x; }
            else { for (int x = 150; x > 10 && xc < 0; x--) if (bed[x] <= y) xc = x; }
            if (xc < 0 || chance(4)) continue;
            int len = irange(18, 40), th = irange(5, 9), dir = side ? -1 : 1;
            for (int k = 0; k < len + 6; k++) // rooted a little way into the rock, tapering underneath
                for (int j = 0; j < th + 2; j++)
                    if (j <= th * (1 - (float)k / (len + 6)) + 2) place(xc + dir * (k - 6), y + j, hash2(xc + k, y + j, seed + 8) > 0.9f ? M::Basalt : M::Stone);
            if (chance(3)) addCoins((float)(xc + dir * len / 2), (float)y - 6, irange(1, 3), 1);
            if (chance(4)) place(xc + dir * irange(2, len / 2), y - 1, M::Glowmoss);
        }
    // wrecks: a longship that never made the shore, and one long since sunk into the deep
    auto wreck = [&](int cx) {
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
        float a = frange(-0.6f, 0.6f); // the mast, snapped and leaning
        int len = irange(28, 50);
        for (int k = 0; k < len; k++)
            for (int t = 0; t < 2; t++) place(cx + (int)(std::sin(a) * k) + t, deck + depth - 3 - (int)(std::cos(a) * k), M::Wood);
        addChest(cx + irange(-hw / 3, hw / 3), deck + depth - 2);
        addCoins((float)cx, (float)deck, irange(2, 5), 1);
    };
    wreck(W - 80); // in the shallows, a stone's throw from where you land
    {
        int best = 200; // the deep wreck: the flattest stretch of the abyss floor
        for (int x = 200; x < W - 360; x += 7) if (std::abs(bed[x + 30] - bed[x - 30]) < std::abs(bed[best + 30] - bed[best - 30])) best = x;
        wreck(best);
    }
    {
        int cx = irange(220, W - 380), fy = bed[cx]; // a whale's bones, picked clean
        for (int dx = -70; dx <= 70; dx++)
        {
            int sy = fy - 16 + (int)(std::sin(dx * 0.03f) * 4);
            for (int t = 0; t < 3; t++) place(cx + dx, sy + t, M::Bone); // the spine
            if ((dx + 70) % 9 == 0 && std::abs(dx) < 56) // ribs arching down to the sand
                for (int k = 0; k < 18; k++) place(cx + dx + (int)(std::sin(k * 0.12f) * 6), sy + 2 + k, M::Bone);
        }
        for (int dy = -9; dy <= 9; dy++) // the skull
            for (int dx = 0; dx < 26; dx++)
                if (dx * dx / 676.0f + dy * dy / 81.0f < 1 && !(dx > 14 && dx < 20 && dy < -2)) place(cx + 70 + dx, fy - 14 + dy, M::Bone);
        addCoins((float)cx, (float)fy - 24, irange(2, 4), 1);
    }
    // the dead and the hungry
    for (size_t i = 0; i < floors.size(); i++)
    {
        Mob m = makeEnemy(chance(2) ? E_DRAUGR : E_SKELETON, (float)floors[i].x + irange(-30, 30), (float)floors[i].y);
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
        if (chance(2)) addCoins((float)floors[i].x, (float)floors[i].y - 6, irange(2, 5), 1);
    }
    for (int k = 0; k < 9; k++)
    {
        int x = irange(170, W - 200), y = irange(SL + 80, std::max(SL + 81, bed[x] - 30));
        Mob m = makeEnemy(E_KELPIE, (float)x, (float)y);
        if (!boxSolid(m.x, m.y, m.w, m.h)) G.mobs.push_back(m);
    }
    for (int k = 0; k < 6; k++) { int x = irange(160, W - 320); addCoins((float)x, (float)bed[x] - 10, irange(1, 3), 1); }
    G.stage = saved;
    pieceEntryX = W - 1;
    pieceEntryFloor = SL;
    upscaleWorld(2);
    return takePiece(false);
}

void startRun()
{
    resetLevelState();
    pieceRects.clear();
    dampOX = dampOY = 0;
    dampSeed = irand(1 << 30);
    Piece first = buildPiece(0, -1);
    const Haven out = first.havens.back();
    Piece second = buildPiece(1, out.floor);
    Vector2 o = compose(first, {0, 0, (float)first.w.w, (float)first.w.h}, &second, out.x1 + 1, out.floor);
    Piece live = takePiece(true); // and the open sea, west of where you land
    Rectangle ahead = aheadRect;
    Piece sea = buildOcean();
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
    W = 1200; H = 560; // bigger than a fullscreen view, so you never see past the edge of the world
    worldInit(W, H);
    seed = irand(1 << 30);
    surf.assign(W, 0);
    const int quay = W - 300, top = H - 300; // the land ends at the quay; past it is the harbour. `top`: the extra sky
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
        int w = irange(118, 156), base = surf[hx + w / 2], wallTop = base - 2 * S - irange(6, 14);
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
        int gap = irange(84, 100);
        stalls.push_back(hx + w + gap / 2);
        hx += w + gap;
    }
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
    for (int x = 30; x < quay - 40; x += irange(30, 60)) // trees in the gaps between the halls
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
