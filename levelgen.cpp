#include "game.h"
#include "util.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <chrono>

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
     M::Brick, M::Stone, M::Brick, M::Empty,
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

struct Spot { int x, y; }; // y = first solid row under the feet

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
    int w = irange(50, 70), h = irange(30, 38);
    int x0 = x - w / 2, x1 = x + w / 2, y0 = fy - h, y1 = fy;
    for (int y = y0; y <= y1 + 1; y++)
        for (int xx = x0; xx <= x1; xx++)
        {
            if (!world.in(xx, y) || world.at(xx, y).material == M::Bedrock) continue;
            bool wall = y <= y0 + 1 || y >= y1 || xx <= x0 + 1 || xx >= x1 - 1;
            bool door = (xx <= x0 + 1 || xx >= x1 - 1) && y > y1 - 26 && y < y1;
            if (wall && !door) place(xx, y, M::Brick);
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
    world.bg[(size_t)y * W + x] = shadeC(c, 0.8f);
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

// A lantern on a short chain (painted behind), and the light it gives.
static void hangLantern(int x, int top, int len)
{
    for (int y = top; y < top + len; y++) bgPut(x, y, {60, 60, 64, 255});
    for (int y = top + len; y < top + len + 4; y++)
        for (int xx = x - 1; xx <= x + 1; xx++) bgPut(xx, y, xx == x ? Color{255, 220, 140, 255} : Color{200, 140, 60, 255});
    G.lamps.push_back({(float)x, (float)(top + len + 2), 72, LAMP_WARM});
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

static void buildBackground(const StageDef& d, bool skies)
{
    std::vector<int> localSurf(W, H); // mountains sit behind the nearby ground, not the highest hill in the level
    if (skies)
        for (int x = 0; x < W; x++)
            for (int k = std::max(0, x - 160); k < std::min(W, x + 160); k += 4) localSurf[x] = std::min(localSurf[x], surf[k]);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            float n = fbm(x * 0.02f, y * 0.03f, seed + 50, 3);
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
                world.sky[(size_t)y * W + x] = open;
                c = sky;
            }
            else
                c = Color{(unsigned char)(c.r * 0.62f), (unsigned char)(c.g * 0.62f), (unsigned char)(c.b * 0.62f), 255};
            world.bg[(size_t)y * W + x] = c;
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
    G.sailT = 0;
    G.duneCrossed = false;
    G.texts.clear();
    G.rags.clear();
    G.msgs.clear();
    G.stoneLoot.clear();
    G.bossId = 0;
    G.winTimer = 0;
    G.nearInteract = -1;
    G.p.hook = 0;
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

// A timber watchtower: two posts, one to three platforms with hatches, and a pitched roof.
static void placeWatchtower(int x)
{
    int g = surf[x + 13], levels = irange(1, 3);
    int deck = g - 23 * levels, top = deck - 26; // the lookout deck, and the eaves above it
    levelGround(x - 6, x + 32, g);
    for (int y = top; y < g; y++) // posts: open at the foot (walk in) and around the lookout
    {
        bool open = y >= g - 21 || (y >= top + 4 && y < deck - 2);
        if (open) continue;
        for (int k = 0; k < 3; k++) { place(x + k, y, M::Wood); place(x + 24 + k, y, M::Wood); }
    }
    for (int y = top; y < g; y++) // the full posts, painted behind
        for (int k = 0; k < 3; k++)
            if (world.at(x + k, y).material == M::Empty) { bgPut(x + k, y, {92, 62, 36, 255}); bgPut(x + 24 + k, y, {92, 62, 36, 255}); }
    for (int lv = 0; lv < levels; lv++)
    {
        int py = g - 23 - lv * 23;
        int h0 = lv % 2 ? x + 3 : x + 14; // hatch alternates sides so you zig-zag up
        for (int xx = x; xx <= x + 26; xx++)
        {
            if (xx >= h0 && xx < h0 + 10) continue;
            place(xx, py, M::Platform);
            place(xx, py + 1, M::Platform);
        }
    }
    int roof = irange(8, 13);
    for (int r = 0; r < roof; r++)
        for (int xx = x - 4 + r * 10 / roof; xx <= x + 30 - r * 10 / roof; xx++) place(xx, top - r, M::Wood);
    for (int y = top + 2; y < g; y++) // cross bracing on the back wall
    {
        int t = (y - (top + 2)) % 23;
        bgPut(x + 3 + t, y, {70, 46, 26, 255});
        bgPut(x + 23 - t, y, {70, 46, 26, 255});
    }
    hangLantern(x + 13, top + 1, 6);
    lookouts.push_back({(float)x + 20, (float)deck});
}

// The approach to Castle Dunmoor: a moat with a drawbridge, then two crenellated towers around a gate.
static void placeGatehouse(int gx)
{
    int g = surf[gx];
    levelGround(gx - 70, W - 6, g);
    for (int xx = gx - 46; xx < gx - 6; xx++) // moat
        for (int y = g; y < g + 16; y++)
            place(xx, y, y < g + 4 ? M::Empty : M::Water);
    for (int xx = gx - 48; xx < gx - 4; xx++) { place(xx, g, M::Platform); place(xx, g + 1, M::Platform); } // drawbridge
    for (int y = g + 16; y < g + 19; y++)
        for (int xx = gx - 48; xx < gx - 4; xx++) place(xx, y, M::Stone);
    auto tower = [&](int x0, int x1, int top) {
        for (int y = top; y < g; y++)
            for (int xx = x0; xx <= x1; xx++) place(xx, y, M::Brick);
        for (int xx = x0; xx <= x1; xx++) // merlons
            if (((xx - x0) / 4) % 2 == 0)
                for (int y = top - 6; y < top; y++) place(xx, y, M::Brick);
        for (int y = top + 14; y < top + 22; y++) world.at((x0 + x1) / 2, y).shade = 0; // arrow slit
    };
    tower(gx, gx + 23, g - 92);
    tower(gx + 24, gx + 55, g - 72);
    tower(gx + 56, gx + 79, g - 92);
    for (int y = g - 30; y < g; y++) // the gate passage runs right through the gatehouse
        for (int xx = gx; xx <= gx + 79; xx++)
        {
            world.at(xx, y) = Cell{};
            bool mortar = (y % 4 == 0) || ((xx + ((y / 4) % 2) * 4) % 8 == 0);
            bgPut(xx, y, mortar ? Color{26, 18, 18, 255} : Color{70, 40, 34, 255}); // the vault of the passage
        }
    for (int y = g - 30; y < g - 22; y++) // raised portcullis behind
        for (int xx = gx + 24; xx <= gx + 55; xx++)
            if ((xx - gx) % 4 == 0 || y % 4 == 0) bgPut(xx, y, {54, 56, 64, 255});
    G.inter.push_back({IT_TORCH, (float)gx - 2, (float)g});
    G.inter.push_back({IT_TORCH, (float)gx + 84, (float)g});
    lookouts.push_back({(float)gx + 12, (float)(g - 98)});
    lookouts.push_back({(float)gx + 68, (float)(g - 98)});
}

// A farmhouse you walk straight through: doorways at both ends, maybe an upstairs on planks, maybe a
// cellar under a trapdoor. Size, storeys, roof, plaster and windows are rolled per house.
static void placeHouse(int x, int w)
{
    int g = surf[x + w / 2];
    levelGround(x - 8, x + w + 8, g);
    static const Color plasters[4] = {{196, 180, 150, 255}, {204, 170, 112, 255}, {214, 206, 192, 255}, {176, 150, 132, 255}};
    static const Color beams[3] = {{84, 54, 32, 255}, {60, 40, 28, 255}, {104, 70, 42, 255}};
    const Color plaster = plasters[irand(4)], beam = beams[irand(3)];
    bool upstairs = !chance(3);
    int wallTop = g - (upstairs ? irange(58, 66) : irange(32, 38)), loft = g - 34;
    int bay = irange(14, 22), win = irange(20, 30);
    for (int y = wallTop; y < g; y++) // back wall: plaster and timber frame
        for (int xx = x; xx <= x + w; xx++)
        {
            bool frame = (xx - x) % bay == 0 || y == wallTop || y == g - 18 || (upstairs && (y == loft || y == (loft + wallTop) / 2));
            bgPut(xx, y, frame ? beam : shadeC(plaster, 0.5f + 0.08f * hash2(xx, y, seed)));
        }
    for (int wx = x + irange(10, 16); wx < x + w - 10; wx += win) // warm windows on the back wall
        for (int fl = 0; fl < (upstairs ? 2 : 1); fl++)
        {
            int wy = fl ? loft - 16 : g - 28;
            if (fl && wy < wallTop + 3) continue;
            for (int y = wy; y < wy + 10; y++)
                for (int xx = wx; xx < wx + 8; xx++) bgPut(xx, y, (xx == wx + 4 || y == wy + 5) ? beam : (chance(9) ? Color{60, 50, 40, 255} : Color{255, 196, 110, 255}));
        }
    for (int y = wallTop; y < g; y++) // end walls with doorways (and an upstairs window)
    {
        bool door = y >= g - 24, window = upstairs && y >= loft - 18 && y < loft - 8;
        if (door || window) continue;
        for (int k = 0; k < 3; k++) { place(x + k, y, M::Wood); place(x + w - k, y, M::Wood); }
    }
    for (int xx = x; xx <= x + w; xx++) { place(xx, g, M::Wood); place(xx, g + 1, M::Wood); } // floorboards
    bool blockL = chance(2), blockR = chance(2);
    if (upstairs)
    {
        for (int xx = x + 3; xx <= x + w - 3; xx++) { place(xx, loft, M::Platform); place(xx, loft + 1, M::Platform); }
        int climbX = blockR ? x + w - 28 : x + w - 18;
        for (int y = g - 10; y < g; y++) // a crate to climb up from
            for (int xx = climbX; xx < climbX + 10; xx++) place(xx, y, M::Wood);
    }
    if (blockL) placeObstacle(x + 3, g, 12, 22); // clutter stacked against the doors: smash through
    if (blockR) placeObstacle(x + w - 15, g, 12, 22);
    int roof = irange(10, 20), eave = irange(4, 9); // steep or shallow, wide or tight
    for (int r = 0; r < roof; r++)
    {
        int in = r * (w / 2 + eave) / roof;
        for (int xx = x - eave + in; xx <= x + w + eave - in; xx++) place(xx, wallTop - 1 - r, M::Wood);
    }
    if (chance(2)) // a chimney
    {
        int cx = x + irange(w / 4, w * 3 / 4);
        for (int y = wallTop - roof - 6; y < wallTop - 1; y++)
            for (int xx = cx; xx < cx + 5; xx++) place(xx, y, M::Brick);
    }
    if (upstairs) hangLantern(x + w / 2, wallTop + 1, std::max(3, loft - wallTop - 18));
    if (chance(2)) paintCobweb(x + 3, wallTop, 1); // up in the rafters
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

// Castle Dunmoor waiting on the horizon.
static void paintDistantCastle()
{
    struct Part { int x, w, h; };
    const Part parts[] = {{W - 560, 26, 70}, {W - 534, 70, 42}, {W - 464, 34, 100}, {W - 430, 60, 48}, {W - 370, 24, 84}, {W - 346, 50, 40}};
    int horizon = 999;
    for (int x = W - 600; x < W - 250; x++) horizon = std::min(horizon, surf[x]);
    for (auto& p : parts)
        for (int x = p.x; x < p.x + p.w; x++)
            for (int y = horizon - p.h - 4; y < horizon - 4; y++)
            {
                if (!world.in(x, y) || world.at(x, y).material != M::Empty) continue;
                bool merlon = y < horizon - p.h && ((x - p.x) / 3) % 2;
                if (y < horizon - p.h + 0 && merlon) continue;
                Color c = {36, 38, 58, 255};
                if (hash2(x / 2, y / 3, seed + 8) > 0.96f && y > horizon - p.h + 6) c = {230, 170, 90, 255}; // lit windows
                bgPut(x, y, c);
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

static bool roadValid = false;
int roadFloorAt(int x)
{
    if (!roadValid || x < 0 || x >= (int)pathFloor.size()) return -1;
    return pathFloor[x];
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

static void stainedGlass(int cx, int top, int w, int h)
{
    static const Color glass[5] = {{190, 40, 50, 255}, {50, 80, 200, 255}, {230, 180, 60, 255}, {60, 150, 80, 255}, {140, 60, 170, 255}};
    for (int y = top; y < top + h; y++)
        for (int dx = -w / 2; dx <= w / 2; dx++)
        {
            int ay = top + w / 2 - y; // rounded arch
            if (ay > 0 && dx * dx + ay * ay > (w / 2) * (w / 2)) continue;
            int x = cx + dx;
            if (!world.in(x, y)) continue;
            bool frame = std::abs(dx) == w / 2 || y == top + h - 1;
            bool lead = dx % 3 == 0 || (y - top) % 4 == 0;
            Color c = frame ? Color{40, 36, 48, 255} : (lead ? Color{18, 16, 22, 255} : glass[(int)(hash2((cx + dx) / 3, (y - top) / 4, seed) * 5)]);
            if (!frame && !lead) c = brighten(c, (int)(20 * hash2(x, y, seed + 2)));
            world.bg[(size_t)y * W + x] = c;
        }
}

static void decorateRoom(const Room& r, bool grand)
{
    int w = r.x1 - r.x0, h = r.y1 - r.y0;
    // back wall: gothic stained glass in grand rooms, tall dark windows elsewhere, plus banners
    for (int wx = r.x0 + 18 + irand(12); wx < r.x1 - 12; wx += irange(36, 60))
    {
        if (grand || chance(3)) stainedGlass(wx, r.y0 + 6, grand ? 15 : 11, std::min(h - 14, grand ? 44 : 28));
        else if (chance(2))
            for (int y = r.y0 + 8; y < r.y0 + 8 + std::min(26, h - 16); y++)
                for (int dx = -4; dx <= 4; dx++)
                    world.bg[(size_t)y * W + wx + dx] = std::abs(dx) == 4 ? Color{170, 140, 60, 255} : Color{90, 18, 26, 255};
    }
    for (int tx = r.x0 + 12; tx < r.x1 - 8; tx += irange(50, 80)) G.inter.push_back({IT_TORCH, (float)tx, (float)(r.y1 + 1)});
    if (grand) return;
    // uneven floor: raised steps and plinths
    for (int k = irand(3); k > 0; k--)
    {
        int bw = irange(8, 26), bh = irange(4, 11), bx = irange(r.x0 + 4, std::max(r.x0 + 5, r.x1 - bw - 4));
        for (int y = r.y1 - bh + 1; y <= r.y1; y++)
            for (int x = bx; x < bx + bw; x++) place(x, y, M::Brick);
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

static LevelEnds castleLayout()
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || y < 4 || x >= W - 4 || y >= H - 4) place(x, y, M::Bedrock);
            else place(x, y, M::Basalt); // dark rock between the halls
        }
    std::vector<Room> rooms;
    Room lobby{20, 30, 230, 104}; // the entrance hall
    clearRect(lobby.x0, lobby.y0, lobby.x1, lobby.y1);
    for (int px = lobby.x0 + 40; px < lobby.x1 - 20; px += 50) // great pillars on the back wall
        for (int y = lobby.y0; y <= lobby.y1; y++)
            for (int dx = -4; dx <= 4; dx++) world.bg[(size_t)y * W + px + dx] = shadeC(Color{70, 62, 80, 255}, 0.55f + 0.05f * (4 - std::abs(dx - 1)));
    decorateRoom(lobby, true);
    rooms.push_back(lobby);

    auto overlaps = [&](const Room& b) {
        for (auto& o : rooms)
            if (b.x0 < o.x1 + 10 && b.x1 > o.x0 - 10 && b.y0 < o.y1 + 10 && b.y1 > o.y0 - 10) return true;
        return false;
    };
    auto corridor = [&](int xa, int fa, int xb, int fb) { // a passage whose floor slides from fa to fb
        int s = xa < xb ? 1 : -1, n = std::abs(xb - xa);
        for (int k = 0; k <= n; k++)
        {
            int x = xa + k * s;
            int f = fa + (fb - fa) * k / std::max(1, n);
            for (int y = f - 25; y <= f; y++)
                if (world.in(x, y) && world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
        }
    };

    int dir = 1;
    for (int tries = 0; tries < 400 && rooms.size() < 17; tries++)
    {
        const Room a = rooms.back();
        int w = irange(70, 170), h = irange(40, 80);
        Room b;
        bool vertical = chance(4);
        if (!vertical)
        {
            int gap = irange(12, 40);
            b.x0 = dir > 0 ? a.x1 + gap : a.x0 - gap - w;
            b.x1 = b.x0 + w;
            b.y1 = a.y1 + irange(-8, 46);
            b.y0 = b.y1 - h;
            if (b.x0 < 10 || b.x1 > W - 10) vertical = true;
        }
        if (vertical)
        {
            b.x0 = std::max(10, std::min(W - 10 - w, a.x0 + irange(-w / 2, a.x1 - a.x0 - w / 2)));
            b.x1 = b.x0 + w;
            b.y0 = a.y1 + irange(26, 50);
            b.y1 = b.y0 + h;
        }
        if (b.y0 < 14) continue;
        if (b.y1 > H - 24) break;
        if (overlaps(b)) { if (tries % 6 == 5) dir = -dir; continue; }
        clearRect(b.x0, b.y0, b.x1, b.y1);
        if (vertical)
        {
            // a shaft through a's floor into b, capped with planks you drop through
            int s0 = std::max(a.x0, b.x0) + 4, s1 = std::min(a.x1, b.x1) - 4;
            if (s1 - s0 < 26) { s1 = std::min(a.x1 - 2, s0 + 26); }
            s1 = std::min(s1, s0 + 34);
            for (int y = a.y1 + 1; y < b.y0; y++)
                for (int x = s0; x <= s1; x++)
                    if (world.at(x, y).material != M::Bedrock) world.at(x, y) = Cell{};
            for (int x = s0; x <= s1; x++) { place(x, a.y1 + 1, M::Platform); place(x, a.y1 + 2, M::Platform); }
            for (int ly = a.y1 + 20; ly < b.y0 - 6; ly += 20)
                for (int x = s0; x <= s1; x++) place(x, ly, M::Platform);
            dir = chance(2) ? 1 : -1;
        }
        else
        {
            int xa = dir > 0 ? a.x1 - 2 : a.x0 + 2, xb = dir > 0 ? b.x0 + 2 : b.x1 - 2;
            corridor(xa, a.y1, xb, b.y1);
            if (b.y1 - a.y1 > 30) // a long drop gets a ledge to catch you
                for (int x = std::min(xa, xb); x <= std::max(xa, xb); x++) place(x, (a.y1 + b.y1) / 2, M::Platform);
            if (chance(5)) dir = -dir; // the occasional doubling back
        }
        decorateRoom(b, w > 140 && h > 66);
        rooms.push_back(b);
    }
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
                if (near[(size_t)y * W + x] && world.at(x, y).material == M::Basalt) place(x, y, M::Brick);
    }
    LevelEnds e{};
    e.sx = lobby.x0 + 30;
    e.sy = lobby.y1 + 1;
    const Room& last = rooms.back();
    e.ex = (last.x0 + last.x1) / 2;
    e.ey = last.y1 + 1;
    path.clear();
    pathFloor.assign(W, e.ey);
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
// Mostly walk-upright tunnels (the hero is 21 tall), now and then a crouch-height squeeze.
static int tunnelRadius() { return chance(4) ? irange(8, 9) : irange(11, 13); }

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
            carveRamp((float)cx, (float)cy, best.x, best.y, 7); // a walkable (crouch-height) way in and out
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
    if (chance(2)) addPickupWeapon((float)p.x + side * 6, (float)p.floor - 8, randomWeapon(1));
    paintCobweb(p.x - p.rx / 2, p.floor - pr + 6, 1);
    return true;
}

// A timber headframe over a shaft; a rope down into a string of caverns with loot (and the dead who came for it).
static int mineX = -1; // keeps trees off the headframe
static void placeMine(int mx)
{
    mineX = mx;
    int g = surf[mx];
    levelGround(mx - 24, mx + 24, g);
    int bottom = std::min(H - 140, g + irange(140, 170));
    const Color timber = {112, 76, 44, 255}, iron = {70, 70, 78, 255};
    for (int y = g; y < bottom; y++) // the shaft, timbered on the back wall
        for (int x = mx - 6; x <= mx + 6; x++)
        {
            world.at(x, y) = Cell{};
            if (x == mx - 6 || x == mx + 6 || (y - g) % 20 < 2) bgPut(x, y, shadeC(timber, (y - g) % 20 == 1 ? 0.7f : 1.0f));
        }
    for (int y = g + 22, sd = 1; y < bottom - 16; y += 30, sd = -sd) addSconce(mx + sd * 5, y, sd); // torches down the shaft
    for (int x = mx - 7; x <= mx + 7; x++) { place(x, g, M::Platform); place(x, g + 1, M::Platform); } // plank cover: S drops through
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

    std::vector<Chamber> ch{{mx, bottom, irange(32, 40), irange(32, 38)}};
    for (int side : {-1, 1})
    {
        Chamber prev = ch[0];
        for (int k = irange(2, 3); k > 0; k--)
        {
            Chamber c{prev.x + side * irange(85, 125), 0, irange(28, 44), irange(28, 40)};
            if (c.x < G.duneEnd + 50 || c.x > W - 320) break;
            c.floor = std::max(surf[c.x] + 110, std::min(H - 40, prev.floor + irange(-25, 45)));
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
        if (i == 0) { addSconce(c.x + 18, c.floor - 14, -1); addSconce(c.x - 18, c.floor - 14, 1); continue; } // the landing under the rope stays clear
        int lx = c.x + irange(-c.rx / 3, c.rx / 3);
        if (chance(4)) addChest(lx, c.floor);
        else if (chance(2))
        {
            paintSkeleton(lx - 10, c.floor, 1); // someone didn't make it back up
            addPickupWeapon((float)lx + 8, (float)c.floor - 8, randomWeapon(1));
        }
        if (chance(3)) paintSkeleton(c.x + (lx < c.x ? 1 : -1) * c.rx / 2, c.floor, lx < c.x ? -1 : 1);
        if (chance(2)) addCoins((float)c.x, (float)c.floor - 4, irange(3, 6), 1);
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

void generateStage(int s)
{
    const StageDef& d = STAGES[s];
    resetLevelState();
    // the first stage opens on the Whispering Dunes: a quiet walk up from the beach before the Greenmarch
    const int D = d.kind == SK_PLAINS && s == 0 ? 760 : 0;
    W = 1200 + D; H = 640;
    worldInit(W, H);
    seed = irand(1 << 30);
    air.assign((size_t)W * H, 0);
    path.clear();
    surf.assign(W, 0);
    G.duneEnd = D;
    bool castle = d.kind == SK_CASTLE;
    LevelEnds ends{};
    int arenaX = -1, arenaFloor = 0;
    if (castle)
    {
        buildBackground(d, false); // back wall first; the castle paints windows and banners over it
        ends = castleLayout();
    }
    else
    {
    bool plains = d.kind == SK_PLAINS;
    if (d.surface)
        for (int x = 0; x < W; x++) surf[x] = 120 + (int)(fbm(x * 0.004f, 0.5f, seed + 5, 3) * 40) - 20; // gentle rolling hills
    for (int x = 0; x < D + 80; x++) // long dunes with small ripples, sloping down to the sea at the far left
    {
        float dune = 150 - (fbm(x * 0.006f, 9.1f, seed + 15, 3) - 0.5f) * 80 - std::sin(x * 0.02f + seed) * 4;
        float beach = clampf((130 - x) / 110.0f, 0, 1);
        dune += (214 - dune) * beach * beach * (3 - 2 * beach);
        float t = clampf((x - D) / 80.0f, 0, 1);
        surf[x] = (int)(dune + (surf[x] - dune) * t * t * (3 - 2 * t));
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            // big open caverns from low-frequency noise, smaller pockets from a finer layer
            float n = fbm(x * 0.0075f, y * 0.012f, seed, 4);
            float n2 = fbm(x * 0.022f, y * 0.032f, seed + 99, 3);
            bool cave = plains ? (x > D + 60 && y > surf[x] + 75 && (n > 0.64f || n2 > 0.77f)) : (n > 0.585f || n2 > 0.715f);
            air[(size_t)y * W + x] = cave ? 1 : 0;
            if (d.surface && y < surf[x]) air[(size_t)y * W + x] = 1;
        }

    // The main road: a gently sloped tunnel from entrance to exit with a guaranteed floor,
    // bridging over any caverns it crosses. Side caves branch off it.
    float wx = 12, wy = d.surface ? (float)surf[12] - 14 : 70.0f, vy = 0;
    float startY = wy, endY = H - 90.0f;
    std::vector<int> radii;
    while (wx < W - 12)
    {
        int r = 16 + (int)(fbm(wx * 0.015f, 3.3f, seed + 3, 2) * 12);
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
        if (wy < 40) { wy = 40; vy = std::fabs(vy); }
        if (wy > H - 45) { wy = H - 45.0f; vy = -std::fabs(vy); }
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
    for (int t = 0; t < (plains ? 0 : 26); t++)
    {
        float tx = (float)irange(40, W - 40), ty = (float)irange(40, H - 40);
        float ang = frange(0, 6.2832f);
        int len = irange(120, 360), r = irange(4, 7);
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
        arenaX = W - 220;
        int ay = (int)path[std::min((size_t)(arenaX / 2), path.size() - 1)].y;
        ay = std::max(ay, 110);
        arenaFloor = ay + 30;
        for (int y = ay - 70; y < arenaFloor; y++)
            for (int x = arenaX - 120; x <= arenaX + 120; x++)
                if (world.in(x, y)) air[(size_t)y * W + x] = 1;
    }

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || y < 4 || x >= W - 4 || y >= H - 4) { place(x, y, M::Bedrock); continue; }
            if (air[(size_t)y * W + x]) continue;
            float n = fbm(x * 0.025f, y * 0.035f, seed + 7, 3);
            M m = n > 0.62f ? d.alt : (n < 0.36f ? d.alt2 : d.base);
            if (x < D + 20 - fbm(y * 0.05f, 4.4f, seed, 2) * 40 && y < surf[x] + 12 + (int)(n * 10)) m = M::Sand; // the dunes
            place(x, y, m);
        }
    for (int x = 4; x < 120 && D; x++) // the sea, lapping the beach
        for (int y = 190; y < surf[x]; y++)
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
                if (near[(size_t)y * W + x] && !air[(size_t)y * W + x] && isRock(x, y)) place(x, y, M::Brick);
    }
    if (arenaX >= 0)
        for (int y = arenaFloor; y < arenaFloor + 5; y++)
            for (int x = arenaX - 125; x <= arenaX + 125; x++) place(x, y, M::Metal);
    buildBackground(d, d.surface); // decorations below paint over it
    lookouts.clear();
    fortZones.clear();
    cellars.clear();
    keepZones.clear();
    mineX = -1;
    if (plains) // the road to Dunmoor: farmsteads, watchtowers and palisades, then the moat and gatehouse
    {
        paintDistantCastle();
        enum { B_HOUSE, B_TOWER, B_PALISADE, B_MINE };
        std::vector<int> plan{B_MINE};
        for (int k = irange(2, 4); k > 0; k--) plan.push_back(B_HOUSE);
        for (int k = irange(1, 2); k > 0; k--) plan.push_back(B_TOWER);
        for (int k = irange(1, 2); k > 0; k--) plan.push_back(B_PALISADE);
        for (int i = (int)plan.size() - 1; i > 0; i--) std::swap(plan[i], plan[irand(i + 1)]);
        int bx = D + irange(80, 110), mineAt = -1;
        for (int b : plan)
        {
            int w = b == B_HOUSE ? irange(56, 110) : 0;
            int span = b == B_HOUSE ? w + 16 : (b == B_TOWER ? 40 : (b == B_PALISADE ? 20 : 52));
            if (bx + span + (mineAt < 0 && b != B_MINE ? 76 : 0) > W - 370) continue; // out of road (always leaving room for the mine)
            if (b == B_HOUSE) placeHouse(bx + 8, w);
            else if (b == B_TOWER) placeWatchtower(bx + 6);
            else if (b == B_PALISADE) placePalisade(bx + 4);
            else mineAt = bx + 26;
            bx += span + irange(24, 70);
        }
        placeGatehouse(W - 270);
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
        if (!isRock(x, y) || (d.surface && y < surf[x] + 8)) continue; // no ore in buildings
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
    auto spots = findSpots(400, D + 120, W - 120, 16, 26);
    size_t si = 0;
    if (castle) // no traps or clutter in the entrance hall
        spots.erase(std::remove_if(spots.begin(), spots.end(), [](const Spot& p) { return p.x < 240 && p.y < 110; }), spots.end());
    auto next = [&](Spot& s) { if (si >= spots.size()) return false; s = spots[si++]; return true; };
    Spot sp;
    if (d.kind == SK_CRYPT)
        for (int i = 0, n = 0; n < 6 && next(sp) && i < 60; i++)
            if (std::abs(sp.y - pathFloor[sp.x]) > 40) { placeCryptRoom(sp.x, sp.y); n++; }
    for (int i = 0; i < d.crates && next(sp); i++) placeCrate(sp.x, sp.y);
    for (int i = 0; i < d.kegs && next(sp); i++) placeKegs(sp.x, sp.y);
    for (int i = 0; i < d.vats && next(sp); i++) placeVat(sp.x, sp.y);
    for (int i = 0; i < d.spikes && next(sp); i++) placeSpikePit(sp.x, sp.y);
    for (int i = 0; i < d.arrows && next(sp); i++) placeArrowTrap(sp.x, sp.y);
    for (int i = 0; i < d.flames && next(sp); i++) placeFlameVent(sp.x, sp.y);
    for (int i = 0; i < d.collapses && next(sp); i++)
        if (std::abs(sp.y - pathFloor[sp.x]) > 50) placeCollapse(sp.x, sp.y); // never drop the ceiling on the road
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
    if (d.kind == SK_CRYPT) for (int i = 0; i < 12 && next(sp); i++) paintCircle(sp.x, sp.y - 2, 3, M::Bone, true);
    if (d.surface) for (int x = D + 30; x < W - 320; x += irange(24, 70)) { int fy; if (std::abs(x - mineX) > 34 && findFloor(x, 4, fy) && fy < surf[x] + 4) placeTree(x, fy); }

    // liquids pool on floors, gas pockets float anywhere
    for (auto& l : findSpots(d.liquidCount * 3 / 2, D + 60, W - 60, 6, 10))
    {
        int r = irange(5, 11);
        paintCircle(l.x, l.y - r / 2, r, chance(3) ? d.liquid2 : d.liquid1, true);
    }
    for (int i = 0; i < d.miasma; i++)
    {
        int x = irange(30, W - 30), y = irange(20, H - 20);
        if (world.in(x, y) && world.at(x, y).material == M::Empty) paintCircle(x, y, irange(6, 10), M::Miasma, true);
    }

    for (int i = 0; i < 70; i++) simulate(0, 0, W, H); // let liquids and loose stuff settle
    if (!castle) connectPockets((int)path[3].x, (int)path[3].y, d.base, 300, d.kind == SK_PLAINS); // structures may have sealed a room or two
    if (d.kind == SK_PLAINS) // carved last, so no tunnel cuts into them
        for (int t = 0, n = 0; t < 600 && n < 2; t++) n += placeSealedPocket();

    if (D) decorateDunes(D);

    // torches mark the road; easy to lose among the side caves, but always there
    for (int x = D + 60; !castle && x < W - 60; x += irange(90, 130))
    {
        int y = pathFloor[x];
        if (!isSolid(x, y) || world.at(x, y - 1).material != M::Empty || world.at(x, y - 12).material != M::Empty) continue;
        G.inter.push_back({IT_TORCH, (float)x, (float)y});
    }

    // start and exit
    int fy;
    int sx = castle ? ends.sx : (D ? 116 : 24);
    if (castle) fy = ends.sy;
    else if (!findFloor(sx, D ? 10 : (int)path[6].y, fy)) fy = (int)path[6].y + 8;
    clearRect(sx - 8, fy - 26, sx + 8, fy - 1);
    placePlayer(sx, fy);

    int px = castle ? ends.ex : W - 40;
    int py = castle ? ends.ey - 12 : (int)path.back().y;
    for (int y = py - 20; y <= py + 14 && !castle; y++)
        for (int x = px - 18; x <= px + 18; x++)
            if (world.in(x, y) && world.at(x, y).material != M::Bedrock && (x - px) * (x - px) + (y - py) * (y - py) < 18 * 18)
                world.at(x, y) = Cell{};
    int pfy = py + 12;
    for (int y = pfy; y < pfy + 3; y++)
        for (int x = px - 14; x <= px + 14; x++) place(x, y, M::Brick);
    G.inter.push_back({IT_PORTAL, (float)px, (float)pfy});
    G.portalOpen = d.boss < 0;

    if (d.boss >= 0)
    {
        Mob b = makeEnemy(d.boss, (float)arenaX + 40, (float)arenaFloor);
        b.facing = -1;
        G.bossId = b.id;
        G.mobs.push_back(b);
    }

    // creatures and chests
    spots = findSpots(300, D + 160, W - 80, 16, 30);
    int placed = 0;
    for (size_t i = 0; i < spots.size() && placed < d.enemyCount * 6 / 10; i++)
    {
        Spot e = spots[i];
        if (arenaX >= 0 && std::abs(e.x - arenaX) < 130) continue;
        if (std::abs(e.x - sx) < 70 && std::abs(e.y - fy) < 60) continue; // leave the entrance quiet
        if (castle && e.x < 240 && e.y < 110) continue;                      // the castle's entrance hall is safe
        int type = pickWeighted(d.enemies, enemyKinds);
        int group = type == E_WOLF ? irange(2, 3) : ((type == E_BAT || type == E_GOBLIN || type == E_SKELETON) && chance(3) ? irange(2, 3) : 1);
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
    for (auto& c : findSpots(60, 100, W - 100, 20, 14)) // spread chests out across the level
    {
        bool crowded = false;
        for (auto& o : chestsPlaced) crowded = crowded || std::abs(o.x - c.x) + std::abs(o.y - c.y) < 110;
        if (crowded || (d.kind == SK_PLAINS && c.y < surf[c.x] + 75)) continue; // plains chests lie in the caves, not on the road
        addChest(c.x, c.y);
        chestsPlaced.push_back(c);
        if (chestsPlaced.size() >= (d.kind == SK_PLAINS ? 2u : 5u)) break;
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
            if (inFortZone(x - 26) || inFortZone(x + 20) || !findFloor(x, 10, fy2) || fy2 > surf[x] + 4) continue;
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
        for (auto& st : findSpots(40, 150, W - 150, 24, 32))
            if (castle || std::abs(st.y - pathFloor[st.x]) > 40)
            {
                placeDisplay((float)st.x, (float)st.y, style, themedWeapon(style, s));
                break;
            }
    G.sanctuary = false;
    G.inVillage = false;
    roadValid = !castle;
    G.stage = s;
    G.bannerTimer = 240;
    if (d.boss >= 0) message("A guardian bars the way to the next portal...");
    if (D) message("You run aground on a moonlit shore. Somewhere ahead, past the dunes, lies the Greenmarch.");
}

void generateSanctuary()
{
    resetLevelState();
    roadValid = false;
    W = 520; H = 260;
    worldInit(W, H);
    seed = irand(1 << 30);
    surf.assign(W, 0);
    int floorY = 200;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || y < 4 || x >= W - 4 || y >= H - 4) place(x, y, M::Bedrock);
            else if (x < 20 || x > W - 20 || y < 60) place(x, y, M::Stone);
            else if (y >= floorY) place(x, y, y < floorY + 6 ? M::Brick : M::Stone);
        }
    {
        StageDef bgd = STAGES[std::min(G.stage + 1, STAGE_COUNT - 1)];
        bgd.bgA = Color{30, 30, 40, 255};
        bgd.bgB = Color{52, 48, 66, 255};
        buildBackground(bgd, false);
    }
    for (int px = 104; px < W - 60; px += 90) placePillar(px, floorY);
    for (int bx : {60, 210, 330, 420}) // braziers of lava
    {
        for (int x = bx - 4; x <= bx + 4; x++) place(x, floorY - 1, M::Metal);
        place(bx - 4, floorY - 2, M::Metal);
        place(bx + 4, floorY - 2, M::Metal);
        for (int x = bx - 3; x <= bx + 3; x++) place(x, floorY - 2, M::Lava);
    }
    for (int x = 240; x < 300; x++) // holy pool
        for (int y = floorY; y < floorY + 5; y++) place(x, y, (x == 240 || x == 299 || y == floorY + 4) ? M::Glass : M::Water);

    placePlayer(40, floorY);
    G.inter.push_back({IT_ANVIL, 150, (float)floorY});
    G.inter.push_back({IT_SHRINE, 370, (float)floorY});
    G.inter.push_back({IT_PORTAL, 480, (float)floorY});
    G.portalOpen = true;
    for (int i = 0; i < 3; i++) G.shrineChoice[i] = randomSpell(G.stage + 1);

    G.sanctuary = true;
    G.inVillage = false;
    G.p.m.hp = G.p.m.maxHp;
    message("A sanctuary. Your wounds close. Forge at the anvil, pray at the shrine.");
}

// Hearthwick: the village between runs. Spend banked coins at the stalls, then sail from the harbour.
void generateVillage()
{
    resetLevelState();
    roadValid = false;
    W = 760; H = 300;
    worldInit(W, H);
    seed = irand(1 << 30);
    surf.assign(W, 0);
    const int quay = W - 210; // the land ends here; past it is the harbour
    for (int x = 0; x < W; x++)
    {
        surf[x] = 228 + (int)(fbm(x * 0.01f, 2.0f, seed, 2) * 6);
        if (x > quay) surf[x] = 232 + (int)(clampf((x - quay) / 16.0f, 0, 1) * 40); // the sea bed shelves away
    }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (x < 4 || y < 4 || x >= W - 4 || y >= H - 4) place(x, y, M::Bedrock);
            else if (y >= surf[x]) place(x, y, x > quay ? (y < surf[x] + 4 ? M::Sand : M::Stone) : (y < surf[x] + 2 ? M::Grass : (y < surf[x] + 14 ? M::Dirt : M::Stone)));
            else if (x > quay && y >= 236) place(x, y, M::Water);
        }
    StageDef bgd = STAGES[0];
    buildBackground(bgd, true);
    // longhouses on the back wall: plank walls, a curved turf roof, carved dragon heads on the gables
    for (int hx = 60; hx < quay - 90; hx += irange(120, 160))
    {
        int w = irange(60, 80), h = irange(26, 32), base = surf[hx + w / 2];
        const Color plank = {78, 54, 36, 255}, plankD = {54, 38, 26, 255}, turf = {58, 84, 40, 255}, carve = {120, 84, 50, 255};
        for (int y = base - h; y < base; y++)
            for (int x = hx; x < hx + w; x++)
                bgPut(x, y, (x - hx) % 4 == 0 ? plankD : shadeC(plank, 0.85f + 0.2f * hash2(x, y / 6, seed)));
        for (int y = base - 18; y < base; y++) // door, with a light inside
            for (int x = hx + w / 2 - 5; x < hx + w / 2 + 5; x++)
                bgPut(x, y, (x == hx + w / 2 - 5 || x == hx + w / 2 + 4 || y == base - 18) ? carve : Color{150, 96, 52, 255});
        G.lamps.push_back({(float)(hx + w / 2), (float)(base - 8), 40, LAMP_WARM});
        for (int k = 0; k < 2; k++) // shuttered windows, glowing
        {
            int wx = hx + 9 + k * (w - 24);
            for (int y = base - h + 8; y < base - h + 14; y++)
                for (int x = wx; x < wx + 6; x++) bgPut(x, y, (x == wx || x == wx + 5) ? plankD : Color{255, 186, 96, 255});
            G.lamps.push_back({(float)wx + 3, (float)(base - h + 11), 26, LAMP_WARM});
        }
        int rh = 16; // the roof bows like an upturned hull, thick with turf
        for (int x = hx - 8; x < hx + w + 8; x++)
        {
            float u = (x - hx - w / 2.0f) / (w / 2.0f + 8);
            int top = base - h - (int)(rh * (1 - u * u));
            for (int y = top; y < base - h + 2; y++)
                bgPut(x, y, y < top + 2 ? shadeC(turf, 1.25f) : shadeC(turf, 0.65f + 0.35f * hash2(x, y, seed + 3)));
        }
        for (int sd : {-1, 1}) // crossed gable boards ending in dragon heads
        {
            int gx = sd < 0 ? hx - 4 : hx + w + 3, gy = base - h - 2;
            for (int k = 0; k < 9; k++) bgPut(gx + sd * (k / 2), gy - k, carve);
            bgPut(gx + sd * 5, gy - 9, carve);
            bgPut(gx + sd * 6, gy - 8, carve);
        }
    }
    for (int x = 30; x < quay - 40; x += irange(60, 120)) placeTree(x, surf[x]);
    // the pier: planks on posts out over the water, the longship moored at its end
    int deck = surf[quay];
    for (int x = quay - 10; x < W - 40; x++)
    {
        place(x, deck, M::Wood);
        place(x, deck + 1, M::Wood);
        world.at(x, deck).shade = (x % 6 == 0) ? 20 : (uint8_t)irange(150, 220);
        if ((x - quay) % 24 == 0)
            for (int y = deck + 2; y < surf[x]; y++)
                for (int k = 0; k < 3; k++) bgPut(x + k, y, {70, 48, 30, 255});
    }
    G.inter.push_back({IT_BOAT, (float)(W - 84), (float)(deck + 9)});
    G.inter.push_back({IT_TORCH, (float)(quay - 6), (float)deck});
    G.inter.push_back({IT_TORCH, (float)(W - 44), (float)deck});

    placePlayer(40, surf[40]);
    for (int i = 0; i < 3; i++) G.inter.push_back({IT_SHOP, 160.0f + i * 140, (float)surf[160 + i * 140], false, i});
    for (int x = 100; x < quay - 40; x += 140) G.inter.push_back({IT_TORCH, (float)x, (float)surf[x]});
    G.portalOpen = true;
    G.sanctuary = false;
    G.inVillage = true;
    G.bannerTimer = 0;
    message("Hearthwick. You have " + std::to_string(META.bank) + " coins to spend. Board the longship at the pier when ready.");
}

void generateSandbox()
{
    resetLevelState();
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
    G.sanctuary = false;
    G.inVillage = false;
    G.portalOpen = true;
    message("Sandbox: CTRL+mouse paints, [ ] change material, -/= brush size, E spawns a foe");
}

// Dev tool: renders every stage to a PNG (no window needed). `sand.exe --dump <dir>`
void dumpStages(const char* dir)
{
    auto GetTime = [] { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
    G.vw = 455; G.vh = 256;
    newGameKit(false);
    for (int s = 0; s < STAGE_COUNT; s++)
    {
        double t0 = GetTime();
        G.stage = s;
        generateStage(s);
        double t1 = GetTime();
        for (int i = 0; i < 60; i++) simulate(0, 0, W, H);
        double t2 = GetTime();
        Image img = GenImageColor(W, H, BLACK);
        std::vector<Color> px((size_t)W * H);
        renderWorld(px.data(), 0, 0, W, H);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) ImageDrawPixel(&img, x, y, px[(size_t)y * W + x]);
        for (auto& m : G.mobs) ImageDrawRectangle(&img, (int)m.x, (int)m.y, m.w, m.h, m.boss ? MAGENTA : RED);
        for (auto& it : G.inter) ImageDrawRectangle(&img, (int)it.x - 3, (int)it.y - 6, 6, 6, it.type == IT_PORTAL ? VIOLET : YELLOW);
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
            std::printf("  unreachable open cells: %d\n", lost);
        }
        std::printf("stage %d %-22s gen %.2fs  sim60 %.2fs (%.1fms/frame full map)  mobs %zu  traps %zu  objects %zu\n",
                    s, STAGES[s].name, t1 - t0, t2 - t1, (t2 - t1) / 60 * 1000, G.mobs.size(), G.traps.size(), G.inter.size());
    }
}
