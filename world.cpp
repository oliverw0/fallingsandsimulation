#include <thread>
#include "world.h"
#include "util.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using M = CellMaterial;

World world;

static Color lut[(int)M::Count][256];
static bool reactive[(int)M::Count];
static const Reaction* pairRx[(int)M::Count][(int)M::Count]; // reactions.h as a lookup: either order

static const int DIRS[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

static inline const MaterialProps& P(M m) { return props(m); }

void worldInit(int w, int h, Cell fill, Color fillBg, int scale, int bgShift)
{
    world.w = w;
    world.h = h;
    world.scale = scale;
    world.bgShift = bgShift >= 0 ? bgShift : (scale == 2 ? 1 : 0);
    world.cw = (w + CS - 1) / CS;
    world.ch = (h + CS - 1) / CS;
    world.chunks.clear();
    world.chunks.resize((size_t)world.cw * world.ch);
    world.fill = fill;
    world.fillBg = fillBg;
    world.blasts.clear();
    world.debris.clear();
    world.disturbed.clear();
    static int gens = 0;
    world.gen = ++gens;

    static bool built = false;
    if (built)
        return;
    built = true;
    for (int m = 0; m < (int)M::Count; m++)
    {
        const MaterialProps& p = P((M)m);
        for (int s = 0; s < 256; s++)
            lut[m][s] = lerpColor(p.a, p.b, s / 255.0f);
        reactive[m] = false;
        for (const auto& r : reactions)
            if ((int)r.a == m || (int)r.b == m)
                reactive[m] = true;
    }
    for (const auto& r : reactions) // the first row listed for a pair wins, as findReaction did
    {
        if (!pairRx[(int)r.a][(int)r.b]) pairRx[(int)r.a][(int)r.b] = &r;
        if (!pairRx[(int)r.b][(int)r.a]) pairRx[(int)r.b][(int)r.a] = &r;
    }
}

// Rock that's never been touched gets its grain only when it's first written to.
Chunk& World::alloc(int x, int y)
{
    auto& slot = chunks[(size_t)(y / CS) * cw + x / CS];
    slot = std::make_unique<Chunk>();
    int x0 = x / CS * CS, y0 = y / CS * CS, nb = (CS >> bgShift) * (CS >> bgShift);
    slot->bg.reset(new Color[nb]);
    slot->sky.reset(new uint8_t[nb]);
    for (int k = 0; k < nb; k++) { slot->bg[k] = fillBg; slot->sky[k] = 0; }
    for (int j = 0; j < CS; j++)
        for (int i = 0; i < CS; i++)
        {
            Cell c = fill;
            if (c.material != M::Empty) c.shade = (uint8_t)(hash2((x0 + i) / (2 * scale), (y0 + j) / (2 * scale), 5) * 160);
            slot->cells[j * CS + i] = c;
        }
    return *slot;
}

static uint8_t defaultLife(M m)
{
    switch (m)
    {
    case M::Fire: return (uint8_t)irange(20, 45);
    case M::Smoke: return (uint8_t)irange(40, 90);
    case M::Steam: return (uint8_t)irange(120, 240);
    default: return 0;
    }
}

static void put(int x, int y, M m)
{
    if (!world.in(x, y))
        return;
    Cell& c = world.atq(x, y);
    c.material = m;
    c.shade = (uint8_t)xr();
    c.flags = world.clock;
    c.life = defaultLife(m);
}

static bool solidCell(int x, int y)
{
    if (!world.in(x, y))
        return true;
    const Cell& c = world.atq(x, y);
    if (c.flags & CF_LOOSE)
        return false;
    if (c.material == M::Platform)
        return false; // one-way: handled by entity movement, not general collision
    Kind k = P(c.material).kind;
    return k == Kind::Solid || k == Kind::Powder;
}



static void paintCells(int cx, int cy, int r, M m, bool onlyEmpty)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy > r * r)
                continue;
            int x = cx + dx, y = cy + dy;
            if (!world.in(x, y))
                continue;
            M cur = world.atq(x, y).material;
            if (cur == M::Bedrock)
                continue;
            if (onlyEmpty && cur != M::Empty)
                continue;
            if (m == M::Empty)
                world.atq(x, y) = Cell{};
            else
                put(x, y, m);
        }
}

static void burn(int x, int y)
{
    if (!world.in(x, y))
        return;
    Cell& c = world.atq(x, y);
    const MaterialProps& p = P(c.material);
    if (!p.flammable || (c.flags & CF_BURNING))
        return;
    c.flags |= CF_BURNING;
    if (c.material == M::Keg)
        c.life = (uint8_t)irange(4, 12);
    else if (c.material == M::Gunpowder)
        c.life = (uint8_t)irange(0, 3);
    else
        c.life = (uint8_t)std::min(255, p.burnTime / 2 + irand(p.burnTime / 2 + 1));
}

// ---------------------------------------------------------------- movement

static inline int dens(const Cell& c) { return (c.flags & CF_LOOSE) ? 30 : P(c.material).density; }

static inline bool isFluid(const Cell& c)
{
    if (c.flags & CF_LOOSE)
        return false;
    Kind k = P(c.material).kind;
    return k == Kind::Liquid || k == Kind::Gas || k == Kind::Fire;
}

// Can `mover` sink into (x, y)? Empty, or a lighter fluid.
static inline bool canSink(const Cell& mover, int x, int y)
{
    if (!world.in(x, y))
        return false;
    const Cell& t = world.atq(x, y);
    if (t.material == M::Empty)
        return true;
    return isFluid(t) && dens(t) < dens(mover);
}

static inline void swapTo(int x, int y, int nx, int ny)
{
    Cell& a = world.atq(x, y);
    Cell& b = world.atq(nx, ny);
    std::swap(a, b);
    a.flags = (a.flags & ~CF_CLOCK) | world.clock;
    b.flags = (b.flags & ~CF_CLOCK) | world.clock;
}

static void updatePowder(int x, int y)
{
    Cell self = world.atq(x, y);
    int cy = y;
    for (int s = 0; s < 3 * world.scale; s++)
    {
        if (!canSink(self, x, cy + 1))
            break;
        bool intoFluid = world.atq(x, cy + 1).material != M::Empty;
        swapTo(x, cy, x, cy + 1);
        cy++;
        if (intoFluid)
            break;
    }
    if (cy != y)
        return;
    int d = (xr() & 1) ? 1 : -1;
    if (canSink(self, x + d, y + 1)) { swapTo(x, y, x + d, y + 1); return; }
    if (canSink(self, x - d, y + 1)) { swapTo(x, y, x - d, y + 1); return; }
}

static void updateLiquid(int x, int y)
{
    Cell self = world.atq(x, y);
    int cy = y;
    for (int s = 0; s < 3 * world.scale; s++)
    {
        if (!canSink(self, x, cy + 1))
            break;
        swapTo(x, cy, x, cy + 1);
        cy++;
    }
    if (cy != y)
        return;
    int d = (xr() & 1) ? 1 : -1;
    if (canSink(self, x + d, y + 1)) { swapTo(x, y, x + d, y + 1); return; }
    if (canSink(self, x - d, y + 1)) { swapTo(x, y, x - d, y + 1); return; }

    int disp = P(self.material).dispersion * world.scale;
    if (self.material == M::Lava && !chance(3))
        return; // lava is sluggish
    for (int pass = 0; pass < 2; pass++)
    {
        int dir = pass ? -d : d;
        int best = 0;
        for (int i = 1; i <= disp; i++)
        {
            if (canSink(self, x + dir * i, y))
                best = i;
            else
                break;
        }
        if (best)
        {
            swapTo(x, y, x + dir * best, y);
            return;
        }
    }
}

static bool gasEnter(const Cell& g, int x, int y)
{
    if (!world.in(x, y))
        return false;
    const Cell& t = world.atq(x, y);
    if (t.material == M::Empty)
        return true;
    if (t.flags & CF_LOOSE)
        return false;
    Kind k = P(t.material).kind;
    if (k == Kind::Liquid)
        return chance(6); // bubble up through liquids
    if (k == Kind::Gas && t.material != g.material)
        return P(t.material).density > P(g.material).density;
    return false;
}

static void updateGas(int x, int y)
{
    Cell& c = world.atq(x, y);
    if (c.material != M::Miasma)
    {
        if (c.life > 0)
            c.life--;
        else
        {
            if (c.material == M::Steam && chance(3))
            {
                c.material = M::Water; // condense
                c.shade = (uint8_t)xr();
            }
            else
                c = Cell{};
            return;
        }
    }
    Cell g = c;
    for (int step = 0; step < world.scale; step++) // rise as fast, in world units, at any grid scale
    {
        int dx = irand(3) - 1;
        if (gasEnter(g, x + dx, y - 1)) { swapTo(x, y, x + dx, y - 1); x += dx; y--; continue; }
        if (gasEnter(g, x, y - 1)) { swapTo(x, y, x, y - 1); y--; continue; }
        int d = (xr() & 1) ? 1 : -1;
        if (gasEnter(g, x + d, y)) { swapTo(x, y, x + d, y); x += d; continue; }
        break;
    }
}

static bool extinguishes(M m) { return P(m).tags & T_EXTINGUISH; }

static void updateFire(int x, int y)
{
    Cell& c = world.atq(x, y);
    if (c.life == 0)
    {
        if (chance(3))
            put(x, y, M::Smoke);
        else
            c = Cell{};
        return;
    }
    c.life--;
    for (auto& d : DIRS)
    {
        int nx = x + d[0], ny = y + d[1];
        if (!world.in(nx, ny))
            continue;
        Cell& n = world.atq(nx, ny);
        if (n.material == M::Water || n.material == M::Blood)
        {
            c = Cell{};
            if (chance(4))
                put(nx, ny, M::Steam);
            return;
        }
        if (n.material == M::Ice || n.material == M::Snow)
        {
            if (chance(10))
                put(nx, ny, M::Water);
        }
        else if (P(n.material).flammable && !(n.flags & CF_BURNING) && irand(100) < P(n.material).flammable)
            burn(nx, ny);
    }
    if (chance(2))
    {
        int dx = irand(3) - 1;
        if (world.in(x + dx, y - 1) && world.atq(x + dx, y - 1).material == M::Empty)
            swapTo(x, y, x + dx, y - 1);
    }
}

// Returns true if the cell is gone.
static bool updateBurning(int x, int y)
{
    Cell& c = world.atq(x, y);
    M m = c.material;
    if (m == M::Gunpowder || m == M::Keg)
    {
        if (c.life > 0) { c.life--; return false; }
        c = Cell{};
        if (m == M::Keg)
            world.blasts.push_back({x, y, 16, 6, 70});
        else
            world.blasts.push_back({x, y, 4, 3, 10});
        return true;
    }
    if (m == M::Miasma)
    {
        put(x, y, M::Fire);
        world.atq(x, y).life = 10;
        for (auto& d : DIRS)
            if (world.in(x + d[0], y + d[1]) && world.atq(x + d[0], y + d[1]).material == M::Miasma)
                burn(x + d[0], y + d[1]);
        return true;
    }
    for (auto& d : DIRS)
    {
        int nx = x + d[0], ny = y + d[1];
        if (world.in(nx, ny) && extinguishes(world.atq(nx, ny).material))
        {
            c.flags &= ~CF_BURNING;
            if (world.atq(nx, ny).material == M::Water && chance(2))
                put(nx, ny, M::Steam);
            return false;
        }
    }
    if (chance(3))
    {
        int nx = x + irand(3) - 1, ny = y - 1;
        if (world.in(nx, ny) && world.atq(nx, ny).material == M::Empty)
            put(nx, ny, M::Fire);
    }
    const int* d = DIRS[irand(4)];
    int nx = x + d[0], ny = y + d[1];
    if (world.in(nx, ny))
    {
        Cell& n = world.atq(nx, ny);
        if (P(n.material).flammable && !(n.flags & CF_BURNING) && irand(100) < P(n.material).flammable)
            burn(nx, ny);
    }
    if (chance(2))
    {
        if (c.life > 0)
            c.life--;
        else
        {
            if (P(m).kind == Kind::Solid) disturb(x, y, 1); // a beam burnt through
            c = Cell{};
            if (chance(3))
                put(x, y, M::Smoke);
            return true;
        }
    }
    return false;
}

// Something here may still happen on a later roll (acid beside rock, a slow reaction): don't let the chunk doze off
// just because nothing changed this tick.
static void stayAwake(int x, int y)
{
    Chunk* c = world.chunk(x, y);
    if (c && c->awake < 2) c->awake = 2;
}

static bool acidProof(M m) { return m == M::Empty || (P(m).tags & T_ACIDPROOF); }

static const int AD[5][2] = {{0, 1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}};
static bool acidOnce(int x, int y)
{
    const int* d = AD[irand(5)];
    int nx = x + d[0], ny = y + d[1];
    if (!world.in(nx, ny))
        return false;
    Cell& n = world.atq(nx, ny);
    if (acidProof(n.material))
        return false;
    Kind k = P(n.material).kind;
    if (k == Kind::Gas || k == Kind::Fire || k == Kind::Liquid)
        return false;
    if (!chance(8))
        return false;
    if (k == Kind::Solid) disturb(nx, ny, 1);
    n = Cell{};
    if (chance(5))
        put(nx, ny, M::Smoke);
    if (chance(3))
    {
        world.atq(x, y) = Cell{};
        return true;
    }
    return false;
}

static void updateLava(int x, int y)
{
    const int* d = DIRS[irand(4)];
    int nx = x + d[0], ny = y + d[1];
    if (!world.in(nx, ny))
        return;
    Cell& n = world.atq(nx, ny);
    if (P(n.material).flammable && !(n.flags & CF_BURNING) && irand(200) < P(n.material).flammable + 20)
        burn(nx, ny);
    if (n.material == M::Empty && chance(400))
        put(nx, ny, M::Fire);
}

static bool reactOnce(int x, int y)
{
    Cell& c = world.atq(x, y);
    const int* d = DIRS[irand(4)];
    int nx = x + d[0], ny = y + d[1];
    if (!world.in(nx, ny))
        return false;
    Cell& n = world.atq(nx, ny);
    if (n.material == M::Empty)
        return false;
    const Reaction* r = pairRx[(int)c.material][(int)n.material];
    if (!r)
        return false;
    if (r->chance > 1 && irand(r->chance) != 0)
        return false;
    bool flip = r->a != c.material; // reactions are unordered pairs
    M ra = flip ? r->resultB : r->resultA;
    M rb = flip ? r->resultA : r->resultB;
    if (c.material != ra) put(x, y, ra);
    if (n.material != rb) put(nx, ny, rb);
    return true;
}

// Each tick only one random neighbour gets a roll; if any could react later, keep the chunk awake.
static bool updateAcid(int x, int y)
{
    if (acidOnce(x, y)) return true;
    for (auto& d : AD)
    {
        int nx = x + d[0], ny = y + d[1];
        if (!world.in(nx, ny)) continue;
        const Cell& n = world.atq(nx, ny);
        Kind k = P(n.material).kind;
        if (!acidProof(n.material) && (k == Kind::Solid || k == Kind::Powder)) { stayAwake(x, y); break; }
    }
    return false;
}

static bool tryReact(int x, int y)
{
    if (reactOnce(x, y)) return true;
    M m = world.atq(x, y).material;
    for (auto& d : DIRS)
        if (world.in(x + d[0], y + d[1]) && pairRx[(int)m][(int)world.atq(x + d[0], y + d[1]).material]) { stayAwake(x, y); break; }
    return false;
}

static void updateCell(int x, int y)
{
    Cell& c = world.atq(x, y);
    if ((c.flags & CF_BURNING) && updateBurning(x, y))
        return;
    M m = c.material;
    if (reactive[(int)m] && tryReact(x, y))
        return;
    if (m == M::Acid && updateAcid(x, y))
        return;
    if (m == M::Lava)
        updateLava(x, y);
    if (c.flags & CF_LOOSE)
    {
        updatePowder(x, y);
        return;
    }
    switch (P(m).kind)
    {
    case Kind::Powder: updatePowder(x, y); break;
    case Kind::Liquid: updateLiquid(x, y); break;
    case Kind::Gas: updateGas(x, y); break;
    case Kind::Fire: updateFire(x, y); break;
    default: break;
    }
}

// Single buffer, bottom-up, alternating scan direction per row.
// The clock bit stops a cell that moved from being updated twice in one tick.
void simulate(int x0, int y0, int x1, int y1)
{
    x0 = std::max(0, x0); y0 = std::max(0, y0);
    x1 = std::min(world.w, x1); y1 = std::min(world.h, y1);
    world.clock ^= CF_CLOCK;
    world.frame++;
    for (int y = y1 - 1; y >= y0; y--)
    {
        bool ltr = ((y + world.frame) & 1) != 0;
        for (int i = x0; i < x1; i++)
        {
            int x = ltr ? i : (x1 - 1 - (i - x0));
            Chunk* ch = world.chunk(x, y);
            if (!ch || !ch->awake) // untouched rock, or nothing's moved here lately: skip to the next chunk
            {
                int next = ltr ? (x | (CS - 1)) + 1 : (x & ~(CS - 1)) - 1;
                i += std::abs(next - x) - 1;
                continue;
            }
            Cell& c = ch->cells[World::idx(x, y)];
            if (c.material == M::Empty)
                continue;
            if ((c.flags & CF_CLOCK) == world.clock)
                continue;
            c.flags = (c.flags & ~CF_CLOCK) | world.clock;
            if (P(c.material).kind == Kind::Solid && !(c.flags & (CF_BURNING | CF_LOOSE)) && !reactive[(int)c.material])
                continue;
            updateCell(x, y);
        }
    }
    // which chunks changed this tick: they and their neighbours stay awake, the rest wind down
    int cx0 = x0 / CS, cx1 = (x1 - 1) / CS, cy0 = y0 / CS, cy1 = (y1 - 1) / CS;
    for (int cy = cy0; cy <= cy1; cy++)
        for (int cx = cx0; cx <= cx1; cx++)
        {
            Chunk* ch = world.chunks[(size_t)cy * world.cw + cx].get();
            if (!ch || !ch->awake) continue;
            uint32_t h = 2166136261u;
            const uint32_t* p = (const uint32_t*)ch->cells; // a Cell is 4 bytes; leave the clock bit out
            for (int k = 0; k < CS * CS; k++) h = (h ^ (p[k] & ~(uint32_t)(CF_CLOCK << 16))) * 16777619u;
            if (h != ch->hash)
            {
                ch->hash = h;
                ch->awake = 4;
                for (int ny = std::max(0, cy - 1); ny <= std::min(world.ch - 1, cy + 1); ny++)
                    for (int nx = std::max(0, cx - 1); nx <= std::min(world.cw - 1, cx + 1); nx++)
                    {
                        Chunk* n = world.chunks[(size_t)ny * world.cw + nx].get();
                        if (n) n->awake = std::max<uint8_t>(n->awake, 2);
                    }
            }
            else
                ch->awake--;
        }
}

// ---------------------------------------------------------------- the world-unit interface

static void blastCells(int cx, int cy, int r, int power);

void setCell(int x, int y, M m)
{
    int k = world.scale;
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++)
            if (world.in(x * k + i, y * k + j)) { world.at(x * k + i, y * k + j); put(x * k + i, y * k + j, m); }
}

void setCellC(int x, int y, M m)
{
    if (world.in(x, y)) { world.at(x, y); put(x, y, m); }
}

void ignite(int x, int y)
{
    int k = world.scale;
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++)
            if (world.in(x * k + i, y * k + j)) { world.at(x * k + i, y * k + j); burn(x * k + i, y * k + j); }
}

bool isSolidC(int x, int y) { return solidCell(x, y); }

bool isSolid(int x, int y)
{
    int k = world.scale;
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++)
            if (solidCell(x * k + i, y * k + j)) return true;
    return false;
}

bool isLiquidAt(int x, int y)
{
    return world.inU(x, y) && P(world.get(x * world.scale, y * world.scale).material).kind == Kind::Liquid;
}

bool lineOfSight(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)std::max(std::fabs(dx), std::fabs(dy));
    if (n < 1)
        return true;
    for (int i = 1; i < n; i++)
    {
        int x = (int)(x0 + dx * i / n), y = (int)(y0 + dy * i / n);
        if (isSolid(x, y))
            return false;
    }
    return true;
}

void paintCircle(int cx, int cy, int r, M m, bool onlyEmpty)
{
    int k = world.scale;
    for (int y = (cy - r) * k; y < (cy + r + 1) * k; y++)
        for (int x = (cx - r) * k; x < (cx + r + 1) * k; x++)
            if (world.in(x, y)) world.at(x, y); // wake what's painted on
    paintCells(cx * k + k / 2, cy * k + k / 2, r * k, m, onlyEmpty);
}

void explodeCells(int cx, int cy, int r, int power)
{
    int k = world.scale;
    for (int y = (cy - r) * k; y <= (cy + r) * k; y += CS / 2)
        for (int x = (cx - r) * k; x <= (cx + r) * k; x += CS / 2)
            if (world.in(x, y)) world.at(x, y);
    blastCells(cx * k + k / 2, cy * k + k / 2, r * k, power);
}

// ---------------------------------------------------------------- --selftest

// Each case builds a sealed bedrock pocket, so nothing can flow away, and runs the sim on it.
void materialSelfTest()
{
    uint32_t keep = rngState();
    rngState() = 12345;
    auto pocket = [](std::initializer_list<std::pair<int, M>> cells) {
        worldInit(12, 12);
        for (int y = 0; y < 12; y++)
            for (int x = 0; x < 12; x++) world.at(x, y).material = M::Bedrock;
        for (auto& c : cells) { world.at(c.first, 5) = Cell{}; put(c.first, 5, c.second); }
    };
    auto count = [](M m) { int n = 0; for (int y = 0; y < world.h; y++) for (int x = 0; x < world.w; x++) n += world.get(x, y).material == m; return n; };
    auto run = [](int n) { for (int i = 0; i < n; i++) simulate(0, 0, 12, 12); };
    const char* fail = nullptr;

    pocket({{5, M::Water}, {6, M::Lava}}); // water + lava -> steam + stone
    run(200);
    if (count(M::Lava) || !count(M::Stone)) fail = "water and lava didn't make stone";

    pocket({{4, M::Stone}, {5, M::Acid}, {6, M::Glass}}); // acid eats stone, never glass
    run(400);
    if (!fail && count(M::Glass) != 1) fail = "acid ate glass";
    if (!fail && count(M::Stone)) fail = "acid left the stone";

    pocket({{5, M::Wood}, {6, M::Water}}); // water puts out a burning beam
    burn(5, 5);
    run(3);
    if (!fail && (world.get(5, 5).material != M::Wood || (world.get(5, 5).flags & CF_BURNING))) fail = "water didn't put out burning wood";

    worldInit(40, 40); // a blast leaves gravel at the edge of the crater, and bedrock stands
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 40; x++) world.at(x, y).material = y < 38 ? M::Stone : M::Bedrock;
    explodeCells(20, 30, 10, 3);
    if (!fail && (!count(M::Gravel) || world.get(20, 39).material != M::Bedrock)) fail = "blast didn't crumble stone to gravel";
    world.debris.clear();
    world.blasts.clear();
    world.disturbed.clear();

    rngState() = keep;
    std::printf("materials: %s\n", fail ? fail : "ok");
}

// ---------------------------------------------------------------- explosions

static M crumbleOf(M m) { return P(m).crumble; }

static void blastCells(int cx, int cy, int r, int power)
{
    disturb(cx, cy, r);
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            int d2 = dx * dx + dy * dy;
            if (d2 > r2)
                continue;
            int x = cx + dx, y = cy + dy;
            if (!world.in(x, y))
                continue;
            Cell& c = world.atq(x, y);
            M m = c.material;
            if (m == M::Empty)
                continue;
            const MaterialProps& p = P(m);
            float dist = std::sqrt((float)d2) + 0.01f;
            float t = dist / (r + 0.01f);
            float nx = dx / dist, ny = dy / dist;

            if (m == M::Keg || m == M::Gunpowder || m == M::Miasma)
            {
                if (t < 0.5f && m != M::Keg) { c = Cell{}; continue; }
                if (!(c.flags & CF_BURNING))
                {
                    c.flags |= CF_BURNING;
                    c.life = (uint8_t)irange(1, m == M::Keg ? 8 : 4);
                }
                continue;
            }
            if (p.hardness > power)
            {
                if (p.flammable) burn(x, y);
                continue;
            }
            if (p.ore)
            {
                c.flags |= CF_LOOSE; // ore survives as collectable nuggets
                if (chance(3))
                {
                    world.debris.push_back({x + 0.5f, y + 0.5f, nx * frange(1, 3), ny * frange(1, 3) - 1.5f, c});
                    c = Cell{};
                }
                continue;
            }
            if (t < 0.7f || chance(2))
            {
                if ((p.kind == Kind::Solid || p.kind == Kind::Powder || p.kind == Kind::Liquid) &&
                    chance(p.kind == Kind::Liquid ? 3 : 7))
                {
                    Cell dc = c;
                    if (p.kind == Kind::Solid) dc.material = crumbleOf(m);
                    dc.flags &= ~CF_BURNING;
                    if (dc.material != M::Empty)
                        world.debris.push_back({x + 0.5f, y + 0.5f, nx * frange(1, 3.5f), ny * frange(1, 3.5f) - 1.5f, dc});
                }
                c = Cell{};
                if (chance(10)) put(x, y, M::Fire);
                else if (chance(14)) put(x, y, M::Smoke);
            }
            else if (p.kind == Kind::Solid)
            {
                M cm = crumbleOf(m);
                if (cm == M::Empty) c = Cell{};
                else c.material = cm;
            }
            else if (p.flammable)
                burn(x, y);
        }
}

// ---------------------------------------------------------------- rendering

static bool isGem(M m) { return P(m).tags & T_GEM; }

// The water shimmer as a sine table (a screen of water is a million cells); built before any render thread runs.
static const struct ShimmerLut { int8_t v[256]; ShimmerLut() { for (int i = 0; i < 256; i++) v[i] = (int8_t)std::lround(std::sin(i * 2 * PI / 256) * 10); } int8_t operator[](int i) const { return v[i]; } } shimmer;

Color cellColor(const Cell& c, int x, int y)
{
    int f = world.frame;
    M m = c.material;
    Color col = lut[(int)m][c.shade];
    switch (m)
    {
    case M::Fire:
        col = lerpColor(Color{190, 40, 10, 230}, Color{255, 236, 130, 255}, std::min(1.0f, c.life / 40.0f));
        break;
    case M::Lava:
    {
        float w = 0.5f + 0.5f * std::sin(f * 0.04f + x * 0.21f + y * 0.13f + c.shade * 0.02f);
        col = lerpColor(col, Color{255, 222, 96, 255}, w * 0.4f);
        break;
    }
    case M::Water: case M::Acid: case M::Blood: case M::Oil:
    {
        col = brighten(col, shimmer[(f * 5 / 2 + x * 12 + y * 4 + (c.shade >> 4) * 41) & 255]); // same shimmer: 256 steps a turn
        break;
    }
    default:
        if (isGem(m) && hash2(x, y, f / 8) > 0.985f)
            col = lerpColor(col, WHITE, 0.7f);
        break;
    }
    if (c.flags & CF_LOOSE)
    {
        col = brighten(col, 30);
        if (hash2(x, y, f / 6) > 0.97f) col = WHITE;
    }
    if (c.flags & CF_BURNING)
    {
        float h = hash2(x, y, f / 3);
        if (h > 0.45f)
            col = lerpColor(col, h > 0.8f ? Color{255, 220, 90, 255} : Color{240, 90, 20, 255}, 0.75f);
    }
    return col;
}

static void renderRows(Color* px, int camX, int camY, int vw, int vh, int j0, int j1);

// Every pixel is independent and only reads the world, so the screen is drawn in bands, one per core.
void renderWorld(Color* px, int camX, int camY, int vw, int vh)
{
    static const int nt = (int)std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::vector<std::thread> ts;
    for (int t = 1; t < nt; t++) ts.emplace_back(renderRows, px, camX, camY, vw, vh, vh * t / nt, vh * (t + 1) / nt);
    renderRows(px, camX, camY, vw, vh, 0, vh / nt);
    for (auto& t : ts) t.join();
}

static void renderRows(Color* px, int camX, int camY, int vw, int vh, int j0, int j1)
{
    // the moon hangs almost still while the land scrolls past beneath it
    float mx = vw * 0.74f - camX * 0.03f, my = vh * 0.15f - camY * 0.015f, mr = 11.0f * world.scale;
    float mr2 = mr * mr, halo2 = mr2 * 25;
    const Color out = {8, 8, 10, 255};
    for (int j = j0; j < j1; j++)
    {
        int y = camY + j;
        Color* row = px + (size_t)j * vw;
        if (y < 0 || y >= world.h) { std::fill(row, row + vw, out); continue; }
        float dy = j - my, dy2 = dy * dy;
        int ly = y & (CS - 1);
        for (int i = 0; i < vw;)
        {
            int x0 = camX + i;
            if (x0 < 0 || x0 >= world.w) { row[i++] = out; continue; }
            Chunk* ch = world.chunk(x0, y); // one lookup per run of cells inside a chunk
            int end = std::min(vw, i + CS - (x0 & (CS - 1)));
            for (; i < end; i++)
            {
                int x = camX + i, k = ly * CS + (x & (CS - 1)), kb = world.bidx(x, y);
                Color b = ch ? ch->bg[kb] : world.fillBg;
                Cell c = ch ? ch->cells[k] : world.fill;
                if (!ch && c.material != M::Empty) c.shade = (uint8_t)(hash2(x / 2, y / 2, 5) * 160);
                Color col;
                if (c.material == M::Empty)
                {
                    col = b;
                    if (ch && ch->sky[kb])
                    {
                        float dx = i - mx, d2 = dx * dx + dy2;
                        if (d2 < mr2) // pale disc with darker maria, lit from the right
                        {
                            float maria = fbm((dx / world.scale + 40) * 0.22f, (dy / world.scale + 40) * 0.22f, 404, 3);
                            Color moon = lerpColor(Color{236, 234, 216, 255}, Color{168, 170, 172, 255}, clampf((maria - 0.45f) * 3, 0, 1));
                            col = brighten(moon, (int)(-14 * clampf(-dx / mr, 0, 1)));
                        }
                        else if (d2 < halo2) // halo
                        {
                            float h = 1 - std::sqrt(d2) / (mr * 5);
                            col = lerpColor(col, Color{120, 132, 170, 255}, h * h * 0.45f);
                        }
                        else if (hash2(x, y, 77) > 1 - 0.0035f / (world.scale * world.scale) && hash2(x, y, world.frame / 20) > 0.25f) // twinkling stars
                            col = Color{210, 214, 236, 255};
                        if (world.storm > 0.01f) // storm clouds roll in over moon and stars, lit by the lightning
                        {
                            float sx = (x / (float)world.scale) * 0.006f + world.frame * 0.0012f, sy = (y / (float)world.scale) * 0.016f;
                            float cl = vnoise(sx, sy, 911) * 0.65f + vnoise(sx * 2.7f, sy * 2.7f + world.frame * 0.002f, 912) * 0.35f;
                            float cover = clampf((cl - 0.75f + world.storm * 0.75f) * 3.0f, 0, 1) * world.storm;
                            Color cloud = lerpColor(Color{30, 30, 40, 255}, Color{58, 60, 72, 255}, clampf((cl - 0.4f) * 2, 0, 1));
                            col = lerpColor(lerpColor(col, Color{10, 10, 16, 255}, world.storm * 0.5f), cloud, cover);
                            if (world.flash > 0.01f) col = brighten(col, (int)(world.flash * (70 + 90 * cover)));
                        }
                    }
                }
                else
                {
                    col = cellColor(c, x, y);
                    Kind kd = P(c.material).kind;
                    if (kd == Kind::Solid || kd == Kind::Powder) // rim light on exposed edges
                    {
                        M up = ch && ly > 0 ? ch->cells[k - CS].material : (y > 0 ? world.get(x, y - 1).material : M::Bedrock);
                        if (up == M::Empty) col = brighten(col, 22);
                        else
                        {
                            M dn = ch && ly < CS - 1 ? ch->cells[k + CS].material : (y < world.h - 1 ? world.get(x, y + 1).material : M::Bedrock);
                            if (dn == M::Empty) col = brighten(col, -18);
                        }
                    }
                    if (col.a < 255) // composite translucent liquids/gases over the back wall
                    {
                        int a = col.a;
                        col = Color{(unsigned char)(b.r + (col.r - b.r) * a / 255), (unsigned char)(b.g + (col.g - b.g) * a / 255),
                                    (unsigned char)(b.b + (col.b - b.b) * a / 255), 255};
                    }
                }
                row[i] = col;
            }
        }
    }
}
