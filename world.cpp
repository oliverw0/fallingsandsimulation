#include "world.h"
#include "util.h"
#include <algorithm>
#include <cmath>

using M = CellMaterial;

World world;

static Color lut[(int)M::Count][256];
static bool reactive[(int)M::Count];

static const int DIRS[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

static inline const MaterialProps& P(M m) { return props(m); }

void worldInit(int w, int h)
{
    world.w = w;
    world.h = h;
    world.cells.assign((size_t)w * h, Cell{});
    world.blasts.clear();
    world.debris.clear();
    world.bg.assign((size_t)w * h, Color{12, 12, 16, 255});
    world.sky.assign((size_t)w * h, 0);

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

void setCell(int x, int y, M m)
{
    if (!world.in(x, y))
        return;
    Cell& c = world.at(x, y);
    c.material = m;
    c.shade = (uint8_t)xr();
    c.flags = world.clock;
    c.life = defaultLife(m);
}

bool isSolid(int x, int y)
{
    if (!world.in(x, y))
        return true;
    const Cell& c = world.at(x, y);
    if (c.flags & CF_LOOSE)
        return false;
    if (c.material == M::Platform)
        return false; // one-way: handled by entity movement, not general collision
    Kind k = P(c.material).kind;
    return k == Kind::Solid || k == Kind::Powder;
}

bool isLiquidAt(int x, int y)
{
    return world.in(x, y) && P(world.at(x, y).material).kind == Kind::Liquid;
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
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy > r * r)
                continue;
            int x = cx + dx, y = cy + dy;
            if (!world.in(x, y))
                continue;
            M cur = world.at(x, y).material;
            if (cur == M::Bedrock)
                continue;
            if (onlyEmpty && cur != M::Empty)
                continue;
            if (m == M::Empty)
                world.at(x, y) = Cell{};
            else
                setCell(x, y, m);
        }
}

void ignite(int x, int y)
{
    if (!world.in(x, y))
        return;
    Cell& c = world.at(x, y);
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
    const Cell& t = world.at(x, y);
    if (t.material == M::Empty)
        return true;
    return isFluid(t) && dens(t) < dens(mover);
}

static inline void swapTo(int x, int y, int nx, int ny)
{
    Cell& a = world.at(x, y);
    Cell& b = world.at(nx, ny);
    std::swap(a, b);
    a.flags = (a.flags & ~CF_CLOCK) | world.clock;
    b.flags = (b.flags & ~CF_CLOCK) | world.clock;
}

static void updatePowder(int x, int y)
{
    Cell self = world.at(x, y);
    int cy = y;
    for (int s = 0; s < 3; s++)
    {
        if (!canSink(self, x, cy + 1))
            break;
        bool intoFluid = world.at(x, cy + 1).material != M::Empty;
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
    Cell self = world.at(x, y);
    int cy = y;
    for (int s = 0; s < 3; s++)
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

    int disp = P(self.material).dispersion;
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
    const Cell& t = world.at(x, y);
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
    Cell& c = world.at(x, y);
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
    int dx = irand(3) - 1;
    if (gasEnter(c, x + dx, y - 1)) { swapTo(x, y, x + dx, y - 1); return; }
    if (gasEnter(c, x, y - 1)) { swapTo(x, y, x, y - 1); return; }
    int d = (xr() & 1) ? 1 : -1;
    if (gasEnter(c, x + d, y)) { swapTo(x, y, x + d, y); return; }
}

static bool extinguishes(M m) { return m == M::Water || m == M::Blood || m == M::WetSand || m == M::Snow; }

static void updateFire(int x, int y)
{
    Cell& c = world.at(x, y);
    if (c.life == 0)
    {
        if (chance(3))
            setCell(x, y, M::Smoke);
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
        Cell& n = world.at(nx, ny);
        if (n.material == M::Water || n.material == M::Blood)
        {
            c = Cell{};
            if (chance(4))
                setCell(nx, ny, M::Steam);
            return;
        }
        if (n.material == M::Ice || n.material == M::Snow)
        {
            if (chance(10))
                setCell(nx, ny, M::Water);
        }
        else if (P(n.material).flammable && !(n.flags & CF_BURNING) && irand(100) < P(n.material).flammable)
            ignite(nx, ny);
    }
    if (chance(2))
    {
        int dx = irand(3) - 1;
        if (world.in(x + dx, y - 1) && world.at(x + dx, y - 1).material == M::Empty)
            swapTo(x, y, x + dx, y - 1);
    }
}

// Returns true if the cell is gone.
static bool updateBurning(int x, int y)
{
    Cell& c = world.at(x, y);
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
        setCell(x, y, M::Fire);
        world.at(x, y).life = 10;
        for (auto& d : DIRS)
            if (world.in(x + d[0], y + d[1]) && world.at(x + d[0], y + d[1]).material == M::Miasma)
                ignite(x + d[0], y + d[1]);
        return true;
    }
    for (auto& d : DIRS)
    {
        int nx = x + d[0], ny = y + d[1];
        if (world.in(nx, ny) && extinguishes(world.at(nx, ny).material))
        {
            c.flags &= ~CF_BURNING;
            if (world.at(nx, ny).material == M::Water && chance(2))
                setCell(nx, ny, M::Steam);
            return false;
        }
    }
    if (chance(3))
    {
        int nx = x + irand(3) - 1, ny = y - 1;
        if (world.in(nx, ny) && world.at(nx, ny).material == M::Empty)
            setCell(nx, ny, M::Fire);
    }
    const int* d = DIRS[irand(4)];
    int nx = x + d[0], ny = y + d[1];
    if (world.in(nx, ny))
    {
        Cell& n = world.at(nx, ny);
        if (P(n.material).flammable && !(n.flags & CF_BURNING) && irand(100) < P(n.material).flammable)
            ignite(nx, ny);
    }
    if (chance(2))
    {
        if (c.life > 0)
            c.life--;
        else
        {
            c = Cell{};
            if (chance(3))
                setCell(x, y, M::Smoke);
            return true;
        }
    }
    return false;
}

static bool acidProof(M m)
{
    return m == M::Empty || m == M::Acid || m == M::Glass || m == M::Bedrock || m == M::Metal;
}

static bool updateAcid(int x, int y)
{
    static const int AD[5][2] = {{0, 1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}};
    const int* d = AD[irand(5)];
    int nx = x + d[0], ny = y + d[1];
    if (!world.in(nx, ny))
        return false;
    Cell& n = world.at(nx, ny);
    if (acidProof(n.material))
        return false;
    Kind k = P(n.material).kind;
    if (k == Kind::Gas || k == Kind::Fire || k == Kind::Liquid)
        return false;
    if (!chance(8))
        return false;
    n = Cell{};
    if (chance(5))
        setCell(nx, ny, M::Smoke);
    if (chance(3))
    {
        world.at(x, y) = Cell{};
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
    Cell& n = world.at(nx, ny);
    if (P(n.material).flammable && !(n.flags & CF_BURNING) && irand(200) < P(n.material).flammable + 20)
        ignite(nx, ny);
    if (n.material == M::Empty && chance(400))
        setCell(nx, ny, M::Fire);
}

static bool tryReact(int x, int y)
{
    Cell& c = world.at(x, y);
    const int* d = DIRS[irand(4)];
    int nx = x + d[0], ny = y + d[1];
    if (!world.in(nx, ny))
        return false;
    Cell& n = world.at(nx, ny);
    if (n.material == M::Empty)
        return false;
    const Reaction* r = findReaction(c.material, n.material);
    if (!r)
        return false;
    if (r->chance > 1 && irand(r->chance) != 0)
        return false;
    bool flip = r->a != c.material; // reactions are unordered pairs
    M ra = flip ? r->resultB : r->resultA;
    M rb = flip ? r->resultA : r->resultB;
    if (c.material != ra) setCell(x, y, ra);
    if (n.material != rb) setCell(nx, ny, rb);
    return true;
}

static void updateCell(int x, int y)
{
    Cell& c = world.at(x, y);
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
            Cell& c = world.cells[(size_t)y * world.w + x];
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
}

// ---------------------------------------------------------------- explosions

static M crumbleOf(M m)
{
    switch (m)
    {
    case M::Stone: case M::Brick: case M::Basalt: case M::Obsidian: return M::Gravel;
    case M::Ice: return M::Snow;
    default: return M::Empty;
    }
}

void explodeCells(int cx, int cy, int r, int power)
{
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
            Cell& c = world.at(x, y);
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
                if (p.flammable) ignite(x, y);
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
                if (chance(10)) setCell(x, y, M::Fire);
                else if (chance(14)) setCell(x, y, M::Smoke);
            }
            else if (p.kind == Kind::Solid)
            {
                M cm = crumbleOf(m);
                if (cm == M::Empty) c = Cell{};
                else c.material = cm;
            }
            else if (p.flammable)
                ignite(x, y);
        }
}

// ---------------------------------------------------------------- rendering

static bool isGem(M m)
{
    return m == M::GoldOre || m == M::Firestone || m == M::Frostite || m == M::Stormite || m == M::Venomite || m == M::Adamantite;
}

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
        col = brighten(col, (int)(std::sin(f * 0.06f + x * 0.3f + y * 0.1f + (c.shade >> 4)) * 10));
        break;
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

void renderWorld(Color* px, int camX, int camY, int vw, int vh)
{
    // the moon hangs almost still while the land scrolls past beneath it
    float mx = vw * 0.74f - camX * 0.03f, my = vh * 0.15f - camY * 0.015f, mr = 11;
    for (int j = 0; j < vh; j++)
    {
        int y = camY + j;
        for (int i = 0; i < vw; i++)
        {
            int x = camX + i;
            Color col = {8, 8, 10, 255};
            if (world.in(x, y))
            {
                size_t k = (size_t)y * world.w + x;
                Color b = world.bg.empty() ? Color{12, 12, 16, 255} : world.bg[k];
                const Cell& c = world.cells[k];
                if (c.material == M::Empty)
                {
                    col = b;
                    if (world.sky[k])
                    {
                        float dx = i - mx, dy = j - my, d = std::sqrt(dx * dx + dy * dy);
                        if (d < mr) // pale disc with darker maria, lit from the right
                        {
                            float maria = fbm((dx + 40) * 0.22f, (dy + 40) * 0.22f, 404, 3);
                            Color moon = lerpColor(Color{236, 234, 216, 255}, Color{168, 170, 172, 255}, clampf((maria - 0.45f) * 3, 0, 1));
                            col = brighten(moon, (int)(-14 * clampf(-dx / mr, 0, 1)));
                        }
                        else if (d < mr * 5) // halo
                        {
                            float h = 1 - d / (mr * 5);
                            col = lerpColor(col, Color{120, 132, 170, 255}, h * h * 0.45f);
                        }
                        else if (hash2(x, y, 77) > 0.9965f && hash2(x, y, world.frame / 20) > 0.25f) // twinkling stars
                            col = Color{210, 214, 236, 255};
                    }
                }
                else
                {
                    col = cellColor(c, x, y);
                    Kind kd = P(c.material).kind;
                    if (kd == Kind::Solid || kd == Kind::Powder) // rim light on exposed edges
                    {
                        if (y > 0 && world.cells[k - world.w].material == M::Empty) col = brighten(col, 22);
                        else if (y < world.h - 1 && world.cells[k + world.w].material == M::Empty) col = brighten(col, -18);
                    }
                    if (col.a < 255) // composite translucent liquids/gases over the back wall
                    {
                        float a = col.a / 255.0f;
                        col = Color{(unsigned char)(b.r + (col.r - b.r) * a), (unsigned char)(b.g + (col.g - b.g) * a),
                                    (unsigned char)(b.b + (col.b - b.b) * a), 255};
                    }
                }
            }
            px[j * vw + i] = col;
        }
    }
}
