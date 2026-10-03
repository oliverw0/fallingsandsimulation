#include "game.h"
#include "util.h"
#include "sprites.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

using M = CellMaterial;

Game G;

const char* ELEMENT_NAMES[EL_COUNT] = {"Physical", "Fire", "Frost", "Shock", "Poison"};
const Color ELEMENT_COLORS[EL_COUNT] = {{220, 220, 220, 255}, {255, 120, 30, 255}, {140, 210, 255, 255}, {240, 240, 110, 255}, {120, 230, 90, 255}};

static const Color FLESH = {150, 20, 24, 255};
static const Color BONEC = {220, 216, 196, 255};

// name, w, h, hp, speed, dmg, ai, attack element, resist {phys, fire, ice, shock, poison}, range, cooldown, noclip, blood, gore
const EnemyDef ENEMIES[ENEMY_COUNT] = {
    {"Goblin", 10, 16, 22, 0.9f, 7, AI_WALK, EL_PHYS, {1, 1, 1, 1, 1}, 8, 40, false, FLESH, M::Blood},
    {"Goblin Bomber", 10, 16, 18, 0.7f, 18, AI_BOMB, EL_FIRE, {1, 1, 1, 1, 1}, 150, 120, false, FLESH, M::Blood},
    {"Skeleton", 10, 22, 32, 0.6f, 9, AI_WALK, EL_PHYS, {1, 1, 0.6f, 1, 0}, 9, 45, false, BONEC, M::Bone},
    {"Skeleton Archer", 10, 22, 24, 0.55f, 8, AI_RANGED, EL_PHYS, {1, 1, 0.6f, 1, 0}, 190, 100, false, BONEC, M::Bone},
    {"Giant Bat", 14, 8, 10, 1.3f, 5, AI_FLY, EL_PHYS, {1, 1, 1, 1, 1}, 0, 50, false, FLESH, M::Blood},
    {"Acid Slime", 14, 12, 28, 1.0f, 8, AI_HOP, EL_POISON, {1, 1, 1, 1, 0}, 0, 50, false, {120, 240, 90, 255}, M::Acid},
    {"Cultist", 10, 22, 30, 0.6f, 12, AI_RANGED, EL_FIRE, {1, 0.7f, 1, 1, 1}, 170, 130, false, FLESH, M::Blood},
    {"Knight", 12, 24, 90, 0.5f, 16, AI_WALK, EL_PHYS, {0.5f, 0.8f, 1, 1.6f, 1}, 11, 55, false, FLESH, M::Blood},
    {"Fire Imp", 10, 13, 20, 1.0f, 8, AI_FLYCAST, EL_FIRE, {1, 0, 2, 1, 1}, 150, 90, false, {255, 140, 40, 255}, M::Fire},
    {"Frost Wraith", 12, 20, 34, 0.8f, 10, AI_FLYCAST, EL_ICE, {0.7f, 2, 0, 1, 0}, 160, 110, true, {170, 220, 255, 255}, M::Snow},
    {"Rock Golem", 18, 23, 150, 0.4f, 22, AI_WALK, EL_PHYS, {0.6f, 0.5f, 0.8f, 0.5f, 0}, 13, 70, false, {130, 126, 120, 255}, M::Gravel},
    {"Dire Wolf", 16, 10, 18, 1.5f, 6, AI_WALK, EL_PHYS, {1, 1, 1, 1, 1}, 8, 35, false, FLESH, M::Blood},
    {"Redcap", 8, 16, 26, 1.15f, 10, AI_WALK, EL_PHYS, {1, 1, 1, 1, 1}, 8, 38, false, FLESH, M::Blood},
    {"Draugr", 10, 23, 60, 0.55f, 14, AI_WALK, EL_ICE, {0.8f, 1.3f, 0, 1, 0}, 10, 55, false, {120, 150, 170, 255}, M::Bone},
    {"Troll", 20, 27, 220, 0.5f, 26, AI_WALK, EL_PHYS, {0.8f, 1.5f, 1, 1, 0.7f}, 16, 80, false, {90, 130, 60, 255}, M::Blood},
    {"Banshee", 12, 21, 40, 0.8f, 14, AI_FLYCAST, EL_SHOCK, {0.5f, 1, 1, 0, 0}, 170, 120, true, {220, 220, 240, 255}, M::Steam},
    {"Kelpie", 18, 14, 55, 1.0f, 14, AI_WALK, EL_ICE, {1, 1.2f, 0.5f, 2, 1}, 10, 45, false, {60, 90, 140, 255}, M::Water},
    {"Castle Guard", 10, 23, 45, 0.7f, 12, AI_WALK, EL_PHYS, {0.8f, 1, 1, 1.2f, 1}, 12, 50, false, FLESH, M::Blood},
    {"Risen Levy", 10, 22, 30, 0.55f, 9, AI_WALK, EL_PHYS, {1, 1.4f, 1, 1, 0}, 10, 50, false, {110, 30, 30, 255}, M::Blood},
    {"The Black Knight", 20, 27, 900, 0.75f, 25, AI_BOSS_KNIGHT, EL_PHYS, {0.6f, 0.8f, 1, 1.3f, 1}, 14, 60, false, FLESH, M::Blood},
    {"The Lich King", 16, 23, 1500, 0.9f, 20, AI_BOSS_LICH, EL_ICE, {0.8f, 1, 0.5f, 1, 0}, 220, 70, true, BONEC, M::Bone},
};

static std::vector<Mob> pendingMobs; // spawned during the mob loop

// ================================================================ helpers

void message(const std::string& s) { G.msgs.push_back({s, 240}); if (G.msgs.size() > 5) G.msgs.erase(G.msgs.begin()); }
void addText(float x, float y, const std::string& s, Color col) { G.texts.push_back({x, y, s, 50, col}); }

void spawnParticle(float x, float y, float vx, float vy, int life, Color col, float grav)
{
    if (G.parts.size() > 5000)
        return;
    G.parts.push_back({x, y, vx, vy, life, col, grav});
}

static void spawnCellParticle(float x, float y, float vx, float vy, M mat, uint8_t flags)
{
    if (G.parts.size() > 5000)
        return;
    Cell c;
    c.material = mat;
    c.shade = (uint8_t)xr();
    c.flags = flags;
    Particle p{x, y, vx, vy, 200, cellColor(c, (int)x, (int)y), 0.15f};
    p.toCell = mat;
    p.cellFlags = flags;
    G.parts.push_back(p);
}

// Noita-style splashes: surface liquid near (x, y) leaps out of the grid as particles with the given push,
// flies under gravity and turns back into liquid wherever it lands.
static void splashLiquid(float x, float y, float vx, float vy, float r, int n)
{
    int sc = world.scale, R = std::max(1, (int)(r * sc)), cx = (int)(x * sc), cy = (int)(y * sc);
    for (int t = 0; t < n; t++) // each throw peels the top cell off a column: only the surface leaps
    {
        int px = cx + irange(-R, R);
        for (int py = cy - R; py <= cy + R; py++)
        {
            if (!world.in(px, py - 1) || !world.in(px, py)) continue;
            Cell c = world.get(px, py);
            if (props(c.material).kind != Kind::Liquid || world.get(px, py - 1).material != M::Empty) continue;
            world.at(px, py) = Cell{};
            world.debris.push_back({px + 0.5f, py + 0.5f, (vx + frange(-0.5f, 0.5f)) * sc, (vy * frange(0.6f, 1.2f) + frange(-0.4f, 0.2f)) * sc, c});
            break;
        }
    }
}

// The render camera keeps a whole-pixel offset from the player, so the hero sits still on
// screen while the world scrolls under them (no 1px shimmer between sprite and terrain).
void syncRenderCamera()
{
    Mob& pm = G.p.m;
    G.rcx = (int)std::floor(pm.x) + (int)std::lround(G.camX - pm.x);
    G.rcy = (int)std::floor(pm.y) + (int)std::lround(G.camY - pm.y);
    if (world.wU() > G.vw) G.rcx = std::max(0, std::min(world.wU() - G.vw, G.rcx));
    if (world.hU() > G.vh) G.rcy = std::max(0, std::min(world.hU() - G.vh, G.rcy));
}

Vector2 mouseWorld()
{
    Vector2 m = GetMousePosition();
    return {m.x / G.scale + G.rcx, m.y / G.scale + G.rcy};
}

// Loose crates and barrels near the camera, as boxes: solid to anything that walks (rebuilt by updateChests).
static std::vector<Rectangle> propBoxes;
static int propBoxGen = -1; // the world they belong to

bool boxSolid(float x, float y, int w, int h) // in units, tested against every cell it covers
{
    if (propBoxGen == world.gen)
        for (auto& r : propBoxes)
            if (x < r.x + r.width && x + w > r.x && y < r.y + r.height && y + h > r.y) return true;
    float k = (float)world.scale;
    int x0 = (int)std::floor(x * k), y0 = (int)std::floor(y * k);
    int x1 = (int)std::floor((x + w) * k - 0.01f), y1 = (int)std::floor((y + h) * k - 0.01f);
    for (int yy = y0; yy <= y1; yy++)
        for (int xx = x0; xx <= x1; xx++)
            if (isSolidC(xx, yy))
                return true;
    return false;
}

static bool platformRow(float x, int w, int row)
{
    for (int xx = (int)std::floor(x); xx <= (int)std::floor(x + w - 0.01f); xx++)
        if (world.matU(xx, row) == M::Platform) return true;
    for (auto& it : G.inter) // a chest's lid is a one-way platform too: hop up and stand on it
    {
        if (it.type != IT_CHEST) continue;
        Vector2 hb = bodyHalf(it);
        float cs = std::fabs(std::cos(it.ang)), sn = std::fabs(std::sin(it.ang));
        float ex = hb.x * cs + hb.y * sn, ey = hb.x * sn + hb.y * cs, cx = it.x, cy = it.y - hb.y;
        if (row == (int)std::floor(cy - ey) && x + w > cx - ex + 1 && x < cx + ex - 1) return true;
    }
    return false;
}

bool onPlatform(const Mob& m) { return platformRow(m.x, m.w, (int)std::floor(m.y + m.h + 0.05f)); }

// Moves a box with collision. Returns bit 1 if x was blocked, bit 2 if y was blocked.
static int moveBy(Mob& m, float dx, float dy, bool step)
{
    int res = 0;
    int n = (int)std::ceil(std::max(std::fabs(dx), std::fabs(dy)));
    if (n < 1) n = 1;
    float sx = dx / n, sy = dy / n;
    for (int i = 0; i < n; i++)
    {
        if (sx != 0)
        {
            if (!boxSolid(m.x + sx, m.y, m.w, m.h))
                m.x += sx;
            else
            {
                bool climbed = false;
                if (step)
                    for (int up = 1; up <= 4; up++)
                        if (!boxSolid(m.x + sx, m.y - up, m.w, m.h))
                        {
                            m.x += sx;
                            m.y -= up;
                            climbed = true;
                            break;
                        }
                if (!climbed)
                {
                    res |= 1;
                    sx = 0;
                }
            }
        }
        if (sy > 0 && m.dropT == 0) // landing on a one-way platform from above
        {
            int oldRow = (int)std::floor(m.y + m.h - 0.01f), newRow = (int)std::floor(m.y + m.h + sy - 0.01f);
            if (newRow > oldRow && platformRow(m.x, m.w, newRow))
            {
                m.y = newRow - (float)m.h;
                res |= 2;
                sy = 0;
            }
        }
        if (sy != 0)
        {
            if (!boxSolid(m.x, m.y + sy, m.w, m.h))
                m.y += sy;
            else
            {
                res |= 2;
                if (sy > 0)
                {
                    float R = std::floor(m.y + m.h + sy - 0.01f);
                    m.y = std::max(m.y, R - m.h);
                }
                else
                    m.y = std::min(m.y, std::floor(m.y + sy) + 1);
                sy = 0;
            }
        }
    }
    return res;
}

static void moveMob(Mob& m)
{
    if (boxSolid(m.x, m.y, m.w, m.h))
    {
        // buried by sand or rubble: pop up if there is room, otherwise wriggle through
        bool ok = false;
        for (int up = 1; up <= 6 && !ok; up++)
            if (!boxSolid(m.x, m.y - up, m.w, m.h)) { m.y -= up; ok = true; }
        if (!ok)
        {
            m.x += m.vx * 0.5f;
            m.y += std::min(m.vy, 0.0f) * 0.5f - 0.2f;
            return;
        }
    }
    float vx = m.vx, vy = m.vy;
    bool wasIn = m.inLiquid;
    int r = moveBy(m, m.vx, m.vy, m.onGround);
    m.wall = (r & 1) ? (vx > 0 ? 1 : -1) : 0;
    if (r & 1) m.vx = 0;
    if (r & 2) m.vy = 0;
    if (m.dropT > 0) m.dropT--;
    m.onGround = (boxSolid(m.x, m.y + m.h + 0.05f - 0.0f, m.w, 1) || (m.dropT == 0 && onPlatform(m))) && m.vy >= 0;
    int liquid = 0;
    for (int yy = (int)m.y; yy < (int)(m.y + m.h); yy++)
        for (int xx = (int)m.x; xx < (int)(m.x + m.w); xx++)
            if (isLiquidAt(xx, yy)) liquid++;
    m.inLiquid = liquid * 3 > m.w * m.h;
    if (m.h >= 10) // bodies throw the water about: diving in, bursting out, wading through
    {
        if (!wasIn && m.inLiquid && vy > 1.2f) splashLiquid(m.cx(), m.cy(), vx * 0.4f, -vy * 0.5f, m.h * 0.6f, (int)(vy * m.w * 1.5f));
        else if (wasIn && !m.inLiquid && vy < -1.0f) splashLiquid(m.cx(), m.cy() + m.h * 0.3f, vx * 0.4f, vy * 0.6f, m.h * 0.5f, (int)(-vy * m.w));
        else if (m.inLiquid && std::fabs(vx) > 0.5f && (G.frame + m.id) % 3 == 0) splashLiquid(m.cx() + (vx > 0 ? 1 : -1) * m.w * 0.6f, m.cy(), vx * 1.2f, -1.0f, m.h * 0.5f, 2);
    }
}

static bool overlap(const Mob& a, const Mob& b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

// ================================================================ pickups & loot

void addPickup(float x, float y, int kind)
{
    Pickup p;
    p.kind = kind;
    p.b.x = x; p.b.y = y; p.b.w = 8; p.b.h = 8;
    p.b.vx = frange(-1, 1); p.b.vy = frange(-2.5f, -1);
    G.pickups.push_back(p);
}
void addPickupSpell(float x, float y, int spell)
{
    addPickup(x, y, PU_SPELL);
    G.pickups.back().spell = spell;
}
void addPickupWeapon(float x, float y, const Weapon& w)
{
    addPickup(x, y, PU_WEAPON);
    G.pickups.back().weapon = w;
}

static int stageOre()
{
    const StageDef& sd = STAGES[std::min(G.stage, STAGE_COUNT - 1)];
    int total = 0;
    for (auto& o : sd.ores) total += o.w;
    int r = irand(total + 4);
    if (r >= total) return (int)M::GoldOre;
    for (auto& o : sd.ores)
    {
        if (r < o.w) return o.id;
        r -= o.w;
    }
    return (int)M::CopperOre;
}

void spawnOreBurst(float x, float y, int count)
{
    M ore = (M)stageOre();
    for (int i = 0; i < count; i++)
    {
        if (chance(4)) ore = (M)stageOre();
        spawnCellParticle(x + frange(-2, 2), y + frange(-2, 2), frange(-1.5f, 1.5f), frange(-3, -0.5f), ore, CF_LOOSE);
    }
}

static void dropLoot(float x, float y, bool rich)
{
    int tier = G.stage;
    if (rich || chance(3)) spawnOreBurst(x, y, rich ? irange(50, 90) : irange(6, 16));
    if (rich || chance(8)) addPickupSpell(x, y, randomSpell(tier));
    if (rich) addPickupSpell(x, y, randomSpell(tier + 1));
    if (chance(9)) addPickup(x, y, PU_HEART);
    if (chance(90)) addPickup(x, y, PU_POTION);
    if (rich || chance(170)) addPickupWeapon(x, y, randomWeapon(tier + (rich ? 1 : 0)));
    if (rich || chance(400)) { addPickup(x, y, PU_AMULET); G.pickups.back().spell = randomAmulet(); }
    if (G.stage >= 1 && (rich ? chance(2) : chance(300))) addPickupWeapon(x, y, rollLegendary(tier + 1, false));
}

void addCoins(float x, float y, int count, int value)
{
    for (int i = 0; i < count; i++)
    {
        addPickup(x + frange(-3, 3), y, PU_COIN);
        G.pickups.back().spell = value; // coin value
        G.pickups.back().b.vy = frange(-3.5f, -1.5f);
        G.pickups.back().b.vx = frange(-1.5f, 1.5f);
    }
}

void placeDisplay(float x, float y, int style, const Weapon& w)
{
    Interact it{IT_STONE, x, y};
    it.data = (int)G.stoneLoot.size();
    it.style = style;
    G.stoneLoot.push_back(w);
    G.inter.push_back(it);
}

// ---------------------------------------------------------------- breakable crates and barrels

static bool inCrate(const Interact& it, int x, int y) { return x >= it.x && x < it.x + it.w && y >= it.y - it.h && y < it.y; }

// Knock one wood cell loose as a tumbling splinter.
static void splinter(int x, int y, float kx, float power)
{
    int k = world.scale;
    spawnParticle(x + 0.5f, y + 0.5f, kx * frange(0.3f, 1.2f) + frange(-power, power), frange(-power * 1.5f, -0.3f), irange(40, 90), cellColor(world.get(x * k, y * k), x * k, y * k), 0.15f);
    for (int j = 0; j < k; j++)
        for (int i = 0; i < k; i++) world.at(x * k + i, y * k + j) = Cell{};
}

static void breakCrate(Interact& it, float kx, bool loud)
{
    it.used = true;
    for (int y = (int)it.y - it.h; y < (int)it.y; y++)
        for (int x = (int)it.x; x < (int)it.x + it.w; x++)
            if (world.inU(x, y) && world.matU(x, y) == M::Wood) splinter(x, y, kx, 1.8f);
    for (int k = 0; k < 14; k++) // dust
        spawnParticle(it.x + frange(0, (float)it.w), it.y - frange(0, (float)it.h), frange(-0.6f, 0.6f), frange(-0.8f, -0.1f), irange(20, 40), {150, 130, 104, 160}, -0.01f);
    if (loud) { playAt(SFX_SMASH, it.x + it.w / 2.0f, it.y, 0.9f, frange(0.9f, 1.1f)); G.shake = std::max(G.shake, 3.0f); }
    if (chance(4)) addCoins(it.x + it.w / 2.0f, it.y - 4, irange(1, 2), 1);
    else if (chance(12)) addPickup(it.x + it.w / 2.0f, it.y - 4, PU_POTION);
}

static void damageCrate(Interact& it, int dmg, float kx)
{
    it.data -= dmg;
    it.hit = 8;
    if (it.data <= 0) { breakCrate(it, kx, true); return; }
    playAt(SFX_KNOCK, it.x + it.w / 2.0f, it.y, 0.8f, frange(0.9f, 1.15f));
    for (int k = 0; k < 6; k++) // chip a few planks off the struck side
    {
        int x = kx > 0 ? (int)it.x + irand(3) : (int)it.x + it.w - 1 - irand(3), y = (int)it.y - 1 - irand(it.h);
        if (world.inU(x, y) && world.matU(x, y) == M::Wood) splinter(x, y, kx, 1.0f);
    }
}

void hitCrateAt(int x, int y, int dmg, float kx)
{
    for (auto& it : G.inter)
        if (it.type == IT_CRATE && !it.used && inCrate(it, x, y)) { damageCrate(it, dmg, kx); return; }
}

// A weapon drawn in the world (on racks, in graves...): grip at `g`, pointing along `ang`.
void drawWorldWeapon(const Weapon& w, Vector2 g, float ang, float len)
{
    (void)len; // the sprite sets its own length
    drawWeaponSprite(w, g, ang, 0.5f, false);
}

// Noita-style legendary aura: a pulsing halo with slow light rays.
void drawGlow(float x, float y, Color c, float r)
{
    float p = 0.75f + 0.25f * std::sin(G.frame * 0.08f);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient((int)x, (int)y, r * p, {c.r, c.g, c.b, (unsigned char)(80 * p)}, {c.r, c.g, c.b, 0});
    DrawCircleGradient((int)x, (int)y, r * 0.4f, {c.r, c.g, c.b, (unsigned char)(130 * p)}, {c.r, c.g, c.b, 0});
    for (int k = 0; k < 4; k++)
    {
        float a = G.frame * 0.012f + k * PI / 2;
        DrawLineEx({x, y}, {x + std::cos(a) * r * 1.3f, y + std::sin(a) * r * 1.3f}, 2, {c.r, c.g, c.b, (unsigned char)(35 * p)});
    }
    EndBlendMode();
}

// ================================================================ new game

void newGameKit(bool sandbox)
{
    G.p = Player{};
    Mob& m = G.p.m;
    m.type = -1;
    m.w = 7; m.h = 21;
    m.hp = m.maxHp = 100;

    Weapon sword; sword.type = W_SWORD; sword.metal = M_COPPER;
    Weapon xb; xb.type = W_CROSSBOW; xb.metal = M_COPPER;
    Weapon st; st.type = W_STAFF;
    st.staff.name = "Apprentice's Staff";
    st.staff.manaMax = st.staff.mana = 120;
    st.staff.regen = 40 / 60.0f;
    st.staff.delay = 8; st.staff.recharge = 25; st.staff.spread = 3;
    st.staff.slots = {makeCard(SP_SPARK), makeCard(SP_SPARK), SpellCard{}, SpellCard{}};
    st.staff.gem = SKYBLUE;
    G.p.hotbar = {sword, xb, st};
    G.p.bag = {makeCard(SP_DIG), makeCard(SP_BOMB)};
    if (!sandbox) applyLoadout(); // real runs start with a frying pan plus whatever you've unlocked
    else
    {
        G.p.hotbar.insert(G.p.hotbar.begin(), fryingPan());
        G.p.hotbar[2].staff.slots.assign(10, SpellCard{});
        G.p.hotbar[2].staff.slots[0] = makeCard(SP_SPARK);
        G.p.hotbar[2].staff.manaMax = G.p.hotbar[2].staff.mana = 600;
        G.p.hotbar[2].staff.regen = 4;
        G.p.bag.clear();
        for (int s = 0; s < SPELL_COUNT; s++) { G.p.bag.push_back(makeCard(s)); G.p.bag.push_back(makeCard(s)); }
        for (int r = 0; r < RES_COUNT; r++) G.p.res[r] = 999;
        G.p.potions = 9;
    }
    G.stage = 0;
    G.sandbox = sandbox;
    G.banked = false;
}

Mob makeEnemy(int type, float x, float y)
{
    const EnemyDef& d = ENEMIES[type];
    Mob m;
    m.type = type;
    m.id = G.nextId++;
    int sc = (type == E_BLACKKNIGHT || type == E_LICH) ? 2 : 1; // bosses are drawn at 2x
    m.w = d.w * sc; m.h = d.h * sc;
    m.x = x - m.w / 2.0f; m.y = y - m.h;
    bool boss = type == E_BLACKKNIGHT || type == E_LICH;
    float scale = boss ? 1.0f : 1.0f + 0.35f * G.stage;
    m.hp = m.maxHp = d.hp * scale;
    m.dmg = d.dmg * (boss ? 1.0f : 1.0f + 0.2f * G.stage);
    m.boss = boss;
    m.timer = irand(200);
    m.state = irand(2) ? 1 : -1;
    return m;
}

// ================================================================ damage

static void bleed(const Mob& m, Color col, int n)
{
    for (int i = 0; i < n; i++)
        spawnParticle(m.cx(), m.cy(), frange(-1.5f, 1.5f), frange(-2, 0.5f), irange(10, 30), col, 0.15f);
}

// The next damageMob's blow, set by whoever strikes (a weapon, a bolt, a blast) and used up by it.
static int nextHit = HK_NONE;
static float nextAng = 0, nextK = 0;
static Vector2 nextAt = {-1e9f, 0}; // where a stab or a bolt went in

void damageMob(Mob& m, float dmg, Element el, float kx, float ky, int flags)
{
    int hk = nextHit;
    Vector2 at = nextAt;
    nextHit = HK_NONE; nextAt.x = -1e9f;
    if (!m.alive)
        return;
    bool isP = &m == &G.p.m;
    if (isP && (flags & DMG_HIT) && m.iframes > 0)
        return;
    float mult = 1;
    bool immune = false;
    if (isP)
    {
        mult *= 1 - armourDef(G.p.armour);
        if (G.p.amulet == AM_HELM) mult *= 0.7f;
        if (G.p.amulet == AM_TROLLCROSS && el != EL_PHYS) mult *= 0.5f;
        if (G.p.armour >= 0 && el != EL_PHYS && METALS[G.p.armour].el == el)
        {
            mult *= 0.2f;
            immune = true;
        }
        if (G.sandbox) mult *= 0.25f;
    }
    else
    {
        mult *= ENEMIES[m.type].resist[el];
        immune = ENEMIES[m.type].resist[el] <= 0;
        m.aggro = true; // whatever hit it, it's coming for you now
    }
    if (m.shock > 0) mult *= 1.25f;
    float d = dmg * mult;

    if ((flags & DMG_HIT) && !immune)
    {
        switch (el)
        {
        case EL_FIRE:
            if (m.wet) { m.wet = std::max(0, m.wet - 200); spawnParticle(m.cx(), m.y, 0, -0.6f, 30, {200, 200, 210, 160}, -0.01f); } // a hiss of steam
            else m.burn = std::max(m.burn, m.oily ? 360 : 180);
            break;
        case EL_ICE: m.chill = std::max(m.chill, 150); break;
        case EL_SHOCK: m.shock = std::max(m.shock, 25); break;
        case EL_POISON: m.poison = std::max(m.poison, 300); break;
        default: break;
        }
    }
    m.hp -= d;
    if (!isP && (flags & DMG_HIT))
    {
        m.lastHit = (uint8_t)hk; m.lastAng = nextAng; m.lastK = nextK;
        if ((hk == HK_PIERCE || hk == HK_BOLT) && at.x > -1e8f) // the wound stays: it bleeds, and a bolt sticks
        {
            uint8_t s;
            float t, rel;
            if (rigLocate(m, at, nextAng, s, t, rel))
            {
                if (m.nWounds == 4)
                {
                    for (int k = 0; k < 3; k++) { m.woundS[k] = m.woundS[k + 1]; m.woundT[k] = m.woundT[k + 1]; m.woundA[k] = m.woundA[k + 1]; m.woundK[k] = m.woundK[k + 1]; }
                    m.nWounds = 3;
                }
                int k = m.nWounds++;
                m.woundS[k] = s; m.woundT[k] = t; m.woundA[k] = rel;
                m.woundK[k] = hk == HK_BOLT ? WK_BOLT : WK_PIERCE;
            }
        }
    }
    if (flags & DMG_HIT)
    {
        m.vx += kx;
        m.vy += ky;
        m.hurtFlash = 6;
        if (!isP) // it reels: knocked off its feet for a moment (a guardian only flinches)
        {
            m.hitT = 14;
            m.hitDir = kx > 0.05f ? 1.0f : (kx < -0.05f ? -1.0f : (float)-m.facing);
            if (!m.boss) m.stagger = std::max(m.stagger, 10 + (int)std::min(d, 30.0f) / 3);
        }
        if (isP) { m.iframes = 40; if (d >= 1) { G.hitstop = std::max(G.hitstop, 2); playSfx(SFX_HURT, 0.8f); } }
        Color bc = isP ? FLESH : ENEMIES[m.type].blood;
        if (el == EL_PHYS && d >= 2 && (isP || ENEMIES[m.type].gore == M::Blood)) m.bloody = std::max(m.bloody, 360);
        bleed(m, bc, 4 + (int)std::min(d, 20.0f) / 3);
        if (d >= 0.5f)
            addText(m.cx(), m.y - 2, std::to_string((int)std::round(d)), isP ? Color{255, 80, 80, 255} : ELEMENT_COLORS[el]);
    }
}

static void chainShock(const Mob& src, float dmg)
{
    Mob* best = nullptr;
    float bd = 45;
    for (auto& o : G.mobs)
    {
        if (!o.alive || o.id == src.id) continue;
        float d = std::hypot(o.cx() - src.cx(), o.cy() - src.cy());
        if (d < bd) { bd = d; best = &o; }
    }
    if (!best) return;
    for (int i = 0; i < 12; i++)
    {
        float t = i / 12.0f;
        spawnParticle(src.cx() + (best->cx() - src.cx()) * t + frange(-1, 1), src.cy() + (best->cy() - src.cy()) * t + frange(-1, 1), 0, 0, 8, {250, 250, 150, 255}, 0);
    }
    damageMob(*best, dmg, EL_SHOCK, 0, -0.5f, DMG_HIT);
}

// ================================================================ loose props
// A crate, barrel or box takes a few blows (`data` = hits left), then bursts into splinters. Barrels may hold
// water or lamp oil; crates and boxes sometimes a few coins.
static void breakProp(Interact& it, float kx)
{
    it.used = true;
    Vector2 hb = bodyHalf(it);
    float cx = it.x, cy = it.y - hb.y;
    playAt(SFX_SMASH, cx, cy, 0.8f, frange(0.95f, 1.15f));
    for (int k = 0; k < 28; k++)
        spawnParticle(cx + frange(-hb.x, hb.x), cy + frange(-hb.y, hb.y), kx * 0.4f + frange(-1.6f, 1.6f), frange(-2.6f, -0.4f), irange(40, 80),
                      k % 3 ? Color{150, 108, 62, 255} : Color{86, 58, 32, 255}, 0.15f);
    if (it.style % 3 == 1 && !chance(3))
        for (int k = 0; k < 60; k++) spawnCellParticle(cx + frange(-hb.x, hb.x) * 0.7f, cy + frange(-hb.y, hb.y) * 0.7f, kx * 0.3f + frange(-1.2f, 1.2f), frange(-1.6f, 0.2f), chance(3) ? M::Oil : M::Water, 0);
    else if (!G.inVillage && chance(3)) addCoins(cx, cy, irange(1, 3) + G.stage / 2, 1);
}

// A blow of `dmg` hits from the side `kx` (its push): it's knocked along, or it breaks.
static void hitProp(Interact& it, int dmg, float kx, float ky)
{
    if (it.used) return;
    if ((it.data -= dmg) <= 0) { breakProp(it, kx); return; }
    it.vx += kx; it.vy += ky;
    it.va += (kx > 0 ? 1 : -1) * frange(0.02f, 0.06f) * std::fabs(kx);
    it.rest = 0;
    playAt(SFX_KNOCK, it.x, it.y, 0.8f, frange(0.9f, 1.2f));
}

// ================================================================ hanging lanterns
// An iron lantern full of lamp oil on a chain. It swings when you brush past it; a blow snaps the chain and
// sends it flying, and wherever it lands (or if it's struck again) the glass breaks and the oil runs out burning.
static void smashLantern(Interact& it)
{
    if (it.used) return;
    it.used = true;
    Vector2 p = lanternPos(it);
    playAt(SFX_SMASH, p.x, p.y, 0.7f, 1.5f);
    playAt(SFX_FIRE, p.x, p.y, 0.6f);
    float vx = it.style ? it.vx * 0.4f : 0;
    for (int k = 0; k < 70; k++) // the oil, splashing out as cells that land where they fall
        spawnCellParticle(p.x + frange(-1.5f, 1.5f), p.y + frange(-1.5f, 1.5f), vx + frange(-1.4f, 1.4f), frange(-1.8f, 0.3f), M::Oil, 0);
    for (int k = 0; k < 14; k++) // glass and iron
        spawnParticle(p.x, p.y, frange(-1.6f, 1.6f), frange(-2.2f, -0.3f), irange(14, 30), k % 3 ? Color{255, 214, 140, 255} : Color{60, 60, 66, 255}, 0.15f);
    for (int k = 0; k < 20; k++)
        spawnParticle(p.x, p.y, frange(-1, 1), frange(-1.5f, 0), irange(10, 26), lerpColor({255, 90, 20, 255}, {255, 230, 120, 255}, frand()), 0.02f);
    int sc = world.scale; // the wick sets it alight
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
        {
            int cx = ((int)p.x + dx) * sc, cy = ((int)p.y + dy) * sc;
            if (dx * dx + dy * dy <= 4 && world.in(cx, cy) && world.get(cx, cy).material == M::Empty) setCellC(cx, cy, M::Oil);
            if (dx * dx + dy * dy <= 2) ignite((int)p.x + dx, (int)p.y + dy);
        }
}

// A blow from the side `kx` (its strength): a hanging lantern's chain snaps; a loose one breaks.
static void hitLantern(Interact& it, float kx)
{
    if (it.used) return;
    if (it.style) { smashLantern(it); return; }
    Vector2 p = lanternPos(it);
    float tang = std::cos(it.ang) * it.va * it.data; // it keeps the speed of its swing
    it.style = 1;
    it.x = p.x; it.y = p.y;
    it.vx = tang + kx; it.vy = -std::fabs(kx) * 0.5f - 0.5f;
    it.ang = -it.ang; // (the drawing turns the other way once it is loose)
    it.va = kx * 0.08f;
    playAt(SFX_CLANG, p.x, p.y, 0.5f, 1.6f);
}

void explode(float x, float y, int r, float dmg, Element el, bool friendly, int power)
{
    explodeCells((int)x, (int)y, r, power);
    pushRagdolls(x, y, r * 2.2f, r * 0.35f);
    for (auto& c : G.corpses) // the dead are thrown
    {
        Vector2 cc = corpseCentre(c);
        float dx = cc.x - x, dy = cc.y - y, d = std::sqrt(dx * dx + dy * dy), R = r * 2.5f + 10;
        if (d > R) continue;
        float k = r * 0.35f * (1 - d / R), nx = d > 0.1f ? dx / d : 0, ny = d > 0.1f ? dy / d : -1;
        corpseKick(c, {x, y}, {nx * k, ny * k - k * 0.6f});
    }
    playAt(SFX_EXPLODE, x, y, std::min(1.0f, 0.35f + r / 14.0f), r > 10 ? 0.8f : 1.1f);
    G.shake = std::min(14.0f, G.shake + r * 0.6f);
    for (int i = 0; i < r * 6; i++)
    {
        float a = frand() * 6.2832f, s = frange(0.4f, 2.4f) * r / 6.0f;
        Color c = lerpColor({255, 90, 20, 255}, {255, 240, 140, 255}, frand());
        spawnParticle(x, y, std::cos(a) * s, std::sin(a) * s, irange(8, 22), c, 0.03f);
    }
    for (int i = 0; i < r * 2; i++)
        spawnParticle(x + frange(-r, r) * 0.5f, y + frange(-r, r) * 0.5f, frange(-0.3f, 0.3f), frange(-0.6f, -0.1f), irange(30, 60), {70, 70, 74, 200}, -0.005f);

    for (auto& it : G.inter) // and send chests tumbling
    {
        if (it.type == IT_LANTERN && !it.used)
        {
            Vector2 p = lanternPos(it);
            if (std::hypot(p.x - x, p.y - y) < r * 2.0f + 6) smashLantern(it);
            continue;
        }
        if (!isBody(it) || (it.type == IT_PROP && it.used)) continue;
        if (it.type == IT_PROP && std::hypot(it.x - x, it.y - bodyHalf(it).y - y) < r + 6) { breakProp(it, it.x > x ? 2.0f : -2.0f); continue; }
        float dx = it.x - x, dy = it.y - bodyHalf(it).y - y, d = std::sqrt(dx * dx + dy * dy), R = r * 2.0f + 10;
        if (d > R) continue;
        float k = r * 0.25f * (1 - d / R), nx = d > 0.1f ? dx / d : 0, ny = d > 0.1f ? dy / d : -1;
        it.vx += nx * k;
        it.vy += ny * k - k * 0.6f;
        it.va += frange(-0.06f, 0.06f) * k + nx * 0.02f * k;
        it.rest = 0;
    }
    auto hit = [&](Mob& m, bool isP) {
        float dx = m.cx() - x, dy = m.cy() - y;
        float d = std::sqrt(dx * dx + dy * dy);
        float R = r * 1.4f + std::max(m.w, m.h) * 0.5f;
        if (d > R) return;
        float f = 1 - d / R * 0.6f;
        float k = (r / 4.0f) * f;
        float nx = d > 0.1f ? dx / d : 0, ny = d > 0.1f ? dy / d : -1;
        float dd = dmg * f;
        if (isP && friendly) dd *= 0.5f;
        nextHit = HK_BLAST; nextAng = std::atan2(ny, nx); nextK = (float)r;
        damageMob(m, dd, el == EL_PHYS ? EL_FIRE : el, nx * k, ny * k - 1, DMG_HIT);
    };
    for (auto& m : G.mobs)
        if (m.alive) hit(m, false);
    if (G.p.m.alive) hit(G.p.m, true);
}

static void freezeArea(float x, float y, int r)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            if (dx * dx + dy * dy > r * r) continue;
            int k = world.scale;
            for (int j = 0; j < k; j++)
                for (int i = 0; i < k; i++)
                {
                    int cx = ((int)x + dx) * k + i, cy = ((int)y + dy) * k + j;
                    if (!world.in(cx, cy)) continue;
                    M m = world.get(cx, cy).material;
                    if (m == M::Water || m == M::Blood) setCellC(cx, cy, M::Ice);
                    else if (m == M::Lava && chance(2)) setCellC(cx, cy, M::Obsidian);
                    else if (m == M::Fire) world.at(cx, cy) = Cell{};
                }
        }
}

static void electrify(float x, float y, bool friendly)
{
    bool wet = false;
    for (int dy = -3; dy <= 3 && !wet; dy++)
        for (int dx = -3; dx <= 3 && !wet; dx++)
            if (isLiquidAt((int)x + dx, (int)y + dy)) wet = true;
    if (!wet) return;
    for (int i = 0; i < 40; i++)
        spawnParticle(x + frange(-40, 40), y + frange(-10, 10), 0, 0, irange(4, 12), {250, 250, 160, 255}, 0);
    auto zap = [&](Mob& m) {
        if (m.alive && (m.inLiquid || m.wet) && std::hypot(m.cx() - x, m.cy() - y) < 70)
            damageMob(m, 25, EL_SHOCK, 0, -1, DMG_HIT);
    };
    for (auto& m : G.mobs) zap(m);
    zap(G.p.m);
    (void)friendly;
}

// ================================================================ projectiles

void spawnSpell(const Shot& sh, float x, float y, float ang, bool friendly)
{
    if (G.projs.size() > 700) return;
    const SpellDef& d = SPELLS[sh.spell];
    const Mods& m = sh.mods;
    Proj p;
    p.kind = PK_SPELL;
    p.spell = sh.spell;
    p.x = x; p.y = y;
    float spd = d.speed * m.speedMul;
    p.vx = std::cos(ang) * spd;
    p.vy = std::sin(ang) * spd;
    p.dmg = (d.dmg + m.dmg) * (friendly ? spellPower() : 1.0f);
    p.el = m.elSet ? m.el : d.el;
    p.life = d.life;
    p.grav = d.grav + m.grav;
    p.blast = d.blast + m.blast;
    p.bounce = m.bounce;
    p.homing = m.homing;
    p.pierce = m.pierce;
    p.trailFire = m.trailFire;
    p.friendly = friendly;
    p.col = d.col;
    if (m.elSet) p.col = lerpColor(p.col, ELEMENT_COLORS[m.el], 0.6f);
    p.payload = sh.payload;
    p.power = sh.spell == SP_BOMB ? 6 : (sh.spell == SP_FIREBALL ? 4 : 3);
    p.fuse = sh.spell == SP_BOMB;
    G.projs.push_back(p);
    if (friendly) // a flash at the staff's tip
        for (int k = 0; k < 5; k++)
            spawnParticle(x, y, std::cos(ang + frange(-0.6f, 0.6f)) * frange(0.4f, 1.4f), std::sin(ang + frange(-0.6f, 0.6f)) * frange(0.4f, 1.4f), irange(5, 10), lerpColor(p.col, WHITE, 0.5f), 0);
    int snd = p.el == EL_FIRE ? SFX_FIRE : (p.el == EL_SHOCK ? SFX_ZAP : (p.el == EL_ICE ? SFX_ICE : SFX_CAST));
    playAt(snd, x, y, friendly ? 0.5f : 0.4f, sh.spell == SP_DIG ? 0.7f : 1.0f);
}

static void enemyProj(int kind, float x, float y, float vx, float vy, float dmg, Element el, float grav, int blast)
{
    Proj p;
    p.kind = kind;
    p.x = x; p.y = y; p.vx = vx; p.vy = vy;
    p.dmg = dmg; p.el = el; p.grav = grav; p.blast = blast;
    p.friendly = false;
    p.life = kind == PK_BOMB ? 160 : 200;
    p.col = kind == PK_ROCK ? Color{130, 126, 120, 255} : Color{200, 180, 140, 255};
    G.projs.push_back(p);
    playAt(kind == PK_ARROW ? SFX_BOW : SFX_SWING, x, y, 0.5f, kind == PK_ARROW ? 1.2f : 0.6f);
}

static void enemySpell(int spell, const Mob& m, float ang, float dmg, Element el)
{
    Shot sh{spell, Mods{}, nullptr};
    if (el != SPELLS[spell].el) { sh.mods.el = el; sh.mods.elSet = true; }
    spawnSpell(sh, m.cx(), m.cy(), ang, false);
    Proj& p = G.projs.back();
    p.dmg = dmg;
    p.trailFire = false;
}

static void projImpact(Proj& p, float x, float y)
{
    p.alive = false;
    if (p.blast > 0)
        explode(x, y, p.blast, p.dmg * (p.kind == PK_SPELL && p.spell == SP_BOMB ? 1.0f : 0.8f), p.el, p.friendly, p.power);
    if (p.kind == PK_SPELL)
    {
        switch (p.spell)
        {
        case SP_ACID: paintCircle((int)x, (int)y, 3, M::Acid, true); break;
        case SP_WATER: paintCircle((int)x, (int)y, 4, M::Water, true); break;
        case SP_ICE: freezeArea(x, y, 6); break;
        case SP_LIGHTNING: electrify(x, y, p.friendly); break;
        case SP_FIREBALL:
            for (int i = 0; i < 14; i++)
            {
                int fx = (int)x + irange(-5, 5), fy = (int)y + irange(-5, 5);
                if (world.inU(fx, fy))
                {
                    if (world.matU(fx, fy) == M::Empty) setCell(fx, fy, M::Fire);
                    else ignite(fx, fy);
                }
            }
            break;
        default: break;
        }
    }
    if (p.kind == PK_ROCK) paintCircle((int)x, (int)y, 2, M::Gravel, true);
    if (p.el == EL_FIRE)
        for (int i = 0; i < 4; i++) ignite((int)x + irange(-2, 2), (int)y + irange(-2, 2));
    if (p.el == EL_ICE && p.spell != SP_ICE) freezeArea(x, y, 3);
    if (p.payload)
    {
        float a = std::atan2(p.vy, p.vx);
        fireShots(*p.payload, x - std::cos(a) * 2, y - std::sin(a) * 2, a, 0, p.friendly);
        p.payload.reset();
    }
    for (int i = 0; i < 14; i++) // a burst on impact
    {
        float a = i * PI / 7 + frange(-0.2f, 0.2f), sp = frange(0.6f, 2.0f);
        spawnParticle(x, y, std::cos(a) * sp, std::sin(a) * sp, irange(8, 18), lerpColor(p.col, WHITE, frand() * 0.4f), 0.02f);
    }
}

static void hitMob(Proj& p, Mob& m)
{
    float sp = std::hypot(p.vx, p.vy) + 0.01f;
    if (p.kind == PK_BOLT || p.kind == PK_ARROW) { nextHit = HK_BOLT; nextAng = std::atan2(p.vy, p.vx); nextK = sp; nextAt = {p.x, p.y}; }
    damageMob(m, p.dmg, p.el, p.vx / sp * 1.5f, p.vy / sp * 1.5f - 0.5f, DMG_HIT);
    if (p.el == EL_SHOCK && p.friendly) chainShock(m, p.dmg * 0.5f);
}

static bool pointIn(const Mob& m, float x, float y)
{
    return x >= m.x - 1 && x <= m.x + m.w + 1 && y >= m.y - 1 && y <= m.y + m.h + 1;
}

static void stepProj(Proj& p)
{
    if (--p.life <= 0)
    {
        if (p.fuse || p.payload || p.kind == PK_BOMB) projImpact(p, p.x, p.y);
        else p.alive = false;
        return;
    }
    if (p.homing)
    {
        Mob* t = nullptr;
        float bd = 150;
        if (p.friendly)
        {
            for (auto& m : G.mobs)
            {
                if (!m.alive) continue;
                float d = std::hypot(m.cx() - p.x, m.cy() - p.y);
                if (d < bd) { bd = d; t = &m; }
            }
        }
        else if (G.p.m.alive)
            t = &G.p.m;
        if (t)
        {
            float want = std::atan2(t->cy() - p.y, t->cx() - p.x);
            float cur = std::atan2(p.vy, p.vx);
            float diff = std::remainder(want - cur, 2 * PI);
            cur += clampf(diff, -0.1f, 0.1f);
            float sp = std::hypot(p.vx, p.vy);
            p.vx = std::cos(cur) * sp;
            p.vy = std::sin(cur) * sp;
        }
    }
    p.vy += p.grav;
    float spd = std::hypot(p.vx, p.vy);
    int n = std::max(1, (int)std::ceil(spd));
    for (int s = 0; s < n; s++)
    {
        float nx = p.x + p.vx / n, ny = p.y + p.vy / n;
        int cx = (int)std::floor(nx), cy = (int)std::floor(ny);
        if (!world.inU(cx, cy)) { p.alive = false; return; }
        const Cell& c = world.get(cx * world.scale, cy * world.scale);
        Kind k = props(c.material).kind;
        bool solid = isSolid(cx, cy);
        if (k == Kind::Liquid && !isLiquidAt((int)std::floor(p.x), (int)std::floor(p.y))) splashLiquid(nx, ny, p.vx * 0.4f, p.vy * 0.3f - 1.2f, 3, std::min(24, 4 + (int)(spd * 2))); // smacks into the water
        if (p.kind == PK_SPELL && p.spell == SP_DIG && solid)
        {
            int sc = world.scale;
            bool hard = false;
            for (int j = 0; j < sc; j++)
                for (int i = 0; i < sc; i++)
                {
                    Cell& d = world.at(cx * sc + i, cy * sc + j);
                    if (props(d.material).kind != Kind::Solid && props(d.material).kind != Kind::Powder) continue;
                    if (props(d.material).hardness > 4) { hard = true; continue; }
                    if (props(d.material).ore) d.flags |= CF_LOOSE;
                    else d = Cell{};
                }
            if (hard && isSolid(cx, cy)) { p.alive = false; return; }
            disturb(cx * sc, cy * sc, sc);
            p.life--;
        }
        else if (solid)
        {
            bool bx = isSolid((int)std::floor(nx), (int)std::floor(p.y));
            bool by = isSolid((int)std::floor(p.x), (int)std::floor(ny));
            if (p.bounce > 0 || p.fuse)
            {
                float damp = p.fuse ? 0.35f : 0.85f;
                if (bx) p.vx = -p.vx * damp;
                if (by) p.vy = -p.vy * damp;
                if (!bx && !by) { p.vx = -p.vx * damp; p.vy = -p.vy * damp; }
                if (!p.fuse) p.bounce--;
                return;
            }
            if (p.friendly) hitCrateAt(cx, cy, 1, p.vx > 0 ? 1.0f : -1.0f);
            projImpact(p, p.x, p.y);
            return;
        }
        else if (k == Kind::Liquid)
        {
            if (p.kind == PK_SPELL && p.spell == SP_LIGHTNING) { projImpact(p, nx, ny); return; }
            if (p.el == EL_FIRE && c.material == M::Water && !p.blast)
            {
                setCell(cx, cy, M::Steam);
                p.alive = false;
                return;
            }
            if (p.el == EL_ICE && c.material == M::Water) { projImpact(p, nx, ny); return; }
            p.vx *= 0.96f;
            p.vy *= 0.96f;
        }
        p.x = nx;
        p.y = ny;
        if (p.trailFire && c.material == M::Empty && chance(3)) setCell(cx, cy, M::Fire);

        if (p.friendly)
        {
            for (auto& m : G.mobs)
            {
                if (!m.alive || !pointIn(m, nx, ny)) continue;
                if (p.pierce)
                {
                    if (std::find(p.hits.begin(), p.hits.end(), m.id) != p.hits.end()) continue;
                    p.hits.push_back(m.id);
                    hitMob(p, m);
                    continue;
                }
                hitMob(p, m);
                projImpact(p, nx, ny);
                return;
            }
        }
        else if (G.p.m.alive && pointIn(G.p.m, nx, ny))
        {
            hitMob(p, G.p.m);
            projImpact(p, nx, ny);
            return;
        }
        if (p.kind == PK_SPELL && p.spell == SP_LIGHTNING && chance(2))
            spawnParticle(p.x, p.y, frange(-0.2f, 0.2f), frange(-0.2f, 0.2f), irange(3, 8), p.col, 0);
    }
    if (p.kind == PK_SPELL && p.spell != SP_LIGHTNING && p.spell != SP_DIG)
        switch (p.el) // each element leaves its own wake
        {
        case EL_FIRE: spawnParticle(p.x + frange(-1, 1), p.y + frange(-1, 1), frange(-0.3f, 0.3f), frange(-0.7f, -0.1f), irange(10, 22), chance(2) ? Color{255, 200, 80, 255} : Color{255, 100, 30, 255}, -0.02f); break;
        case EL_ICE: spawnParticle(p.x + frange(-1, 1), p.y + frange(-1, 1), frange(-0.2f, 0.2f), frange(0, 0.3f), irange(12, 24), chance(3) ? WHITE : Color{170, 220, 255, 255}, 0.02f); break;
        case EL_POISON: spawnParticle(p.x, p.y, frange(-0.1f, 0.1f), 0.2f, irange(10, 20), {130, 230, 80, 255}, 0.08f); break;
        case EL_SHOCK: spawnParticle(p.x + frange(-2, 2), p.y + frange(-2, 2), frange(-1, 1), frange(-1, 1), irange(3, 6), {255, 255, 200, 255}, 0); break;
        default: spawnParticle(p.x, p.y, frange(-0.25f, 0.25f), frange(-0.25f, 0.25f), irange(8, 16), lerpColor(p.col, WHITE, frand() * 0.6f), 0); break;
        }
    if (p.fuse && G.frame % 4 == 0)
        spawnParticle(p.x + 3, p.y - 14, frange(-0.3f, 0.3f), -0.4f, 8, {255, 200, 80, 255}, 0);
}

// ================================================================ structural collapse
// Terrain only stays up through what it's attached to. Whenever something is blown up, burnt through,
// dissolved or dug out, the solid pieces around the hole are traced: a piece that no longer reaches the
// bedrock, the world's edge or a big mass of rock (a cave wall, a castle curtain) breaks free and falls as
// one block. It crushes whatever it lands on, and shatters into rubble if it fell far. Planks are left
// out of it: they're held up by their own posts and ropes.

struct Body { std::vector<std::pair<int, int>> cells; float vy = 0, fall = 0; int dropped = 0; };
static std::vector<Body> bodies;
static int anchorCells() { return 3000 * world.scale * world.scale; } // a piece bigger than this is a wall, not a loose block

static long long cellKey(int x, int y) { return (long long)y * 1048576LL + x; }
static bool structural(int x, int y)
{
    if (!world.in(x, y)) return false;
    const Cell& c = world.get(x, y);
    return props(c.material).kind == Kind::Solid && !(c.flags & CF_LOOSE) && c.material != M::Platform && c.material != M::Bedrock;
}

// Trace the pieces touching the edge of a disturbed area; any that hang free start to fall.
static void checkSupport(int cx, int cy, int r)
{
    std::unordered_map<long long, int> part; // cell -> which traced piece it belongs to
    std::vector<bool> anchored;
    r += 1;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            if (std::abs(dx) != r && std::abs(dy) != r) continue; // the ring around the hole
            int sx = cx + dx, sy = cy + dy;
            if (!structural(sx, sy) || part.count(cellKey(sx, sy))) continue;
            int id = (int)anchored.size();
            bool held = false;
            std::vector<std::pair<int, int>> q{{sx, sy}};
            part[cellKey(sx, sy)] = id;
            for (size_t i = 0; i < q.size() && !held; i++)
            {
                if ((int)q.size() > anchorCells()) { held = true; break; }
                static const int D4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto& d : D4)
                {
                    int nx = q[i].first + d[0], ny = q[i].second + d[1];
                    if (!world.in(nx, ny) || world.get(nx, ny).material == M::Bedrock) { held = true; break; }
                    if (!structural(nx, ny)) continue;
                    auto f = part.find(cellKey(nx, ny));
                    if (f != part.end()) { if (f->second != id && anchored[f->second]) { held = true; break; } continue; }
                    part[cellKey(nx, ny)] = id;
                    q.push_back({nx, ny});
                }
            }
            anchored.push_back(held);
            if (!held)
            {
                Body b;
                b.cells = std::move(q);
                bodies.push_back(std::move(b));
            }
        }
}

// Drop a falling block one cell. False when something solid (or someone) is in the way.
static bool stepBody(Body& b)
{
    // pieces of it may have burnt or been blasted away while it fell
    b.cells.erase(std::remove_if(b.cells.begin(), b.cells.end(), [](const std::pair<int, int>& p) { return !structural(p.first, p.second); }), b.cells.end());
    if (b.cells.empty()) return false;
    std::unordered_set<long long> mine;
    for (auto& p : b.cells) mine.insert(cellKey(p.first, p.second));
    int bx0 = 1 << 30, bx1 = -1, by0 = 1 << 30, by1 = -1;
    for (auto& p : b.cells) { bx0 = std::min(bx0, p.first); bx1 = std::max(bx1, p.first); by0 = std::min(by0, p.second); by1 = std::max(by1, p.second); }
    float sc = (float)world.scale;
    std::vector<Mob*> near; // only foes around the block can be hit by it (they're in units, the block in cells)
    for (auto& m : G.mobs)
        if (m.alive && m.x < bx1 / sc + 2 && m.x + m.w > bx0 / sc - 1 && m.y < by1 / sc + 2 && m.y + m.h > by0 / sc) near.push_back(&m);
    bool blocked = false;
    for (auto& p : b.cells)
    {
        int x = p.first, y = p.second + 1;
        float ux = x / sc, uy = y / sc;
        if (mine.count(cellKey(x, y))) continue;
        if (!world.in(x, y)) return false;
        const Cell& t = world.get(x, y);
        Kind k = props(t.material).kind;
        if ((t.material != M::Empty && !(k == Kind::Liquid || k == Kind::Gas || k == Kind::Fire)) || t.material == M::Platform) return false;
        for (Mob* m : {&G.p.m}) // a falling block lands on you
            if (m->alive && ux >= m->x && ux < m->x + m->w && uy >= m->y && uy < m->y + m->h) blocked = true;
        for (Mob* m : near)
            if (m->alive && ux >= m->x && ux < m->x + m->w && uy >= m->y && uy < m->y + m->h)
            {
                if (b.vy > sc) damageMob(*m, b.vy / sc * 6 + b.cells.size() / (sc * sc) * 0.02f, EL_PHYS, 0, 1.5f, DMG_HIT);
                blocked = true;
            }
    }
    if (blocked)
    {
        Mob& pm = G.p.m;
        if (b.vy > sc && pm.alive) damageMob(pm, b.vy / sc * 4 + b.cells.size() / (sc * sc) * 0.01f, EL_PHYS, 0, 1.5f, DMG_HIT);
        b.dropped = 99; // whatever hit someone comes apart on them
        return false;
    }
    // shift every column of the block down one; whatever filled the gap beneath (air, water) moves up
    std::map<int, std::vector<int>> cols;
    for (auto& p : b.cells) cols[p.first].push_back(p.second);
    for (auto& kv : cols)
    {
        auto& ys = kv.second;
        std::sort(ys.begin(), ys.end());
        int x = kv.first;
        for (size_t i = 0; i < ys.size();)
        {
            size_t j = i;
            while (j + 1 < ys.size() && ys[j + 1] == ys[j] + 1) j++;
            int top = ys[i], bot = ys[j];
            Cell under = world.at(x, bot + 1);
            for (int y = bot; y >= top; y--) world.at(x, y + 1) = world.at(x, y);
            world.at(x, top) = under;
            i = j + 1;
        }
    }
    for (auto& p : b.cells) p.second++;
    b.dropped++;
    return true;
}

static void landBody(Body& b)
{
    float sc = (float)world.scale;
    if (b.dropped < 10 * sc || b.cells.empty()) return; // a short drop: it just settles where it stops
    int n = (int)b.cells.size();
    float sx = 0, sy = 0;
    for (auto& p : b.cells)
    {
        sx += p.first; sy += p.second;
        Cell& c = world.at(p.first, p.second);
        if (chance(3)) c.flags |= CF_LOOSE; // shattered into rubble that pours like gravel
        if (chance(12 * (int)(sc * sc))) spawnParticle(p.first / sc, p.second / sc, frange(-1, 1), frange(-1.5f, -0.3f), irange(20, 50), cellColor(c, p.first, p.second), 0.12f);
    }
    float cxu = sx / b.cells.size() / sc, cyu = sy / b.cells.size() / sc; // the block's middle, in units
    n = std::max(1, (int)(n / (sc * sc)));                                 // and its size in units
    for (int k = 0; k < std::min(40, n / 8 + 4); k++)
        spawnParticle(cxu + frange(-n * 0.05f, n * 0.05f), cyu, frange(-1.2f, 1.2f), frange(-0.8f, -0.1f), irange(30, 70), {120, 112, 100, 140}, -0.01f);
    playAt(SFX_SMASH, cxu, cyu, std::min(1.0f, 0.3f + n / 600.0f), 0.6f);
    G.shake = std::max(G.shake, (float)std::min(10, 2 + n / 150));
}

static void updateStructures()
{
    static int bodiesGen = -1;
    if (bodiesGen != world.gen) { bodies.clear(); bodiesGen = world.gen; } // a rebuilt world leaves blocks where they are
    std::vector<Disturbance> todo;
    todo.swap(world.disturbed);
    std::unordered_set<long long> done;
    int n = 0;
    for (auto& d : todo)
    {
        if (!done.insert(cellKey(d.x / 6, d.y / 6)).second) continue; // one trace per neighbourhood per frame
        if (n++ >= 12) { world.disturbed.push_back(d); continue; }
        checkSupport(d.x, d.y, d.r);
    }
    for (auto& b : bodies)
    {
        b.vy = std::min(b.vy + 0.15f * world.scale, 4.0f * world.scale); // in cells per frame
        b.fall += b.vy;
        bool moving = true;
        while (b.fall >= 1 && moving)
        {
            b.fall -= 1;
            moving = stepBody(b);
        }
        if (!moving) { landBody(b); b.cells.clear(); }
    }
    bodies.erase(std::remove_if(bodies.begin(), bodies.end(), [](const Body& b) { return b.cells.empty(); }), bodies.end());
}

// --selftest: a slab on two pillars stays up; blow one pillar away and it falls, and settles on the ground.
void collapseSelfTest()
{
    worldInit(120, 80);
    for (int y = 70; y < 80; y++) for (int x = 0; x < 120; x++) world.at(x, y).material = M::Bedrock;
    for (int y = 40; y < 70; y++) { world.at(30, y).material = M::Stone; world.at(80, y).material = M::Stone; } // pillars
    for (int x = 30; x <= 80; x++) for (int y = 36; y < 40; y++) world.at(x, y).material = M::Stone; // the slab
    updateStructures();
    for (int i = 0; i < 30; i++) updateStructures();
    if (world.at(55, 36).material != M::Stone) { std::printf("collapse: FAIL - a supported slab moved\n"); return; }
    for (int y = 40; y < 70; y++) world.at(30, y) = Cell{}; // one pillar gone: the other still holds it
    disturb(30, 50, 12);
    for (int i = 0; i < 30; i++) updateStructures();
    if (world.at(55, 36).material != M::Stone) { std::printf("collapse: FAIL - fell while still held\n"); return; }
    for (int y = 60; y < 70; y++) world.at(80, y) = Cell{}; // cut the second at its foot
    disturb(80, 64, 5);
    for (int i = 0; i < 60; i++) updateStructures();
    int below = 0;
    for (int y = 40; y < 70; y++) below += world.at(55, y).material == M::Stone;
    std::printf("collapse: %s\n", world.at(55, 36).material == M::Empty && below ? "ok - the slab came down" : "FAIL - the slab still hangs");
}

static void updateProjectiles()
{
    std::vector<Proj> cur;
    cur.swap(G.projs); // new projectiles (payloads) go into G.projs and step next frame
    for (auto& p : cur)
        if (p.alive) stepProj(p);
    for (auto& p : cur)
        if (p.alive) G.projs.push_back(std::move(p));
}

// ================================================================ environment & status

static void updateStatus(Mob& m)
{
    bool isP = &m == &G.p.m;
    if (m.iframes > 0) m.iframes--;
    if (m.hurtFlash > 0) m.hurtFlash--;
    if (m.hitT > 0) m.hitT--;
    if (m.chill > 0) m.chill--;
    if (m.shock > 0) m.shock--;
    if (m.envCd > 0) m.envCd--;
    if (isP && G.p.amulet == AM_YGGDRASIL && G.frame % 40 == 0 && m.alive) m.hp = std::min(m.maxHp, m.hp + 1);
    if (!isP && ENEMIES[m.type].gore == M::Blood)
        for (int k = 0; k < m.nWounds; k++) // stab wounds and stuck bolts drip
            if ((G.frame + k * 7 + m.id) % 11 == 0 && chance(2))
            {
                Vector2 w = rigWoundPos(m, k);
                spawnCellParticle(w.x, w.y, m.vx * 0.5f + frange(-0.2f, 0.2f), frange(-0.2f, 0.3f), M::Blood, 0);
            }

    int lava = 0, acid = 0, fire = 0, water = 0, miasma = 0, oil = 0, blood = 0;
    int x0 = (int)m.x, y0 = (int)m.y, x1 = (int)(m.x + m.w), y1 = (int)(m.y + m.h), sc = world.scale;
    for (int yy = y0 * sc; yy < y1 * sc; yy++)
        for (int xx = x0 * sc; xx < x1 * sc; xx++)
        {
            if (!world.in(xx, yy)) continue;
            const Cell& c = world.get(xx, yy);
            switch (c.material)
            {
            case M::Lava: lava++; break;
            case M::Acid: acid++; break;
            case M::Fire: fire++; break;
            case M::Water: water++; break;
            case M::Miasma: miasma++; break;
            case M::Oil: oil++; break;
            case M::Blood: blood++; break;
            default: break;
            }
            if (c.flags & CF_BURNING) fire++;
        }
    bool spikes = false;
    for (int xx = x0; xx < x1; xx++)
        if (world.matU(xx, y1) == M::Spikes || world.matU(xx, y1 - 1) == M::Spikes) spikes = true;

    bool fireImmune = isP ? (G.p.armour >= 0 && METALS[G.p.armour].el == EL_FIRE) : ENEMIES[m.type].resist[EL_FIRE] <= 0;
    // coatings: water soaks you and rinses the rest off; oil and blood cling until they wear off
    if (water) { m.wet = 600; m.oily = std::max(0, m.oily - 6); m.bloody = std::max(0, m.bloody - 10); }
    if (oil) m.oily = std::max(m.oily, 900);
    if (blood) m.bloody = std::max(m.bloody, 600);
    if (m.wet > 0) m.wet -= m.burn || fire ? 4 : 1; // heat dries you out
    if (m.oily > 0) m.oily -= m.burn ? 3 : 1;       // and burns the oil off
    if (m.bloody > 0) m.bloody--;
    m.wet = std::max(0, m.wet);
    m.oily = std::max(0, m.oily);
    if (lava) { damageMob(m, 0.6f, EL_FIRE, 0, 0, 0); m.wet = 0; if (!fireImmune) m.burn = std::max(m.burn, m.oily ? 360 : 180); }
    if (acid) damageMob(m, 0.35f, EL_POISON, 0, 0, 0);
    if (fire && !fireImmune && !m.wet) m.burn = std::max(m.burn, m.oily ? 300 : 120);
    if ((water || m.wet) && m.burn)
    {
        m.burn = 0;
        for (int i = 0; i < 6; i++) spawnParticle(m.x + frand() * m.w, m.y + frand() * m.h, frange(-0.3f, 0.3f), -0.7f, irange(20, 40), {210, 210, 220, 150}, -0.01f); // doused
    }
    if (G.frame % 9 == 0) // drips
    {
        float dx = m.x + frand() * m.w, dy = m.y + m.h * frange(0.3f, 1.0f);
        if (m.wet) spawnParticle(dx, dy, 0, 0.2f, 30, {120, 170, 235, 220}, 0.12f);
        else if (m.oily && G.frame % 18 == 0) spawnParticle(dx, dy, 0, 0.1f, 40, {40, 32, 24, 255}, 0.08f);
        if (m.bloody && G.frame % 27 == 0) spawnParticle(dx, dy, 0, 0.1f, 40, {150, 16, 22, 255}, 0.1f);
    }
    if (miasma && (isP || ENEMIES[m.type].resist[EL_POISON] > 0)) m.poison = std::max(m.poison, 60);
    if (spikes && m.envCd == 0 && !(m.type >= 0 && ENEMIES[m.type].noclip))
    {
        damageMob(m, 14, EL_PHYS, 0, -2.5f, DMG_HIT);
        m.envCd = 30;
    }
    if (m.burn > 0)
    {
        m.burn--;
        damageMob(m, m.oily ? 0.2f : 0.12f, EL_FIRE, 0, 0, 0); // oil burns hotter
        for (int k = 0; k < 2; k++) // embers, and smoke trailing off
            spawnParticle(m.x + frand() * m.w, m.y + frand() * m.h * 0.7f, frange(-0.3f, 0.3f), frange(-1.0f, -0.4f), irange(10, 22), {255, (unsigned char)irange(110, 230), 40, 255}, -0.02f);
        if (G.frame % 4 == 0)
            spawnParticle(m.cx() + frange(-2, 2), m.y - 2, frange(-0.2f, 0.2f), -0.5f, irange(30, 50), {60, 56, 56, 140}, -0.005f);
        if (chance(20)) ignite((int)(m.x + frand() * m.w), (int)(m.y + m.h));
    }
    if (m.poison > 0)
    {
        m.poison--;
        damageMob(m, 0.06f, EL_POISON, 0, 0, 0);
        if (G.frame % 8 == 0)
            spawnParticle(m.x + frand() * m.w, m.y + frand() * m.h, 0, -0.3f, 14, {120, 230, 90, 255}, 0);
    }
    if (m.chill > 0 && G.frame % 6 == 0)
        spawnParticle(m.x + frand() * m.w, m.y + frand() * m.h, 0, 0.2f, 14, {200, 230, 255, 255}, 0);
    if (m.shock > 0 && G.frame % 3 == 0)
        spawnParticle(m.x + frand() * m.w, m.y + frand() * m.h, frange(-1, 1), frange(-1, 1), 4, {250, 250, 150, 255}, 0);
}

// ================================================================ player

static bool isRubble(M m) { return m == M::Sand || m == M::WetSand || m == M::Gravel || m == M::Snow || m == M::Bone; }

// Each melee weapon has its own way of fighting, and chains three attacks: the third is a finisher.
// Swords slash back and forth (the third a wide, heavy cut), daggers stab (and slash on the third),
// spears poke straight out (the third a lunge), axes chop down from over the shoulder, and hammers
// are hauled overhead and slammed into the ground in front. `hitAt` is when it lands.
struct AttackDef { int style, len, hitAt; };
static AttackDef attackFor(int type, int combo)
{
    bool fin = combo == 2;
    switch (type)
    {
    case W_DAGGER: return fin ? AttackDef{ATK_SLASH, 9, 2} : AttackDef{ATK_STAB, 7, 2};
    case W_SPEAR: return fin ? AttackDef{ATK_THRUST, 16, 6} : AttackDef{ATK_THRUST, 12, 4};
    case W_AXE: return {ATK_CHOP, 22, 10};
    case W_MACE: return {ATK_SLAM, 26, 12};
    default: return fin ? AttackDef{ATK_SLASH, 15, 4} : AttackDef{ATK_SLASH, 11, 3};
    }
}

void startAttack(const Weapon& w)
{
    Player& P = G.p;
    P.combo = P.comboT > 0 ? (P.combo + 1) % 3 : 0;
    AttackDef d = attackFor(w.type, P.combo);
    P.atkStyle = d.style;
    P.atkLen = P.swingT = d.len;
    P.atkHitAt = d.hitAt;
    P.swingId++;
    P.swingDir = -P.swingDir;
    P.comboT = d.len + 20; // press again before this runs out to carry the chain on
    float pitch = w.type == W_DAGGER ? 1.3f : (w.type == W_SWORD ? 1.0f : 0.9f);
    if (w.type == W_PAN) playSfx(SFX_WHOOSH, 0.7f, P.combo == 2 ? 0.9f : 1.0f);
    else if (d.style == ATK_SLASH) playSfx(SFX_SWING, P.combo == 2 ? 0.85f : 0.65f, P.combo == 2 ? pitch * 0.85f : pitch);
    else if (d.style == ATK_STAB || d.style == ATK_THRUST) playSfx(SFX_THRUST, 0.7f, P.combo == 2 ? 0.85f : (w.type == W_DAGGER ? 1.25f : 1.0f));
}

// The blade is live for the whole swing: any foe it passes through is struck (once a swing). `impact` is the
// moment the blow lands - the ground cracks, crates split and the rock is cut then.
static void meleeStrike(const Weapon& w, bool impact)
{
    Player& P = G.p;
    const WeaponTypeDef& t = WTYPES[w.type];
    const MetalDef& md = METALS[w.metal];
    int fx = w.fx;
    Mob& pm = G.p.m;
    float ox = pm.cx(), oy = pm.y + pm.h * 0.45f;
    float aim = P.aim, f = (float)pm.facing;
    bool fin = P.combo == 2, heavy = P.atkStyle == ATK_CHOP || P.atkStyle == ATK_SLAM;
    float arc = (t.arc + (fin ? 20 : 0)) * DEG2RAD, R = t.range * (fin ? 1.15f : 1.0f), mult = fin ? 1.35f : 1.0f;
    if (P.atkStyle == ATK_STAB) arc = 20 * DEG2RAD;
    float sx = 0, sy = 0, sr = 0; // the slam's point of impact
    if (P.atkStyle == ATK_SLAM)
    {
        sx = ox + f * R * 0.75f;
        sy = oy;
        while (sy < oy + 24 && !isSolid((int)sx, (int)sy + 1)) sy++;
        sr = R * 0.55f + 4;
    }
    if (P.atkStyle == ATK_THRUST && fin) { R *= 1.35f; if (impact) pm.vx += f * 1.6f; } // the lunge
    // is (x, y), something `reach` across, inside the attack?
    auto inShape = [&](float x, float y, float reach) {
        float dx = x - ox, dy = y - oy;
        if (P.atkStyle == ATK_SLAM) return (x - sx) * (x - sx) + (y - sy) * (y - sy) <= (sr + reach) * (sr + reach);
        if (P.atkStyle == ATK_THRUST)
        {
            float along = dx * std::cos(aim) + dy * std::sin(aim), across = -dx * std::sin(aim) + dy * std::cos(aim);
            return along > -2 && along < R + reach && std::fabs(across) < 3 + reach;
        }
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > R + reach) return false;
        return std::fabs(std::remainder(std::atan2(dy, dx) - aim, 2 * PI)) <= arc || d <= reach + 2;
    };

    bool hitAny = false;
    for (auto& m : G.mobs)
    {
        if (!m.alive || m.hitSwing == P.swingId || !inShape(m.cx(), m.cy(), std::max(m.w, m.h) * 0.5f)) continue;
        m.hitSwing = P.swingId;
        hitAny = true;
        float dmg = weaponDamage(w) * frange(0.9f, 1.1f) * mult;
        if (fx & UF_RANDOM) dmg *= frange(0.4f, 2.2f);
        if ((fx & UF_EXECUTE) && m.hp < m.maxHp * 0.3f) dmg *= 3;
        float knock = (heavy || w.type == W_PAN ? 3.5f : 2.5f) * ((fx & UF_KNOCK) ? 2.5f : 1.0f) * (fin ? 1.4f : 1.0f);
        float kx = P.atkStyle == ATK_SLAM ? (m.cx() > sx ? 1 : -1) * knock * 0.6f : std::cos(aim) * knock;
        float ky = P.atkStyle == ATK_SLAM ? -3.5f : ((fx & UF_KNOCK) ? -3.0f : -1.2f);
        nextHit = w.type == W_SWORD ? HK_SLASH : w.type == W_AXE ? HK_CHOP : w.type == W_MACE ? HK_BLUNT : w.type == W_SPEAR ? HK_PIERCE : HK_NONE;
        nextAng = aim; nextK = knock;
        nextAt = {m.cx() - std::cos(aim) * m.w * 0.35f, clampf(oy, m.y + 2, m.y + m.h - 2)};
        damageMob(m, dmg, md.el, kx, ky, DMG_HIT);
        if (md.el == EL_SHOCK || (fx & UF_CHAIN) || (fin && P.amulet == AM_MJOLNIR)) chainShock(m, dmg * 0.5f);
        if (fx & UF_BURN) m.burn = std::max(m.burn, 240);
        if ((fx & UF_CHILL) || P.amulet == AM_SKADI) m.chill = std::max(m.chill, 200);
        if ((fx & UF_POISON) || P.amulet == AM_JORMUNGANDR) m.poison = std::max(m.poison, 400);
        if (fx & UF_BLEED)
        {
            m.bleedMark = 900;
            for (int k = 0; k < 6; k++) spawnCellParticle(m.cx(), m.cy(), std::cos(aim) * frange(0.5f, 2) + frange(-1, 1), frange(-2, 0), ENEMIES[m.type].gore, 0);
        }
        if (fx & UF_LEECH) pm.hp = std::min(pm.maxHp, pm.hp + dmg * 0.15f);
        bool armoured = m.type == E_KNIGHT || m.type == E_GOLEM || m.type == E_BLACKKNIGHT || m.type == E_SKELETON || m.type == E_GUARD || m.type == E_DRAUGR;
        if (w.type == W_PAN) playSfx(SFX_DING, 1.0f); // the pan rings like a bell
        else playSfx(armoured ? SFX_CLANG : SFX_HIT, heavy ? 1.0f : 0.8f, heavy ? 0.8f : 1.0f);
        for (int k = 0; k < (heavy || fin ? 12 : 6); k++) // sparks fly off along the blow
            spawnParticle(m.cx(), m.cy(), std::cos(aim) * frange(0.5f, 2.5f) + frange(-0.6f, 0.6f), std::sin(aim) * frange(0.5f, 2) + frange(-0.9f, 0.4f), irange(5, 12), armoured ? Color{255, 230, 160, 255} : Color{255, 250, 220, 255}, 0.05f);
    }
    if (hitAny && (heavy || fin)) { G.hitstop = std::max(G.hitstop, heavy ? 4 : 3); G.shake = std::max(G.shake, heavy ? 3.0f : 2.0f); }
    if (!impact) return;

    for (auto& it : G.inter) // crates and barrels in the way
    {
        if (it.type != IT_CRATE || it.used) continue;
        if (!inShape(it.x + it.w * 0.5f, it.y - it.h * 0.5f, std::max(it.w, it.h) * 0.5f)) continue;
        damageCrate(it, heavy ? 2 : 1, (P.atkStyle == ATK_SLAM ? (it.x > sx ? 1 : -1) : std::cos(aim)) * 2);
        if (heavy) G.hitstop = std::max(G.hitstop, 2);
    }
    for (auto& c : G.corpses) // the dead: knocked about
    {
        Vector2 cc = corpseCentre(c);
        if (!inShape(cc.x, cc.y, 6)) continue;
        float k = (heavy ? 2.4f : 1.4f) * (fin ? 1.3f : 1.0f);
        corpseKick(c, {ox + std::cos(aim) * R * 0.7f, oy + std::sin(aim) * R * 0.7f}, {std::cos(aim) * k, std::min(0.0f, std::sin(aim)) * k - k * 0.5f});
    }
    for (auto& it : G.inter) // loose crates and barrels: knocked flying
    {
        if (it.type != IT_PROP || it.used) continue;
        Vector2 hb = bodyHalf(it);
        if (!inShape(it.x, it.y - hb.y, std::max(hb.x, hb.y))) continue;
        float k = (heavy ? 2.6f : 1.6f) * (fin ? 1.4f : 1.0f) * ((fx & UF_KNOCK) ? 1.8f : 1.0f);
        float kx = P.atkStyle == ATK_SLAM ? (it.x > sx ? 1 : -1) * k * 0.6f : std::cos(aim) * k;
        hitProp(it, heavy || fin ? 2 : 1, kx, P.atkStyle == ATK_SLAM ? -k : std::sin(aim) * k * 0.5f - k * 0.45f);
        if (heavy) G.hitstop = std::max(G.hitstop, 2);
    }
    for (auto& it : G.inter) // lanterns: knocked off their chains
    {
        if (it.type != IT_LANTERN || it.used) continue;
        Vector2 lp = lanternPos(it);
        if (inShape(lp.x, lp.y + 4, 5)) hitLantern(it, std::cos(aim) * (heavy ? 2.2f : 1.5f));
    }

    if (P.atkStyle == ATK_SLAM) // the ground takes it: a crack of dust and grit both ways
    {
        playAt(SFX_SLAM, sx, sy, 1.0f);
        G.shake = std::max(G.shake, 5.0f);
        for (int k = 0; k < 18; k++)
        {
            float dir = k % 2 ? 1.0f : -1.0f;
            spawnParticle(sx + dir * frange(0, 4), sy, dir * frange(0.6f, 2.4f), frange(-1.0f, -0.1f), irange(18, 40), {150, 140, 124, 170}, -0.01f);
        }
        if (world.inU((int)sx, (int)sy + 1) && isSolid((int)sx, (int)sy + 1))
            for (int k = 0; k < 6; k++)
            {
                int gx = ((int)sx + irange(-3, 3)) * world.scale, gy = ((int)sy + 1) * world.scale;
                spawnParticle(sx + frange(-3, 3), sy, frange(-1.5f, 1.5f), frange(-2.8f, -1.2f), irange(25, 45), cellColor(world.get(gx, gy), gx, gy), 0.15f);
            }
    }
    if (P.atkStyle == ATK_THRUST)
        spawnParticle(ox + std::cos(aim) * R, oy + std::sin(aim) * R, std::cos(aim) * 0.6f, std::sin(aim) * 0.6f, 6, {255, 250, 220, 255}, 0);

    int sc = world.scale, r = ((int)std::max(R, sr + (sx ? std::fabs(sx - ox) : 0)) + 2) * sc;
    for (int dy = -r; dy <= r; dy++) // every cell in reach (the shape is tested in units)
        for (int dx = -r; dx <= r; dx++)
        {
            int x = (int)(ox * sc) + dx, y = (int)(oy * sc) + dy;
            float ux = (x + 0.5f) / sc, uy = (y + 0.5f) / sc;
            if (!world.in(x, y) || (dx * dx + dy * dy < 4 * sc * sc && P.atkStyle != ATK_SLAM) || !inShape(ux, uy, 0)) continue;
            if (world.get(x, y).material == M::Empty) continue;
            Cell& c = world.at(x, y);
            M m = c.material;
            if (m == M::Empty) continue;
            const MaterialProps& p = props(m);
            if (m == M::Keg) { ignite((int)ux, (int)uy); continue; }
            if ((md.el == EL_FIRE || (fx & UF_BURN)) && p.flammable && chance(6 * sc * sc)) ignite((int)ux, (int)uy);
            if ((md.el == EL_ICE || (fx & UF_CHILL)) && m == M::Water && chance(3)) { setCellC(x, y, M::Ice); continue; }
            if (p.kind == Kind::Liquid) // a blade through the water flings it the way you swung
            {
                if (chance(3) && world.get(x, y - 1).material == M::Empty)
                {
                    world.debris.push_back({x + 0.5f, y + 0.5f, std::cos(aim) * frange(1.0f, 2.5f) * sc, (std::sin(aim) * frange(0.5f, 1.5f) - frange(0.8f, 1.6f)) * sc, c});
                    c = Cell{};
                }
                continue;
            }
            // any weapon can shovel loose rubble aside, so a cave-in never traps you
            if (isRubble(m) && !(c.flags & CF_LOOSE))
            {
                if (chance(2))
                {
                    world.debris.push_back({x + 0.5f, y + 0.5f, std::cos(aim) * frange(0.5f, 1.5f), -frange(0.5f, 1.5f), c});
                    c = Cell{};
                }
                continue;
            }
            if ((fx & UF_MINER) && p.kind == Kind::Solid && p.hardness <= md.mine + (P.amulet == AM_BROKKR) && !p.ore && frand() < t.mineChance + 0.4f)
            {
                c = Cell{}; // delving weapons cut the rock itself
                disturb(x, y, 1);
                continue;
            }
            // otherwise blades only chip ore loose; the rock itself stays put
            if (!p.ore || (c.flags & CF_LOOSE) || p.hardness > md.mine + (P.amulet == AM_BROKKR) || frand() > std::max(t.mineChance, 0.15f)) continue;
            c.flags |= CF_LOOSE;
            if (chance(3 * sc * sc)) spawnParticle(ux, uy, frange(-1, 1), frange(-1.5f, 0), 8, {255, 240, 180, 255}, 0.1f);
        }
}

static void collectOre()
{
    Mob& m = G.p.m;
    int sc = world.scale;
    for (int yy = ((int)m.y - 2) * sc; yy < ((int)(m.y + m.h) + 2) * sc; yy++)
        for (int xx = ((int)m.x - 2) * sc; xx < ((int)(m.x + m.w) + 2) * sc; xx++)
        {
            if (!world.in(xx, yy)) continue;
            const Cell& g = world.get(xx, yy);
            if (!(g.flags & CF_LOOSE) || !props(g.material).ore) continue;
            Cell& c = world.at(xx, yy);
            int ore = props(c.material).ore;
            if ((xx + yy) % (sc * sc) && sc > 1) { c = Cell{}; continue; } // a unit's worth of grains makes one nugget
            G.p.res[ore - 1]++;
            G.p.pending[ore - 1]++;
            c = Cell{};
            playSfx(SFX_ORE, 0.35f, 1 + (ore - 1) * 0.05f);
        }
    if (G.frame % 25 == 0)
        for (int r = 0; r < RES_COUNT; r++)
            if (G.p.pending[r])
            {
                addText(m.cx(), m.y - 6, "+" + std::to_string(G.p.pending[r]) + " " + RES_NAMES[r], RES_COLORS[r]);
                G.p.pending[r] = 0;
            }
}

static void updatePlayer()
{
    Player& P = G.p;
    Mob& m = P.m;
    if (!m.alive) return;
    updateStatus(m);
    if (m.hp <= 0) return;

    Vector2 mw = mouseWorld();
    float cx = m.cx(), cy = m.y + m.h * 0.45f;
    P.aim = std::atan2(mw.y - cy, mw.x - cx);
    m.facing = std::cos(P.aim) >= 0 ? 1 : -1;

    bool L = IsKeyDown(KEY_A), R = IsKeyDown(KEY_D), U = IsKeyDown(KEY_W);
    bool jumpPressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W);
    bool sandboxPaint = G.sandbox && IsKeyDown(KEY_LEFT_CONTROL);

    int dir = (R ? 1 : 0) - (L ? 1 : 0);

    // posture: hold S to crouch (16 tall); hold S + Ctrl, or push into a gap too low to crouch through,
    // to drop prone and crawl (9 tall). You only rise again where there's headroom.
    {
        static const int heights[3] = {21, 16, 9};
        int posture = P.prone ? 2 : (P.crouch ? 1 : 0);
        if (IsKeyPressed(KEY_S) && m.onGround && onPlatform(m) && !boxSolid(m.x, m.y + m.h + 0.05f, m.w, 1) && posture == 0)
            m.dropT = 14; // drop through the planks
        bool down = IsKeyDown(KEY_S) && m.onGround && P.hook != 2 && !m.inLiquid && P.rollT == 0 && m.dropT == 0;
        int want = down ? ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C)) ? 2 : 1) : 0;
        if (posture >= 1 && down && dir != 0 && boxSolid(m.x + dir, m.y, m.w, m.h) && !boxSolid(m.x + dir, m.y + m.h - 9, m.w, 9))
            want = 2; // squeeze into a crawlspace
        int target = want;
        while (target < posture) // rising needs clear space overhead
        {
            int dh = heights[target] - m.h;
            if (!boxSolid(m.x, m.y - dh, m.w, dh)) break;
            target++;
        }
        if (target != posture)
        {
            m.y += m.h - heights[target];
            m.h = heights[target];
            P.crouch = target >= 1;
            P.prone = target == 2;
        }
    }
    if (P.crouch) jumpPressed = false;

    // ropes: W or S beside one to grab on, W / S to climb, Space or A / D to let go
    const Interact* rope = nullptr;
    for (auto& it : G.inter)
        if (it.type == IT_ROPE && std::fabs(m.cx() - it.x) < 5 && m.y + m.h > it.y + 2 && m.y < it.data) rope = &it;
    if (!rope || P.hook == 2) P.climb = false;
    else if (!P.climb && (U || IsKeyDown(KEY_S)) && !P.crouch && P.rollT == 0 && !(m.onGround && IsKeyDown(KEY_S)))
        P.climb = true;
    else if (P.climb && (IsKeyPressed(KEY_SPACE) || (dir != 0 && !U && !IsKeyDown(KEY_S))))
    {
        P.climb = false;
        if (IsKeyPressed(KEY_SPACE)) m.vy = -2.6f;
        m.vx = dir * 1.2f;
        jumpPressed = false;
    }
    if (P.climb) { jumpPressed = false; P.jumpBuf = 0; }

    // ledge pull-up in progress: rise level with the lip, then roll over onto it
    if (P.mantleT > 0)
    {
        const int MT = 14;
        float t = 1 - --P.mantleT / (float)MT, up = clampf(t / 0.6f, 0, 1), over = clampf((t - 0.6f) / 0.4f, 0, 1);
        m.x = P.mx0 + (P.mx1 - P.mx0) * over;
        m.y = P.my0 + (P.my1 - P.my0) * (1 - (1 - up) * (1 - up));
        m.vx = m.vy = 0;
        m.onGround = P.mantleT == 0;
        P.onWall = P.mantleT > 0 ? P.mantleDir : 0;
        P.runPhase += 0.2f;
        return;
    }

    float speed = 0.85f * (m.chill > 0 ? 0.5f : 1.0f) * (P.prone ? 0.3f : (P.crouch ? 0.45f : 1.0f));
    float accel = m.onGround ? 0.28f : 0.14f;
    if (P.hook == 2) accel = 0.12f;
    if (dir != 0 || m.onGround) m.vx += clampf(dir * speed - m.vx, -accel, accel);

    if (m.inLiquid)
    {
        m.vy = std::min(m.vy + 0.08f, 1.2f);
        m.vx *= 0.9f;
        if (U || IsKeyDown(KEY_SPACE)) m.vy = std::max(m.vy - 0.35f, -1.6f);
    }
    else if (!P.climb)
        m.vy = std::min(m.vy + 0.24f, 5.5f);

    if (m.onGround) P.coyote = 6; else if (P.coyote > 0) P.coyote--;
    if (jumpPressed) P.jumpBuf = 6; else if (P.jumpBuf > 0) P.jumpBuf--;
    if (P.crouch) P.jumpBuf = 0;

    // wall scaling: hold toward a wall to cling, W to climb (costs stamina), Space to kick off
    bool wl = boxSolid(m.x - 1, m.y + 3, 1, m.h - 9), wr = boxSolid(m.x + m.w, m.y + 3, 1, m.h - 9);
    P.onWall = 0;
    if (!m.onGround && !m.inLiquid && P.hook != 2 && !P.climb)
    {
        if (wl && L) P.onWall = -1;
        else if (wr && R) P.onWall = 1;
    }
    // ledge grab: push into a wall whose lip is within reach and you haul yourself up onto it
    if (dir != 0 && !m.onGround && !m.inLiquid && !P.climb && P.hook != 2 && P.rollT == 0 && !P.crouch && (dir < 0 ? wl : wr))
    {
        float nx = dir > 0 ? m.x + m.w : m.x - m.w; // where you'll stand
        for (int ly = (int)(m.y + m.h) - 4; ly >= (int)m.y - 8; ly--) // the lowest lip first
        {
            float ty = (float)(ly - m.h);
            if (!boxSolid(nx, (float)ly, m.w, 1) || boxSolid(std::min(m.x, nx), ty, m.w * 2, m.h)) continue;
            if (ty < m.y && boxSolid(m.x, ty, m.w, (int)std::ceil(m.y - ty))) break; // no headroom to rise
            P.mantleT = 14;
            P.mantleDir = dir;
            P.mx0 = m.x; P.my0 = m.y; P.mx1 = nx; P.my1 = ty;
            P.hook = 0;
            playSfx(SFX_JUMP, 0.35f, 0.8f);
            return;
        }
    }
    if (P.jumpBuf > 0 && P.coyote > 0)
    {
        m.vy = -3.7f;
        P.jumpBuf = 0;
        P.coyote = 0;
        P.squash = 1.3f;
        playSfx(SFX_JUMP, 0.5f);
    }
    else if (P.onWall)
    {
        if (IsKeyPressed(KEY_SPACE) && P.stamina >= 8)
        {
            m.vy = -3.4f;
            m.vx = -P.onWall * 2.0f;
            P.stamina -= 12;
            P.squash = 1.3f;
            playSfx(SFX_JUMP, 0.5f, 1.15f);
            P.jumpBuf = 0;
        }
        else if (U && P.stamina > 0)
        {
            m.vy = -0.85f;
            P.stamina -= 0.35f;
            if (G.frame % 6 == 0) spawnParticle(P.onWall > 0 ? m.x + m.w : m.x, m.y + m.h, 0, 0.3f, 10, {120, 110, 100, 255}, 0.1f);
        }
        else
            m.vy = std::min(m.vy, 0.4f);
    }
    if (m.onGround) P.stamina = std::min(100.0f, P.stamina + 2.0f);

    // dodge roll (Shift): quick burst with invulnerability
    if (IsKeyPressed(KEY_LEFT_SHIFT) && P.rollT == 0 && P.rollCd == 0 && P.stamina >= 15 && !m.inLiquid && !P.crouch && !P.climb)
    {
        P.rollT = 20;
        P.rollDir = dir ? dir : m.facing;
        P.stamina -= 15;
        P.hook = 0;
        playSfx(SFX_ROLL, 0.6f);
    }
    if (P.rollT > 0)
    {
        P.rollT--;
        m.vx = P.rollDir * 1.9f;
        m.iframes = std::max(m.iframes, 2);
        m.facing = P.rollDir;
        if (P.rollT == 0) P.rollCd = 12;
        if (m.onGround && G.frame % 2 == 0)
            spawnParticle(m.cx() - P.rollDir * 2, m.y + m.h - 1, -P.rollDir * frange(0.2f, 0.8f), frange(-0.5f, -0.1f), irange(8, 14), {150, 140, 120, 200}, 0.02f);
    }
    else if (P.rollCd > 0)
        P.rollCd--;

    // Baldr's Offering: the wisp lags after you, bobbing just above your shoulder
    if (P.hasWisp)
    {
        float tx = m.cx() + std::sin(G.frame * 0.017f) * 12, ty = m.y - 8 + std::sin(G.frame * 0.05f) * 3;
        if (std::hypot(tx - P.wx, ty - P.wy) > 200) { P.wx = tx; P.wy = ty; } // new run or a long fall: catch up at once
        P.wx += (tx - P.wx) * 0.06f;
        P.wy += (ty - P.wy) * 0.06f;
        if (G.frame % 6 == 0) spawnParticle(P.wx + frange(-1.5f, 1.5f), P.wy + 1, frange(-0.15f, 0.15f), frange(0.05f, 0.25f), irange(14, 24), {255, 230, 150, 255}, 0);
    }

    // grappling hook (hold right mouse)
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !P.hasHook && !sandboxPaint && G.frame % 1 == 0)
        message("You don't own a grappling hook yet - the village Outfitter sells one.");
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && P.hook == 0 && !sandboxPaint && P.hasHook)
    {
        P.hook = 1;
        P.hx = cx; P.hy = cy;
        P.hvx = std::cos(P.aim) * 8; P.hvy = std::sin(P.aim) * 8;
        P.hookTravel = 0;
        playSfx(SFX_HOOK, 0.4f, 0.8f);
    }
    if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) P.hook = 0;
    if (P.hook == 1)
    {
        for (int i = 0; i < 8 && P.hook == 1; i++)
        {
            P.hx += P.hvx / 8; P.hy += P.hvy / 8;
            P.hookTravel++;
            if (isSolid((int)P.hx, (int)P.hy))
            {
                P.hook = 2;
                P.rope = std::hypot(P.hx - cx, P.hy - cy);
                playSfx(SFX_HOOK, 0.6f, 1.3f);
            }
        }
        if (P.hook == 1 && P.hookTravel > 150) P.hook = 0;
    }
    if (P.hook == 2)
    {
        if (!isSolid((int)P.hx, (int)P.hy)) P.hook = 0;
        else
        {
            if (U) P.rope = std::max(6.0f, P.rope - 1.8f);
            if (IsKeyDown(KEY_S)) P.rope = std::min(160.0f, P.rope + 1.8f);
            if (IsKeyPressed(KEY_SPACE)) { P.hook = 0; m.vy -= 2.6f; }
        }
    }

    if (P.climb)
    {
        bool down = IsKeyDown(KEY_S);
        m.vx = 0;
        m.vy = U ? -1.1f : (down ? 1.5f : 0.0f);
        moveBy(m, clampf(rope->x - m.cx(), -0.6f, 0.6f), 0, false); // hug the rope
        if (U && m.y + m.h - 1.1f <= rope->y + 3 && !boxSolid(m.x, rope->y - (float)m.h, m.w, m.h))
        {
            m.y = rope->y - (float)m.h; // hauled yourself out onto the planks
            m.vy = 0;
            P.climb = false;
        }
        if (down && m.onGround) P.climb = false;
        if (std::fabs(m.vy) > 0.1f) P.runPhase += 0.12f;
    }

    float vyBefore = m.vy;
    moveMob(m);
    if (G.roamX1 > 0 && (m.x < G.roamX0 || m.x + m.w > G.roamX1)) // Hearthwick's edges: the fields and the end of the pier
    {
        m.x = clampf(m.x, G.roamX0, G.roamX1 - m.w);
        m.vx = 0;
    }
    if (m.onGround && !P.wasGround && vyBefore > 1.8f)
    {
        P.squash = 0.7f;
        playSfx(SFX_LAND, std::min(1.0f, vyBefore / 5));
        for (int k = 0; k < 8; k++)
            spawnParticle(m.cx() + frange(-3, 3), m.y + m.h - 1, frange(-1, 1), frange(-0.6f, -0.1f), irange(8, 16), {150, 140, 120, 200}, 0.02f);
    }
    P.wasGround = m.onGround;
    P.squash += (1 - P.squash) * 0.2f;
    if (m.onGround)
    {
        float before = P.runPhase;
        P.runPhase += std::fabs(m.vx) * 0.32f;
        if ((int)(before / PI) != (int)(P.runPhase / PI) && std::fabs(m.vx) > 0.3f) playSfx(SFX_STEP, 0.35f);
    }
    static bool wasWet = false;
    if (m.inLiquid && !wasWet) playSfx(SFX_SPLASH, 0.6f);
    wasWet = m.inLiquid;
    G.underwater = isLiquidAt((int)m.cx(), (int)m.y + 1);
    if (G.underwater && P.amulet != AM_NJORD) // head under: about 14 seconds of air
    {
        P.breath = std::max(0.0f, P.breath - 0.12f);
        if (G.frame % 9 == 0) spawnParticle(m.cx() + m.facing * 2, m.y + 2, frange(-0.2f, 0.2f), -0.7f, 40, {200, 230, 255, 200}, -0.01f);
        if (P.breath <= 0 && G.frame % 30 == 0) damageMob(m, 6 + 2.0f * G.stage, EL_PHYS, 0, 0, 0); // drowning
    }
    else P.breath = std::min(100.0f, P.breath + 1.5f);

    if (P.hook == 2)
    {
        float pcx = m.cx(), pcy = m.cy();
        float dx = pcx - P.hx, dy = pcy - P.hy;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d > P.rope && d > 0.01f)
        {
            float nx = dx / d, ny = dy / d;
            moveBy(m, P.hx + nx * P.rope - pcx, P.hy + ny * P.rope - pcy, false);
            float vr = m.vx * nx + m.vy * ny;
            if (vr > 0) { m.vx -= nx * vr; m.vy -= ny * vr; }
        }
    }
    if (m.y > world.hU() + 20) m.hp = 0;

    // hotbar
    int n = (int)P.hotbar.size(), wasSel = P.sel;
    for (int k = 0; k < 6; k++)
        if (IsKeyPressed(KEY_ONE + k) && k < n) P.sel = k;
    float wh = GetMouseWheelMove();
    if (wh != 0 && n > 0) P.sel = ((P.sel - (wh > 0 ? 1 : -1)) % n + n) % n;
    if (P.sel >= n) P.sel = std::max(0, n - 1);
    if (P.sel != wasSel && n > 0 && P.hotbar[P.sel].type == W_SWORD) playSfx(SFX_DRAW, 0.7f); // the blade leaves its scabbard

    for (auto& w : P.hotbar)
        if (w.type == W_STAFF)
        {
            if (w.staff.cd > 0) w.staff.cd--;
            w.staff.mana = std::min(w.staff.manaMax, w.staff.mana + w.staff.regen);
        }

    if (P.attackCd > 0) P.attackCd--;
    if (P.combatT > 0) P.combatT--;
    if (P.comboT > 0) P.comboT--;
    if (P.swingT > 0)
    {
        P.swingT--;
        int at = P.atkLen - P.swingT;
        bool melee = n > 0 && isMelee(P.hotbar[P.sel].type);
        if (melee && at == P.atkHitAt - 4 && (P.atkStyle == ATK_CHOP || P.atkStyle == ATK_SLAM))
            playSfx(SFX_HEAVY, 0.8f); // the downswing
        if (melee) meleeStrike(P.hotbar[P.sel], at == P.atkHitAt);
    }
    if (P.recoil > 0) P.recoil--;
    if (n > 0 && P.rollT == 0 && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !sandboxPaint)
    {
        Weapon& w = P.hotbar[P.sel];
        float hx = cx + std::cos(P.aim) * 6, hy = cy + std::sin(P.aim) * 6;
        P.combatT = 150;
        if (w.type == W_STAFF)
            castStaff(w.staff, hx, hy, P.aim, true);
        else if (P.attackCd == 0)
        {
            if (w.type == W_CROSSBOW)
            {
                int bolts = (w.fx & UF_MULTISHOT) ? 3 : 1;
                for (int k = 0; k < bolts; k++)
                {
                    float a = P.aim + (k - (bolts - 1) * 0.5f) * 0.12f;
                    Proj b;
                    b.kind = PK_BOLT;
                    b.x = hx; b.y = hy;
                    b.vx = std::cos(a) * 9; b.vy = std::sin(a) * 9;
                    b.dmg = weaponDamage(w);
                    b.el = (w.fx & UF_BURN) ? EL_FIRE : ((w.fx & UF_CHILL) ? EL_ICE : ((w.fx & UF_POISON) ? EL_POISON : METALS[w.metal].el));
                    if (w.fx & UF_CHAIN) b.el = EL_SHOCK;
                    if (w.fx & UF_EXPLOSIVE) { b.blast = 5; b.power = 3; }
                    b.grav = 0.05f;
                    b.life = 120;
                    b.col = w.glow.a ? w.glow : METALS[w.metal].color;
                    G.projs.push_back(b);
                }
                P.attackCd = weaponCooldown(w);
                P.recoil = 6;
                playSfx(SFX_XBOW, 0.85f);
            }
            else
            {
                startAttack(w);
                P.attackCd = std::max(weaponCooldown(w), P.atkLen);
            }
        }
    }

    if (IsKeyPressed(KEY_Q) && P.potions > 0 && m.hp < m.maxHp)
    {
        P.potions--;
        m.hp = std::min(m.maxHp, m.hp + 50);
        m.burn = m.poison = 0;
        addText(m.cx(), m.y - 4, "+50", {120, 255, 120, 255});
        playSfx(SFX_POTION, 0.7f);
    }
    if (IsKeyPressed(KEY_G) && n > 1)
    {
        addPickupWeapon(cx, m.y, P.hotbar[P.sel]);
        P.hotbar.erase(P.hotbar.begin() + P.sel);
        P.sel = std::min(P.sel, (int)P.hotbar.size() - 1);
    }
    collectOre();
    m.anim += std::fabs(m.vx) * 0.15f;
}

// ================================================================ enemies

static void touchDamage(Mob& m, float scale)
{
    Mob& pm = G.p.m;
    if (pm.alive && overlap(m, pm))
        damageMob(pm, m.dmg * scale, ENEMIES[m.type].el, (pm.cx() > m.cx() ? 1 : -1) * 2.0f, -1.5f, DMG_HIT);
}

// Foes don't hurt by touch. Each melee attack winds up where you can see it (they plant their feet, lean back,
// a glint), strikes for a few frames, then leaves them open while they recover.
enum AttackKind { AK_SWING, AK_LUNGE, AK_POUNCE, AK_DIVE };
static int attackKind(int type)
{
    switch (type)
    {
    case E_WOLF: case E_KELPIE: return AK_LUNGE; // a leap and a bite
    case E_SLIME: return AK_POUNCE;
    case E_BAT: return AK_DIVE;
    default: return AK_SWING; // blades, clubs, claws: a strike in front of them
    }
}
static int windupFor(int type)
{
    switch (type)
    {
    case E_GOBLIN: case E_REDCAP: return 10;
    case E_TROLL: case E_GOLEM: return 24;
    case E_BLACKKNIGHT: return 18;
    case E_SLIME: return 12;
    default: return 15;
    }
}
// Close enough to start one?
static bool attackInReach(const Mob& m, float dx, float dy)
{
    const EnemyDef& d = ENEMIES[m.type];
    switch (attackKind(m.type))
    {
    case AK_LUNGE: return std::fabs(dx) < 52 && std::fabs(dy) < (m.inLiquid ? 52 : 18);
    case AK_POUNCE: return std::fabs(dx) < 70 && std::fabs(dy) < 30;
    case AK_DIVE: return std::fabs(dx) < 60 && std::fabs(dy) < 50;
    default: return std::fabs(dx) < d.range + (m.w + G.p.m.w) * 0.5f + 2 && std::fabs(dy) < m.h;
    }
}
static void beginAttack(Mob& m)
{
    m.atkPhase = 1;
    m.atkT = windupFor(m.type);
    m.atkDone = false;
    m.cd = ENEMIES[m.type].cooldown + irand(15);
}
// Runs an attack under way; while it does, the attacker isn't steering.
static void runAttack(Mob& m, float dx, float dy)
{
    const EnemyDef& d = ENEMIES[m.type];
    Mob& pm = G.p.m;
    int kind = attackKind(m.type);
    bool flying = d.ai == AI_FLY;
    m.atkT--;
    if (m.atkPhase == 1) // the wind-up: plant, face you, telegraph
    {
        m.facing = dx > 0 ? 1 : -1;
        m.vx *= 0.7f;
        if (flying) m.vy *= 0.7f;
        if (m.atkT % 5 == 0) spawnParticle(m.cx() + m.facing * m.w * 0.45f, m.y + 2, 0, -0.25f, 7, {255, 244, 210, 255}, 0); // a glint
        if (m.atkT > 0) return;
        m.atkPhase = 2;
        float l = std::sqrt(dx * dx + dy * dy) + 0.01f;
        switch (kind)
        {
        case AK_SWING:
            m.atkT = 6;
            m.vx = m.facing * 1.3f; // a step into the blow
            playAt(SFX_SWING, m.cx(), m.cy(), 0.45f, m.w > 16 ? 0.7f : 1.15f);
            break;
        case AK_LUNGE:
            m.atkT = 30;
            if (m.inLiquid) { m.vx = dx / l * 4.2f; m.vy = dy / l * 4.2f; }
            else { m.vx = m.facing * clampf(std::fabs(dx) / 11.0f, 2.2f, 4.2f); m.vy = -2.6f; }
            playAt(m.type == E_WOLF ? SFX_BARK : SFX_WHOOSH, m.cx(), m.cy(), 0.5f, 1.2f);
            break;
        case AK_POUNCE:
            m.atkT = 45;
            m.vx = m.facing * clampf(std::fabs(dx) / 18.0f, 1.2f, 2.6f);
            m.vy = -3.8f;
            break;
        case AK_DIVE:
            m.atkT = 32;
            m.vx = dx / l * 3.4f;
            m.vy = dy / l * 3.4f;
            playAt(SFX_WHOOSH, m.cx(), m.cy(), 0.35f, 1.6f);
            break;
        }
        return;
    }
    if (m.atkPhase == 2) // live
    {
        Rectangle hb = {m.x - 2, m.y - 2, (float)m.w + 4, (float)m.h + 4}; // jaws, a slimy body: whatever it leads with
        if (kind == AK_SWING)
        {
            float reach = d.range + 3;
            hb = {m.facing > 0 ? m.x + m.w * 0.5f : m.x - reach, m.y - 2, m.w * 0.5f + reach, (float)m.h + 2};
            if (m.atkT == 5) // the arc of the blow
                for (int k = 0; k < 6; k++)
                {
                    float a = -1.1f + k * 0.4f;
                    spawnParticle(m.cx() + m.facing * std::cos(a) * reach, m.cy() + std::sin(a) * reach * 0.8f, m.facing * 0.4f, 0, 5, {255, 250, 230, 220}, 0);
                }
        }
        if (kind == AK_DIVE) { float l = std::sqrt(dx * dx + dy * dy) + 0.01f; m.vx += dx / l * 0.08f; m.vy += dy / l * 0.08f; } // it corrects a little
        if (!m.atkDone && pm.alive && CheckCollisionRecs(hb, {pm.x, pm.y, (float)pm.w, (float)pm.h}))
        {
            m.atkDone = true;
            damageMob(pm, m.dmg, d.el, m.facing * (kind == AK_SWING ? 2.2f : 2.8f), -1.6f, DMG_HIT);
            if (kind == AK_DIVE || kind == AK_LUNGE) m.vx *= -0.4f; // it bounces off you
        }
        bool landed = (kind == AK_LUNGE || kind == AK_POUNCE) && m.onGround && m.atkT < 24 && !m.inLiquid;
        if (m.atkT <= 0 || landed || (kind == AK_DIVE && m.atkDone))
        {
            m.atkPhase = 3;
            m.atkT = kind == AK_SWING ? (m.w > 16 ? 22 : 14) : 18; // open to a counter
        }
        return;
    }
    m.vx *= m.onGround ? 0.75f : 0.95f; // recovering
    if (flying) { m.vx += (-m.facing * 1.0f - m.vx) * 0.1f; m.vy += (-1.0f - m.vy) * 0.1f; } // flap back up out of reach
    if (m.atkT <= 0) m.atkPhase = 0;
}

static bool findFloorNear(float x, float y, float& ox, float& oy, int w, int h)
{
    for (int tries = 0; tries < 30; tries++)
    {
        int tx = (int)x + irange(-80, 80), ty = (int)y + irange(-40, 20);
        if (!world.inU(tx, ty) || isSolid(tx, ty)) continue;
        int fy = ty;
        while (fy < world.hU() - 1 && !isSolid(tx, fy + 1) && fy - ty < 60) fy++;
        if (!isSolid(tx, fy + 1)) continue;
        if (boxSolid(tx - w / 2.0f, fy + 1 - h, w, h)) continue;
        ox = (float)tx; oy = (float)(fy + 1);
        return true;
    }
    return false;
}

// Ground foes heading `dir`: hop anything they can clear, leap gaps when the target isn't below them,
// otherwise drop down after it. Wait at the edge if there's nowhere to land.
static void traverse(Mob& m, int dir, float targetDy)
{
    if (!m.onGround) return;
    float fx = dir > 0 ? m.x + m.w : m.x - 3;
    if (boxSolid(fx, m.y, 3, m.h - 4)) // something ahead taller than a step
    {
        for (int h = 4; h <= 38; h += 2)
            if (!boxSolid(fx, m.y - h, 3, m.h) && !boxSolid(m.x, m.y - h, m.w, h)) // the top is clear, and so is the way up
            {
                m.vy = -std::min(4.6f, std::sqrt(2 * 0.26f * (h + 5)));
                m.vx = dir * std::max(std::fabs(m.vx), 0.8f);
                return;
            }
        return; // a sheer wall
    }
    int col = (int)(dir > 0 ? m.x + m.w + 2 : m.x - 3), feet = (int)std::floor(m.y + m.h);
    for (int k = 0; k < 14; k++)
        if (isSolid(col, feet + k) || world.matU(col, feet + k) == M::Platform) return; // ground ahead
    M below = M::Empty;
    for (int k = 0; k < 40 && below == M::Empty; k++) below = world.matU(col, feet + k);
    bool deadly = below == M::Lava || below == M::Acid || below == M::Spikes;
    if (targetDy > 16 && !deadly) return; // the target is below: drop down after it
    for (int ahead = 6; ahead <= 44; ahead += 2) // a gap: is there ground across it to leap to?
    {
        int x = (int)m.cx() + dir * (m.w / 2 + ahead);
        for (int y = feet - 14; y <= feet + 16; y++)
            if (isSolid(x, y) && !isSolid(x, y - 1) && !boxSolid(x - m.w / 2.0f, (float)(y - m.h), m.w, m.h))
            {
                m.vy = -3.8f;
                m.vx = dir * 1.4f;
                return;
            }
    }
    m.vx = 0;
}

static void walkerPatrol(Mob& m, float spd)
{
    if (m.timer % 140 == 0) m.state = irand(3) - 1;
    if (m.wall) m.state = -m.wall;
    // don't stroll into pits or lava
    int fx = (int)(m.cx() + m.state * (m.w / 2 + 2));
    int fy = (int)(m.y + m.h);
    M below = world.matU(fx, fy);
    bool cliff = true;
    for (int k = 0; k < 10; k++) if (isSolid(fx, fy + k)) { cliff = false; break; }
    if (below == M::Lava || below == M::Acid || cliff) m.state = -m.state;
    m.vx += clampf(m.state * spd * 0.4f - m.vx, -0.15f, 0.15f);
    if (m.state) m.facing = m.state;
}

static void updateEnemy(Mob& m)
{
    const EnemyDef& d = ENEMIES[m.type];
    Mob& pm = G.p.m;
    float cx = m.cx(), cy = m.cy(), px = pm.cx(), py = pm.cy();
    float dx = px - cx, dy = py - cy, dist = std::sqrt(dx * dx + dy * dy);
    if ((G.frame + m.id) % 10 == 0)
        m.los = pm.alive && dist < 280 && lineOfSight(cx, cy, px, py);
    bool shooter = d.ai == AI_RANGED || d.ai == AI_BOMB || d.ai == AI_FLYCAST;
    if (!m.aggro && m.los && dist < (shooter ? 150 : 100)) m.aggro = true; // they only notice you up close
    if (m.boss && dist < 220 && !m.aggro) { m.aggro = true; playSfx(SFX_ROAR, 0.9f, m.type == E_LICH ? 1.3f : 0.8f); G.shake = 6; }
    if (m.aggro && dist > 260 && !m.boss) m.aggro = false;
    if (!pm.alive) m.aggro = false;
    if (m.cd > 0) m.cd--;
    if (m.attackT > 0) m.attackT--;
    if (m.bleedMark > 0)
    {
        m.bleedMark--;
        if (G.frame % 9 == 0) spawnCellParticle(m.cx(), m.cy(), frange(-0.5f, 0.5f), 0.3f, ENEMIES[m.type].gore, 0);
    }
    m.timer++;
    float pace = G.stage == 0 ? 0.55f : 1.0f; // the Greenmarch's foes are slower: you're still finding your feet
    float spd = d.speed * pace * (m.chill > 0 ? 0.45f : 1.0f);
    if (m.type == E_KELPIE && m.inLiquid) spd *= 2.2f;               // kelpies are deadly in the water
    if (m.type == E_TROLL && m.burn == 0 && m.hp < m.maxHp && !(G.p.amulet == AM_TROLLCROSS && std::fabs(m.cx() - G.p.m.cx()) < 120)) m.hp += 0.04f; // trolls regenerate unless burned (or warded)
    if (m.shock > 0) spd *= 0.2f;
    int fdir = dx > 0 ? 1 : -1;
    bool flying = d.ai == AI_FLY || d.ai == AI_FLYCAST || d.ai == AI_BOSS_LICH;
    float aimAng = std::atan2(dy, dx);

    if (m.stagger > 0) // reeling: whatever it was winding up is lost, and it can't steer
    {
        m.stagger--;
        m.attackT = 0;
        m.atkPhase = 0; // the blow is lost
        m.vx *= m.onGround ? 0.82f : 0.97f;
        if (flying) m.vy *= 0.9f;
    }
    else
    switch (d.ai)
    {
    case AI_WALK:
        if (m.atkPhase) runAttack(m, dx, dy);
        else if (m.aggro)
        {
            m.facing = fdir;
            float tv = std::fabs(dx) < d.range * 0.5f ? 0 : fdir * spd;
            if (m.onGround || std::fabs(m.vx) < std::fabs(tv)) m.vx += clampf(tv - m.vx, -0.2f, 0.2f); // keep a leap's momentum
            if (tv != 0) traverse(m, fdir, dy);
            if (m.onGround && m.vy == 0 && dy < -24 && chance(40)) m.vy = -3.9f;
            if (m.onGround && dy > 24 && onPlatform(m) && chance(30)) m.dropT = 12;
            if (m.cd == 0 && pm.alive && attackInReach(m, dx, dy)) beginAttack(m);
            if ((m.type == E_GOLEM || m.type == E_TROLL) && m.cd == 0 && m.los && dist > 40 && dist < 160 && chance(60))
            {
                m.cd = d.cooldown * 2;
                m.attackT = 12;
                float T = clampf(dist / 3.0f, 20, 50), g = 0.15f;
                enemyProj(PK_ROCK, cx, m.y + 2, dx / T, (dy - 0.5f * g * T * T) / T, m.dmg * 0.8f, EL_PHYS, g, 0);
            }
        }
        else walkerPatrol(m, spd);
        break;

    case AI_RANGED:
    case AI_BOMB:
        if (m.aggro)
        {
            m.facing = fdir;
            float tv = 0;
            if (dist < 55) tv = -fdir * spd;
            else if (dist > 120 || !m.los) tv = fdir * spd;
            if (m.onGround || std::fabs(m.vx) < std::fabs(tv)) m.vx += clampf(tv - m.vx, -0.2f, 0.2f);
            if (tv != 0) traverse(m, tv > 0 ? 1 : -1, dy);
            if (m.los && m.cd == 0 && dist < d.range)
            {
                m.cd = d.cooldown + irand(30);
                m.attackT = 15;
                if (m.type == E_ARCHER)
                {
                    float sp = 5.0f, g = 0.04f, t = dist / sp;
                    float a = std::atan2(dy - 0.5f * g * t * t, dx);
                    enemyProj(PK_ARROW, cx, cy - 2, std::cos(a) * sp, std::sin(a) * sp, m.dmg, EL_PHYS, g, 0);
                }
                else if (m.type == E_CULTIST)
                    enemySpell(G.stage >= 3 && chance(2) ? SP_ICE : SP_FIREBALL, m, aimAng, m.dmg, G.stage >= 3 && chance(2) ? EL_ICE : EL_FIRE);
                else
                {
                    float T = clampf(dist / 2.5f, 30, 70), g = 0.15f;
                    enemyProj(PK_BOMB, cx, m.y, dx / T, (dy - 0.5f * g * T * T) / T, m.dmg, EL_FIRE, g, 6);
                }
            }
            if (m.type == E_CULTIST && dist < 35 && chance(50))
            {
                float nx, ny;
                if (findFloorNear(cx, cy, nx, ny, m.w, m.h))
                {
                    for (int i = 0; i < 20; i++) spawnParticle(cx, cy, frange(-1, 1), frange(-1, 1), 20, {170, 80, 220, 255}, 0);
                    m.x = nx - m.w / 2.0f; m.y = ny - m.h;
                }
            }
        }
        else walkerPatrol(m, spd);
        break;

    case AI_FLY:
        if (m.atkPhase)
        {
            runAttack(m, dx, dy);
            if (!m.atkPhase) { m.state = 5; m.timer = 0; } // then away, and round again
        }
        else if (m.aggro)
        {
            if (m.state <= 1 && m.state >= -1 && m.timer > 0)
            {
                float sp = spd * 1.3f;
                m.vx += (std::cos(aimAng) * sp - m.vx) * 0.08f;
                m.vy += (std::sin(aimAng) * sp + std::sin(m.timer * 0.2f) * 0.6f - m.vy) * 0.08f;
                if (m.cd == 0 && pm.alive && attackInReach(m, dx, dy)) beginAttack(m);
            }
            else
            {
                m.vx += (-fdir * spd - m.vx) * 0.05f;
                m.vy += (-spd - m.vy) * 0.08f;
                if (m.timer > 40) m.state = 0;
            }
            m.facing = fdir;
        }
        else
        {
            m.vx = std::sin(m.timer * 0.03f + m.id) * 0.4f;
            m.vy = std::cos(m.timer * 0.05f + m.id) * 0.3f;
        }
        break;

    case AI_FLYCAST:
    {
        float tx = px - fdir * 60, ty = py - 30;
        if (!m.aggro) { tx = cx + std::sin(m.timer * 0.02f + m.id) * 20; ty = cy + std::cos(m.timer * 0.03f) * 10; }
        float ddx = tx - cx, ddy = ty - cy, dd = std::hypot(ddx, ddy) + 0.01f;
        float sp = std::min(spd, dd * 0.05f);
        m.vx += (ddx / dd * sp - m.vx) * 0.06f;
        m.vy += (ddy / dd * sp + std::sin(m.timer * 0.1f) * 0.3f - m.vy) * 0.06f;
        m.facing = fdir;
        if (m.aggro && m.los && m.cd == 0 && dist < d.range)
        {
            m.cd = d.cooldown + irand(40);
            m.attackT = 15;
            if (m.type == E_IMP) enemySpell(SP_SPARK, m, aimAng, m.dmg, EL_FIRE);
            else if (m.type == E_BANSHEE) { enemySpell(SP_MISSILE, m, aimAng, m.dmg, EL_SHOCK); playAt(SFX_ROAR, cx, cy, 0.35f, 2.2f); } // a keening wail
            else enemySpell(SP_ICE, m, aimAng, m.dmg, EL_ICE);
        }
        break;
    }

    case AI_HOP:
        if (m.atkPhase) runAttack(m, dx, dy);
        else if (m.onGround)
        {
            m.vx *= 0.8f;
            if (m.aggro && m.cd == 0 && pm.alive && attackInReach(m, dx, dy)) beginAttack(m); // a squat, then a pounce
            else if (m.aggro && m.cd == 0) // hop closer
            {
                m.vx = fdir * 1.7f * pace * (m.chill > 0 ? 0.5f : 1.0f);
                m.vy = -3.4f;
                m.cd = 25 + irand(20);
            }
            else if (!m.aggro && m.timer % 150 == 0)
            {
                m.vx = (irand(2) ? 1 : -1) * 0.8f;
                m.vy = -2.5f;
            }
        }
        if (!m.atkPhase) m.facing = fdir;
        break;

    case AI_BOSS_KNIGHT:
        if (!m.aggro) break;
        if (m.atkPhase) runAttack(m, dx, dy);
        else if (m.state == 0 || m.state == 1 || m.state == -1)
        {
            m.facing = fdir;
            float tv = std::fabs(dx) < 10 ? 0 : fdir * spd;
            m.vx += clampf(tv - m.vx, -0.2f, 0.2f);
            if (m.onGround && m.wall == fdir) m.vy = -4.5f;
            if (m.cd == 0 && std::fabs(dx) < d.range + 8 && std::fabs(dy) < m.h) beginAttack(m);
            if (m.timer > 220 && m.onGround) { m.state = 2; m.timer = 0; message("The Black Knight lowers his lance!"); }
            else if (m.timer > 150 && m.onGround && chance(120)) { m.state = 5; m.vy = -6.0f; m.vx = clampf(dx / 30.0f, -3, 3); m.timer = 0; }
        }
        else if (m.state == 2) // wind-up
        {
            m.vx *= 0.8f;
            if (m.timer % 4 == 0) spawnParticle(cx, m.y, frange(-1, 1), -1, 12, {255, 60, 60, 255}, 0);
            if (m.timer > 35) { m.state = 3; m.timer = 0; m.facing = fdir; }
        }
        else if (m.state == 3) // charge, smashing terrain
        {
            m.vx = m.facing * 3.6f;
            if (m.timer % 4 == 0) explodeCells((int)(cx + m.facing * 24), (int)cy, 16, 5);
            touchDamage(m, 1.2f);
            if (m.timer > 70) { m.state = 4; m.timer = 0; }
        }
        else if (m.state == 4) // recover
        {
            m.vx *= 0.85f;
            if (m.timer > 50) { m.state = 0; m.timer = 0; }
        }
        else if (m.state == 5) // leap
        {
            if (m.onGround && m.timer > 5)
            {
                explode(cx, m.y + m.h, 12, 22, EL_PHYS, false, 3);
                m.state = 0;
                m.timer = 0;
            }
        }
        break;

    case AI_BOSS_LICH:
    {
        if (!m.aggro) break;
        bool phase2 = m.hp < m.maxHp * 0.5f;
        int side = ((m.timer / 300) & 1) ? 1 : -1;
        float tx = px + side * 90, ty = py - 50;
        float ddx = tx - cx, ddy = ty - cy, dd = std::hypot(ddx, ddy) + 0.01f;
        float sp = std::min(spd * (phase2 ? 1.5f : 1.0f), dd * 0.04f);
        m.vx += (ddx / dd * sp - m.vx) * 0.05f;
        m.vy += (ddy / dd * sp + std::sin(m.timer * 0.05f) * 0.4f - m.vy) * 0.05f;
        m.facing = fdir;
        if (m.cd == 0)
        {
            m.cd = (int)(d.cooldown * (phase2 ? 0.6f : 1.0f)) + irand(20);
            m.attackT = 20;
            switch (m.state = (m.state + 1) % 4)
            {
            case 0:
                for (int k = -2; k <= 2; k++) enemySpell(SP_FIREBALL, m, aimAng + k * 0.25f, m.dmg, EL_FIRE);
                break;
            case 1:
                for (int k = -1; k <= 1; k++) enemySpell(SP_ICE, m, aimAng + k * 0.12f, m.dmg * 0.8f, EL_ICE);
                break;
            case 2:
                enemySpell(SP_LIGHTNING, m, aimAng, m.dmg * 1.2f, EL_SHOCK);
                break;
            case 3:
                for (int k = 0; k < (phase2 ? 3 : 2); k++)
                {
                    float nx, ny;
                    if (findFloorNear(px, py, nx, ny, 12, 22))
                    {
                        Mob s = makeEnemy(chance(3) ? E_ARCHER : E_SKELETON, nx, ny);
                        s.aggro = true;
                        pendingMobs.push_back(s);
                        for (int i = 0; i < 16; i++) spawnParticle(nx, ny - 5, frange(-1, 1), frange(-2, 0), 20, {170, 80, 220, 255}, 0);
                    }
                }
                break;
            }
        }
        break;
    }
    }

    if (m.type == E_WOLF && (m.aggro ? chance(160) : chance(1500))) // wolves make themselves known
    {
        bool howl = !m.aggro && chance(3);
        playAt(howl ? SFX_HOWL : SFX_BARK, cx, cy, howl ? 0.5f : 0.6f, 0.85f + (m.id % 5) * 0.06f);
    }

    if (!flying)
    {
        if (m.inLiquid)
        {
            m.vy = std::min(m.vy + 0.06f, 1.0f);
            m.vx *= 0.9f;
            if (m.type == E_KELPIE && m.aggro) m.vy = clampf(m.vy - 0.06f + clampf((py - cy) * 0.02f, -0.3f, 0.3f), -2.2f, 2.2f); // kelpies hunt through the water, up or down
            else if (m.aggro && py < cy) m.vy -= 0.12f;
        }
        else
            m.vy = std::min(m.vy + 0.26f, 6.0f);
    }
    if (d.noclip)
    {
        m.x = clampf(m.x + m.vx, 4, world.wU() - m.w - 4.0f);
        m.y = clampf(m.y + m.vy, 4, world.hU() - m.h - 4.0f);
    }
    else
    {
        float vyB = m.vy;
        bool wasG = m.onGround;
        moveMob(m);
        if (m.onGround && !wasG && vyB > 1.5f) m.squash = 0.7f;
    }
    m.squash += (1 - m.squash) * 0.2f;
    if (m.y > world.hU() + 10) m.hp = 0;
    m.anim += std::fabs(m.vx) * 0.15f + (flying ? 0.15f : 0);
}

static void killMob(Mob& m)
{
    m.alive = false;
    const EnemyDef& d = ENEMIES[m.type];
    G.p.kills++;
    G.hitstop = std::max(G.hitstop, 3);
    spawnCorpse(m);
    playAt(SFX_DIE, m.cx(), m.cy(), 0.7f, m.boss ? 0.5f : frange(0.9f, 1.3f));
    for (int i = 0; i < 14 + m.w * 2; i++)
        spawnCellParticle(m.cx() + frange(-2, 2), m.cy() + frange(-3, 3), m.vx * 0.5f + frange(-1.8f, 1.8f), frange(-3.0f, 0), d.gore, 0);
    bleed(m, d.blood, 12);
    if (d.gore == M::Blood && std::hypot(m.cx() - G.p.m.cx(), m.cy() - G.p.m.cy()) < 20) G.p.m.bloody = std::max(G.p.m.bloody, 480);
    if (m.bleedMark > 0) // cut by a bleeding weapon: a fountain of gore
        for (int i = 0; i < 50; i++)
            spawnCellParticle(m.cx(), m.cy(), frange(-2.5f, 2.5f), frange(-4.0f, -0.5f), d.gore, 0);
    if (m.boss) addCoins(m.cx(), m.cy(), 15, 4);
    else if (chance(2)) addCoins(m.cx(), m.cy(), irange(1, 2) + G.stage / 3, 1); // not every foe carries coin
    if (m.type == E_SLIME) paintCircle((int)m.cx(), (int)m.cy(), 2, M::Acid, true);
    if (m.type == E_BOMBER) explode(m.cx(), m.cy(), 5, 10, EL_FIRE, false, 3);
    dropLoot(m.cx(), m.cy(), m.boss);
    if (m.boss)
    {
        G.shake = 14;
        message(std::string(d.name) + " has fallen!");
        if (m.type == E_LICH) G.winTimer = 200;
        for (auto& h : G.havens) // its haven's gate grinds open
            if (h.locked && h.bossId == m.id)
            {
                h.locked = false;
                setGate(h.x0, h.x0 + HAVEN_WALL - 1, h.floor - HAVEN_DOOR, h.floor - 1, false);
                playSfx(SFX_PORTAL, 0.7f, 0.6f);
                message("Somewhere ahead, a gate grinds open. The way on is clear.");
            }
    }
}

// ================================================================ the folk of Hearthwick
// A few villagers and hens potter about between the halls: they wander, stop, turn to look at you, and
// have a word to say when you come close.
struct Villager { float x = 0, y = 0, anim = 0; int kind = 0, dir = 1, walk = 0, wait = 0, talkCd = 0; Color coat = WHITE; };
static std::vector<Villager> folk;
static int folkGen = -1;
static const char* FOLK_SAY[] = {"Skal!", "Fair winds to you.", "Mind the oil down there. It burns hot.", "Water puts a fire out. Remember that.",
                                 "The crypts took my brother.", "They say the Lich King was a man once.", "Gold for the smith, coin for the ale.",
                                 "Come back with your shield, or on it.", "The longship's waiting.", "Odin's eye on you."};
static const char* CHILD_SAY[] = {"Are you going down there?!", "I found a coin! ...it was a button.", "Can I hold your sword?", "Bring me back a skull!"};

static float groundUnder(float x, float y) // the first solid row at or below y-8 in column x (units)
{
    int yy = (int)y - 8;
    while (yy < world.hU() - 1 && !isSolid((int)x, yy)) yy++;
    return (float)yy;
}

static void updateVillagers()
{
    if (!G.inVillage) { folk.clear(); folkGen = -1; return; }
    const int x0 = 50, x1 = world.wU() - 330; // the land, short of the pier
    if (folkGen != world.gen)
    {
        folkGen = world.gen;
        folk.clear();
        static const Color COATS[] = {{128, 44, 38, 255}, {52, 82, 132, 255}, {74, 104, 62, 255}, {150, 116, 64, 255}, {104, 72, 124, 255}, {156, 146, 124, 255}};
        static const int KINDS[] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 3, 3};
        for (int k : KINDS)
        {
            Villager v;
            v.kind = k;
            v.x = (float)irange(x0, x1);
            v.y = groundUnder(v.x, G.p.m.y + G.p.m.h - 10); // on the ground, not the roofs
            v.coat = COATS[irand(6)];
            v.dir = chance(2) ? 1 : -1;
            v.wait = irange(0, 200);
            folk.push_back(v);
        }
    }
    const Mob& pm = G.p.m;
    for (auto& v : folk)
    {
        if (v.talkCd > 0) v.talkCd--;
        float speed = v.kind == 3 ? 0.3f : (v.kind == 2 ? 0.45f : 0.28f);
        if (v.wait > 0)
        {
            if (--v.wait == 0) { v.dir = chance(2) ? 1 : -1; v.walk = irange(60, 260); }
            if (v.kind == 3 && chance(25)) v.anim += 1; // hens peck
        }
        else if (v.walk > 0)
        {
            float nx = v.x + v.dir * speed;
            if (nx < x0 || nx > x1 || isSolid((int)nx, (int)v.y - 4)) v.dir = -v.dir; // the edge of the village, or a wall
            else { v.x = nx; v.anim += speed * 0.22f; }
            if (--v.walk == 0) v.wait = irange(60, 320);
        }
        v.y = groundUnder(v.x, v.y);
        float d = std::fabs(pm.cx() - v.x);
        if (v.kind != 3 && d < 26 && std::fabs(pm.y + pm.h - v.y) < 20)
        {
            if (v.walk == 0) v.dir = pm.cx() < v.x ? -1 : 1; // turn to look at you
            if (v.talkCd == 0 && d < 18)
            {
                const char* line = v.kind == 2 ? CHILD_SAY[irand(4)] : FOLK_SAY[irand(10)];
                addText(v.x, v.y - (v.kind == 2 ? 13 : 19), line, {236, 222, 190, 255});
                v.talkCd = irange(900, 1500);
            }
        }
    }
}

static void drawVillagers(int camX, int camY)
{
    for (auto& v : folk)
    {
        float x = v.x - camX, y = v.y - camY;
        if (x < -20 || x > G.vw + 20 || y < -30 || y > G.vh + 30) continue;
        bool b = ((int)v.anim) & 1;
        const Sprite& s = v.kind == 0 ? (b ? SPR_MAN_B : SPR_MAN_A) : v.kind == 1 ? (b ? SPR_WOMAN_B : SPR_WOMAN_A) : v.kind == 2 ? (b ? SPR_CHILD_B : SPR_CHILD_A) : (b ? SPR_HEN_B : SPR_HEN_A);
        drawSpriteTint(s, x, y, v.dir < 0, v.coat);
    }
}

// ================================================================ particles, pickups, traps

static void updateParticles()
{
    for (auto& q : G.parts)
    {
        q.life--;
        q.vy += q.grav;
        float nx = q.x + q.vx, ny = q.y + q.vy;
        if (q.toCell != M::Empty)
        {
            int sc = world.scale, cx = (int)std::floor(nx * sc), cy = (int)std::floor(ny * sc);
            if (!world.in(cx, cy)) { q.life = 0; continue; }
            if (world.get(cx, cy).material != M::Empty || q.life <= 0)
            {
                int px = (int)std::floor(q.x * sc), py = (int)std::floor(q.y * sc);
                if (world.in(px, py) && world.get(px, py).material == M::Empty)
                {
                    setCellC(px, py, q.toCell);
                    world.at(px, py).flags |= q.cellFlags;
                }
                q.life = 0;
                continue;
            }
        }
        else if (q.grav > 0.05f && isSolid((int)std::floor(nx), (int)std::floor(ny)))
        {
            q.vy *= -0.3f; // debris bounces and settles instead of sinking into rock
            q.vx *= 0.6f;
            continue;
        }
        q.x = nx;
        q.y = ny;
    }
    G.parts.erase(std::remove_if(G.parts.begin(), G.parts.end(), [](const Particle& q) { return q.life <= 0; }), G.parts.end());

    for (auto& d : world.debris)
    {
        Color c = cellColor(d.cell, (int)d.x, (int)d.y);
        float sc = (float)world.scale; // debris comes in cells
        Particle p{d.x / sc, d.y / sc, d.vx / sc, d.vy / sc, 200, c, 0.15f};
        p.toCell = d.cell.material;
        p.cellFlags = d.cell.flags & CF_LOOSE;
        if (G.parts.size() < 5000) G.parts.push_back(p);
    }
    world.debris.clear();
}

static void updatePickups()
{
    Player& P = G.p;
    for (auto& pu : G.pickups)
    {
        if (!pu.alive) continue;
        pu.age++;
        float pdx = P.m.cx() - pu.b.cx(), pdy = P.m.cy() - pu.b.cy(), pd = std::sqrt(pdx * pdx + pdy * pdy) + 0.01f;
        if (pu.kind == PU_COIN && pu.age > 25 && pd < 48 && P.m.alive) // coins are drawn to you
        {
            pu.b.vx += pdx / pd * 0.5f;
            pu.b.vy += pdy / pd * 0.5f;
            pu.b.vx *= 0.9f;
            pu.b.vy *= 0.9f;
            pu.b.x += pu.b.vx;
            pu.b.y += pu.b.vy;
        }
        else
        {
            pu.b.vy = std::min(pu.b.vy + 0.2f, 4.0f);
            pu.b.vx *= pu.kind == PU_COIN && !pu.b.onGround ? 0.99f : 0.95f;
            float vy = pu.b.vy, vx = pu.b.vx;
            moveMob(pu.b);
            if (pu.kind == PU_COIN) // coins bounce, ricochet off walls and roll away downhill
            {
                if (pu.b.vy == 0 && vy > 1.0f) { pu.b.vy = -vy * 0.45f; pu.b.vx += frange(-0.4f, 0.4f); }
                if (pu.b.vx == 0 && std::fabs(vx) > 0.5f) pu.b.vx = -vx * 0.5f;
                if (pu.b.onGround)
                    for (int d : {-1, 1})
                        if (!boxSolid(pu.b.x + d * 2, pu.b.y + 2, pu.b.w, pu.b.h)) pu.b.vx += d * 0.12f;
            }
        }
        if (pu.b.y > world.hU() + 10) pu.alive = false;
        if (pu.kind == PU_WEAPON && pu.weapon.glow.a && G.frame % 7 == 0)
            spawnParticle(pu.b.cx() + frange(-5, 5), pu.b.cy() + frange(-3, 3), 0, -0.4f, 30, pu.weapon.glow, -0.002f);
        if (!P.m.alive || !overlap(pu.b, P.m) || pu.age < 30) continue;
        switch (pu.kind)
        {
        case PU_COIN:
            P.coins += pu.spell;
            playSfx(SFX_ORE, 0.4f, 1.7f);
            pu.alive = false;
            break;
        case PU_SPELL:
            P.bag.push_back(makeCard(pu.spell));
            message(std::string("Spell found: ") + SPELLS[pu.spell].name + "  (TAB to equip)");
            playSfx(SFX_PICKUP, 0.6f);
            pu.alive = false;
            break;
        case PU_POTION:
            P.potions++;
            message("Healing flask  (Q to drink)");
            playSfx(SFX_PICKUP, 0.6f, 0.9f);
            pu.alive = false;
            break;
        case PU_MEAD:
            P.m.hp = P.m.maxHp;
            P.m.burn = P.m.poison = P.m.chill = 0;
            addText(P.m.cx(), P.m.y - 4, "Healed whole", {255, 214, 120, 255});
            message("You drain the horn of mead. Your wounds close.");
            playSfx(SFX_POTION, 0.8f, 0.8f);
            pu.alive = false;
            break;
        case PU_AMULET:
            if (P.amulet < 0 || IsKeyPressed(KEY_F))
            {
                if (P.amulet >= 0) // swapped: the old one is left lying
                {
                    addPickup(pu.b.x, pu.b.y - 2, PU_AMULET);
                    G.pickups.back().spell = P.amulet;
                    G.pickups.back().age = -90;
                }
                wearAmulet(pu.spell);
                message(std::string("You put on ") + AMULETS[pu.spell].name + ": " + AMULETS[pu.spell].desc);
                playSfx(SFX_PICKUP, 0.7f, 0.7f);
                pu.alive = false;
            }
            else if (pu.age % 150 == 30)
                message(std::string(AMULETS[pu.spell].name) + " - press F to wear it instead of your " + AMULETS[P.amulet].name);
            break;
        case PU_HEART:
            P.m.maxHp += 5;
            P.m.hp = std::min(P.m.maxHp, P.m.hp + 30);
            addText(P.m.cx(), P.m.y - 4, "+30  max HP +5", {255, 120, 140, 255});
            playSfx(SFX_PICKUP, 0.6f, 1.2f);
            pu.alive = false;
            break;
        case PU_WEAPON:
            if (pu.age < 60) break;
            if (P.hotbar.size() < 6)
            {
                P.hotbar.push_back(pu.weapon);
                message((pu.weapon.fx ? "Legendary! " : "Picked up ") + weaponName(pu.weapon));
                playSfx(SFX_PICKUP, 0.6f, 0.8f);
                pu.alive = false;
            }
            else if (pu.age % 120 == 0)
                message("Hotbar full - press G to drop your current item");
            break;
        }
    }
    G.pickups.erase(std::remove_if(G.pickups.begin(), G.pickups.end(), [](const Pickup& p) { return !p.alive; }), G.pickups.end());
}

static void updateTraps()
{
    Mob& pm = G.p.m;
    for (auto& t : G.traps)
    {
        if (t.timer > 0) t.timer--;
        switch (t.type)
        {
        case TR_ARROW:
        {
            float dx = pm.cx() - t.x;
            if (pm.alive && t.timer == 0 && std::fabs(pm.cy() - t.y) < 12 && dx * t.dir > 0 && std::fabs(dx) < 160 &&
                lineOfSight(t.x + t.dir * 2.0f, (float)t.y, pm.cx(), pm.cy()))
            {
                enemyProj(PK_ARROW, t.x + t.dir * 2.0f, (float)t.y, t.dir * 6.0f, 0, 14.0f + G.stage * 3, EL_PHYS, 0.01f, 0);
                t.timer = 80;
            }
            break;
        }
        case TR_FLAME:
        {
            int phase = G.frame % 240;
            if (phase < 70)
                for (int k = 1; k <= 3; k++)
                    if (world.inU(t.x, t.y - k) && world.matU(t.x, t.y - k) == M::Empty && chance(2))
                    {
                        setCell(t.x + irange(-1, 1), t.y - k, M::Fire);
                    }
            if (phase < 70 && G.frame % 2 == 0)
                spawnParticle((float)t.x, (float)t.y - 2, frange(-0.4f, 0.4f), frange(-2.5f, -1.2f), irange(10, 24), {255, (unsigned char)irange(80, 220), 30, 255}, 0);
            break;
        }
        case TR_COLLAPSE:
            if (!t.done && pm.alive && pm.cx() > t.rx0 - 4 && pm.cx() < t.rx1 + 4 && pm.y > t.ry1)
            {
                t.done = true;
                G.shake = 8;
                message("The ceiling gives way!");
                for (int y = t.ry0 * world.scale; y < (t.ry1 + 1) * world.scale; y++)
                    for (int x = t.rx0 * world.scale; x < (t.rx1 + 1) * world.scale; x++)
                    {
                        if (!world.in(x, y)) continue;
                        disturb(x, y, 1);
                        Cell& c = world.at(x, y);
                        const MaterialProps& p = props(c.material);
                        if (p.kind != Kind::Solid || p.hardness > 5) continue;
                        if (p.ore) c.flags |= CF_LOOSE;
                        else if (c.material == M::Wood) c = Cell{};
                        else c.material = c.material == M::Dirt ? M::Sand : M::Gravel;
                    }
            }
            break;
        }
    }
}

static void updateInteract()
{
    for (auto& it : G.inter) // crates burnt or blasted mostly away fall apart
    {
        if (it.type != IT_CRATE || it.used) continue;
        if (it.hit > 0) it.hit--;
        if ((G.frame + (int)it.x) % 15) continue;
        int n = 0;
        for (int y = (int)it.y - it.h; y < (int)it.y; y++)
            for (int x = (int)it.x; x < (int)it.x + it.w; x++) n += world.matU(x, y) == M::Wood;
        if (n * 5 < it.cells * 2) breakCrate(it, 0, false);
    }
    Mob& pm = G.p.m;
    G.nearInteract = -1;
    float bd = 26;
    for (int i = 0; i < (int)G.inter.size(); i++)
    {
        Interact& it = G.inter[i];
        if ((it.type == IT_CHEST && it.used) || (it.type == IT_BOAT && it.used) || it.type == IT_TORCH || it.type == IT_ROPE || it.type == IT_CRATE || it.type == IT_LANTERN || it.type == IT_PROP) continue;
        float d = std::hypot(it.x - pm.cx(), it.y - 10 - pm.cy());
        if (d < bd) { bd = d; G.nearInteract = i; }
    }
    if (G.nearInteract < 0 || !IsKeyPressed(KEY_F) || !pm.alive) return;
    Interact& it = G.inter[G.nearInteract];
    switch (it.type)
    {
    case IT_CHEST:
        it.used = true;
        playSfx(SFX_CHEST, 0.8f);
        spawnOreBurst(it.x, it.y - 6, irange(10, 25));
        addPickupSpell(it.x, it.y - 6, randomSpell(G.stage));
        if (chance(7)) addPickup(it.x, it.y - 6, PU_POTION);
        if (chance(2)) addPickupSpell(it.x, it.y - 6, randomSpell(G.stage));
        if (chance(12)) addPickupWeapon(it.x, it.y - 6, randomWeapon(G.stage));
        if (chance(4)) addPickup(it.x, it.y - 6, PU_HEART);
        if (chance(12)) { addPickup(it.x, it.y - 6, PU_AMULET); G.pickups.back().spell = randomAmulet(); }
        if (G.stage >= 1 && chance(20)) addPickupWeapon(it.x, it.y - 6, rollLegendary(G.stage + 1, false));
        addCoins(it.x, it.y - 6, irange(4, 8) + G.stage, 1);
        break;
    case IT_STONE:
        if (it.used) message("Only an empty cleft remains in the stone.");
        else if (G.p.hotbar.size() >= 6) message("Your hands are full - drop something first (Tab, drag to the bin).");
        else
        {
            it.used = true;
            const Weapon& w = G.stoneLoot[it.data];
            G.p.hotbar.push_back(w);
            message("You take the " + weaponName(w) + "!");
            playSfx(SFX_CRAFT, 0.9f, 1.4f);
            G.shake = 6;
            for (int k = 0; k < 40; k++) spawnParticle(it.x, it.y - 14, frange(-2, 2), frange(-3, 0), irange(20, 50), w.glow, 0.05f);
        }
        break;
    case IT_ANVIL: G.state = GS_ANVIL; break;
    case IT_SHOP: G.shopId = it.data; G.state = GS_SHOP; break;
    case IT_BOAT: // cast off: the longship carries you out of the harbour
        G.sailT = 1;
        playSfx(SFX_SPLASH, 0.7f, 0.7f);
        message("You cast off. The longship slips out into the dark...");
        break;
    }
}

// Sail out of Hearthwick on a fresh run with the chosen loadout.
void travelOnward()
{
    if (G.sandbox || !G.inVillage) return;
    newGameKit(false);
    spendKit(); // they're aboard now: next time you're in Hearthwick the stalls are yours to shop again
    G.stage = 0;
    G.loadTarget = LOAD_STAGE;
    G.state = GS_LOADING;
}

// ================================================================ chests: little rigid bodies
// A box with mass and spin. Points round its outline test the terrain; each one that's in rock is a
// contact, and gets a bounce (restitution) and friction impulse at that point, so a chest tips off a
// ledge, tumbles down a slope and rattles to a stop. It sleeps once settled, and wakes if the ground under
// it goes, or a blast throws it.
bool rigidStep(RigidBody& b, const std::vector<Vector2>& pts, float I, bool square)
{
    const float M = 1, e = 0.32f, mu = 0.55f;
    float cx = b.x, cy = b.y;
    bool touching = false, wet = false;
    for (int sub = 0; sub < 2; sub++)
    {
        b.vy = std::min(b.vy + 0.11f, 4.0f);
        cx += b.vx * 0.5f; cy += b.vy * 0.5f; b.ang += b.va * 0.5f;
        float cs = std::cos(b.ang), sn = std::sin(b.ang);
        Vector2 rs[64], ns[64];
        int nc = 0;
        Vector2 sumN = {0, 0};
        for (auto& p : pts)
        {
            Vector2 r = {p.x * cs - p.y * sn, p.x * sn + p.y * cs};
            int wx = (int)std::floor(cx + r.x), wy = (int)std::floor(cy + r.y);
            if (isLiquidAt(wx, wy)) wet = true;
            if (!isSolid(wx, wy)) continue;
            Vector2 n = {0, 0}; // away from the rock: towards whichever neighbours are open
            for (int dy = -2; dy <= 2; dy += 2)
                for (int dx = -2; dx <= 2; dx += 2)
                    if ((dx || dy) && !isSolid(wx + dx, wy + dy)) { n.x += dx; n.y += dy; }
            float l = std::sqrt(n.x * n.x + n.y * n.y);
            if (l < 0.01f) { n = {-r.x, -r.y}; l = std::sqrt(n.x * n.x + n.y * n.y) + 0.01f; } // buried: back towards the middle
            n.x /= l; n.y /= l;
            rs[nc] = r; ns[nc] = n; nc++;
            if (isSolid((int)std::floor(cx + r.x + n.x * 1.2f), (int)std::floor(cy + r.y + n.y * 1.2f))) { sumN.x += n.x; sumN.y += n.y; } // only a deep point pushes out: resting ones are left to the impulses
            if (nc == 64) break;
        }
        if (!nc) continue;
        touching = true;
        for (int i = 0; i < nc; i++)
        {
            Vector2 r = rs[i], n = ns[i];
            Vector2 vr = {b.vx - b.va * r.y, b.vy + b.va * r.x}; // the velocity of that point
            float vn = vr.x * n.x + vr.y * n.y;
            if (vn >= 0) continue;
            float rn = r.x * n.y - r.y * n.x;
            float j = -(1 + (vn < -0.6f ? e : 0)) * vn / (1 / M + rn * rn / I) / nc; // gentle contacts don't bounce
            b.vx += j * n.x / M; b.vy += j * n.y / M; b.va += rn * j / I;
            Vector2 t = {-n.y, n.x};
            float vt = (b.vx - b.va * r.y) * t.x + (b.vy + b.va * r.x) * t.y, rt = r.x * t.y - r.y * t.x;
            float jt = clampf(-vt / (1 / M + rt * rt / I) / nc, -mu * j, mu * j);
            b.vx += jt * t.x / M; b.vy += jt * t.y / M; b.va += rt * jt / I;
        }
        float l = std::sqrt(sumN.x * sumN.x + sumN.y * sumN.y);
        if (l > 0.01f) { cx += sumN.x / l * 0.4f; cy += sumN.y / l * 0.4f; } // ease out of the rock
    }
    if (wet) { b.vx *= 0.94f; b.vy *= 0.94f; b.va *= 0.94f; }
    b.va *= 0.995f;
    float speed = std::fabs(b.vx) + std::fabs(b.vy);
    if (touching && speed > 1.6f && b.rest == 0) playAt(SFX_KNOCK, cx, cy, std::min(0.8f, speed * 0.2f), 0.7f); // a thud as it lands
    // Lying on a face (its two lowest points level and apart) it can rest; on a single point it's still
    // tipping, unless something's propping it up and it has truly stopped turning.
    float cs = std::cos(b.ang), sn = std::sin(b.ang), loY = -1e9f, loX = 0;
    for (auto& p : pts) { float y = p.x * sn + p.y * cs; if (y > loY) { loY = y; loX = p.x * cs - p.y * sn; } }
    bool flat = false;
    for (auto& p : pts) flat = flat || (p.x * sn + p.y * cs > loY - 1.5f && std::fabs(p.x * cs - p.y * sn - loX) >= 3);
    b.rest = touching && speed < 0.3f && std::fabs(b.va) < (flat ? 0.01f : 0.0015f) ? b.rest + 1 : 0;
    if (b.rest > 40) // settled: asleep, squared up if it's nearly level
    {
        b.vx = b.vy = b.va = 0;
        float q = std::round(b.ang / (PI / 2)) * (PI / 2);
        if (square && std::fabs(b.ang - q) < 0.2f) b.ang = q;
    }
    b.x = cx;
    b.y = cy;
    return touching;
}

// Chests and loose props: boxes, with their outline every couple of units.
static void chestPhysics(Interact& it)
{
    Vector2 hb = bodyHalf(it);
    const float HW = hb.x, HH = hb.y;
    std::vector<Vector2> pts;
    float sx = 2 * HW / std::ceil(2 * HW / 2.4f), sy = 2 * HH / std::ceil(2 * HH / 2.4f);
    for (float x = -HW; x <= HW + 0.01f; x += sx) { pts.push_back({x, HH}); pts.push_back({x, -HH}); }
    for (float y = -HH + sy; y < HH - 0.5f; y += sy) { pts.push_back({-HW, y}); pts.push_back({HW, y}); }
    RigidBody b{it.x, it.y - HH, it.vx, it.vy, it.ang, it.va, it.rest};
    rigidStep(b, pts, ((2 * HW) * (2 * HW) + (2 * HH) * (2 * HH)) / 12, true);
    it.x = b.x; it.y = b.y + HH; it.vx = b.vx; it.vy = b.vy; it.ang = b.ang; it.va = b.va; it.rest = b.rest;
}

static void updateChests()
{
    propBoxes.clear();
    propBoxGen = world.gen;
    for (auto& it : G.inter)
    {
        if (!isBody(it) || (it.type == IT_PROP && it.used) || std::fabs(it.x - G.camX - G.vw / 2) > G.vw + 200 || std::fabs(it.y - G.camY - G.vh / 2) > G.vh + 200) continue;
        Vector2 hb = bodyHalf(it);
        if (it.type == IT_PROP)
        {
            float cx = it.x, cy = it.y - hb.y, r = std::max(hb.x, hb.y);
            for (auto& pr : G.projs) // a bolt or a spell knocks it along
                if (pr.alive && std::fabs(pr.x - cx) < r && std::fabs(pr.y - cy) < r)
                {
                    float k = clampf(std::sqrt(pr.vx * pr.vx + pr.vy * pr.vy) * 0.3f, 0.6f, 2.0f), l = std::sqrt(pr.vx * pr.vx + pr.vy * pr.vy) + 0.01f;
                    hitProp(it, 1, pr.vx / l * k, pr.vy / l * k - 0.6f);
                    projImpact(pr, pr.x, pr.y);
                    if (it.used) break;
                }
            if (it.used) continue;
            float ex = hb.x * std::fabs(std::cos(it.ang)) + hb.y * std::fabs(std::sin(it.ang)), ey = hb.x * std::fabs(std::sin(it.ang)) + hb.y * std::fabs(std::cos(it.ang));
            auto shove = [&](const Mob& m) { // walk into its side and you push it along
                if (!m.alive || !m.wall || m.y + m.h <= cy - ey + 0.5f || m.y >= cy + ey) return;
                float gap = m.wall > 0 ? (cx - ex) - (m.x + m.w) : m.x - (cx + ex);
                if (gap < -1 || gap > 1.5f) return;
                float step = m.wall * 0.55f, front = m.wall > 0 ? cx + ex + step : cx - ex + step;
                for (float y = cy - ey + 1; y < cy + ey - 1; y += 1) // slides along the floor, unless a wall's in the way
                    if (isSolid((int)std::floor(front), (int)std::floor(y))) return;
                for (auto& o : G.inter) // or another of its kind
                    if (&o != &it && o.type == IT_PROP && !o.used && std::fabs(o.x - (cx + step)) < ex + bodyHalf(o).x && std::fabs(o.y - it.y) < ey + bodyHalf(o).y) return;
                it.x += step;
                cx += step;
            };
            shove(G.p.m);
            for (auto& m : G.mobs) if (std::fabs(m.x - cx) < 30) shove(m);
        }
        if (it.rest > 40)
        {
            if ((G.frame + (int)it.x) % 10) continue; // asleep: look now and then for the ground going from under it
            float cs = std::cos(it.ang), sn = std::sin(it.ang), cx = it.x, cy = it.y - hb.y;
            bool held = false;
            for (float x = -hb.x; x <= hb.x && !held; x += 2.0f)
                for (float y : {-hb.y, hb.y})
                    held = held || isSolid((int)std::floor(cx + x * cs - y * sn), (int)std::floor(cy + x * sn + y * cs + 1.2f));
            if (held) continue;
            it.rest = 0;
        }
        chestPhysics(it);
    }
    for (auto& it : G.inter) // what's solid to walkers this frame
    {
        if (it.type != IT_PROP || it.used || std::fabs(it.x - G.camX - G.vw / 2) > G.vw + 300 || std::fabs(it.y - G.camY - G.vh / 2) > G.vh + 300) continue;
        Vector2 hb = bodyHalf(it);
        float cs = std::fabs(std::cos(it.ang)), sn = std::fabs(std::sin(it.ang)), ex = hb.x * cs + hb.y * sn, ey = hb.x * sn + hb.y * cs;
        propBoxes.push_back({it.x - ex, it.y - hb.y - ey, ex * 2, ey * 2});
    }
}

static void updateLanterns()
{
    for (auto& it : G.inter)
    {
        if (it.type != IT_LANTERN || it.used || std::fabs(it.x - G.camX - G.vw / 2) > G.vw + 150 || std::fabs(it.y - G.camY - G.vh / 2) > G.vh + 150) continue;
        Vector2 p = lanternPos(it);
        for (auto& pr : G.projs) // a bolt or a spell through it
            if (pr.alive && std::fabs(pr.x - p.x) < 3.5f && std::fabs(pr.y - p.y - 4) < 4.5f) { hitLantern(it, pr.vx > 0 ? 1.2f : -1.2f); if (it.style) pr.alive = false; break; }
        if (it.used) continue;
        if (!it.style) // a pendulum on its chain; anything walking through sets it swinging
        {
            float L = it.data + 4.0f; // out to the lantern's middle
            it.va += -0.2f / L * std::sin(it.ang);
            auto body = [&](const Mob& m) { // a body can't pass through it: it's shoved aside, and swings off
                float lx = it.x + std::sin(it.ang) * L, ly = it.y + std::cos(it.ang) * L;
                if (!m.alive || lx < m.x - 2.5f || lx > m.x + m.w + 2.5f || ly + 3 < m.y || ly - 3 > m.y + m.h) return;
                float tx = lx < m.cx() ? m.x - 2.6f : m.x + m.w + 2.6f;
                float na = std::asin(clampf((tx - it.x) / L, -0.97f, 0.97f));
                it.va = clampf((na - it.ang) * 0.5f + m.vx * 0.02f / L, -0.25f, 0.25f);
                it.ang = na;
            };
            body(G.p.m);
            for (auto& m : G.mobs) if (std::fabs(m.x - p.x) < 30) body(m);
            it.va *= 0.996f;
            it.ang += it.va;
            float lx = it.x + std::sin(it.ang) * L, ly = it.y + std::cos(it.ang) * L;
            if (isSolid((int)std::floor(lx + (it.va > 0 ? 2 : -2) * std::cos(it.ang)), (int)std::floor(ly))) // swung into a wall
            {
                if (std::fabs(it.va) * L > 1.3f) { it.x = lx; it.y = ly - 4; it.style = 1; it.vx = it.vy = 0; smashLantern(it); continue; }
                it.ang -= it.va;
                it.va *= -0.4f;
                playAt(SFX_KNOCK, lx, ly, 0.3f, 1.8f);
            }
            continue;
        }
        it.vy = std::min(it.vy + 0.2f, 5.0f); // loose: it flies, spinning, till it hits something
        it.x += it.vx; it.y += it.vy; it.ang += it.va;
        int fx = (int)std::floor(it.x), fy = (int)std::floor(it.y);
        if (isSolid(fx, fy + 2) || (it.vy > 0 && world.matU(fx, fy + 2) == M::Platform) || isSolid(fx + (it.vx > 0 ? 2 : -2), fy) || isLiquidAt(fx, fy) || !world.inU(fx, fy + 2)) smashLantern(it);
    }
}

// Pause-menu escape hatch: if you ever get stuck, walk back to the nearest torch on the road.
void returnToRoad()
{
    Mob& m = G.p.m;
    float bd = 1e9f, tx = -1, ty = 0;
    for (auto& it : G.inter)
        if (it.type == IT_TORCH && it.x > G.playX0 + HAVEN_WALL) // never back behind a gate you came through
        {
            float d = std::hypot(it.x - m.cx(), it.y - m.cy());
            if (d < bd) { bd = d; tx = it.x; ty = it.y; }
        }
    if (tx < 0 || !m.alive) return;
    int fee = G.p.coins / 10;
    G.p.coins -= fee;
    m.x = tx + 6 - m.w / 2.0f;
    m.y = ty - m.h - 1;
    m.vx = m.vy = 0;
    G.p.hook = 0;
    message("You find your way back to the road" + (fee ? std::string(" (dropped ") + std::to_string(fee) + " coins)" : std::string(".")));
}

static void sandboxTools()
{
    static const M brush[] = {M::Sand, M::Water, M::Stone, M::Lava, M::Steam, M::Dirt, M::Wood, M::Oil, M::Gunpowder, M::Keg,
                              M::Acid, M::Ice, M::Snow, M::Fire, M::Miasma, M::Gravel, M::Glass, M::CopperOre, M::IronOre,
                              M::GoldOre, M::Firestone, M::Adamantite, M::Metal, M::Spikes};
    const int nb = (int)(sizeof(brush) / sizeof(brush[0]));
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) G.brushMat = (G.brushMat + 1) % nb;
    if (IsKeyPressed(KEY_LEFT_BRACKET)) G.brushMat = (G.brushMat + nb - 1) % nb;
    if (IsKeyPressed(KEY_MINUS) && G.brushR > 0) G.brushR--;
    if (IsKeyPressed(KEY_EQUAL) && G.brushR < 20) G.brushR++;
    Vector2 mw = mouseWorld();
    if (IsKeyPressed(KEY_E))
    {
        Mob e = makeEnemy(irand(E_BLACKKNIGHT), mw.x, mw.y);
        G.mobs.push_back(e);
    }
    if (IsKeyPressed(KEY_C)) // a chest, dropped from the mouse
    {
        Interact c{IT_CHEST, mw.x, mw.y + CHEST_HH};
        c.rest = 0;
        c.va = frange(-0.03f, 0.03f);
        G.inter.push_back(c);
    }
    if (!IsKeyDown(KEY_LEFT_CONTROL)) return;
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) paintCircle((int)mw.x, (int)mw.y, G.brushR, brush[G.brushMat], false);
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) paintCircle((int)mw.x, (int)mw.y, G.brushR, M::Empty, false);
}

const char* sandboxBrushName()
{
    static const char* names[] = {"Sand", "Water", "Stone", "Lava", "Steam", "Dirt", "Wood", "Oil", "Gunpowder", "Powder Keg",
                                  "Acid", "Ice", "Snow", "Fire", "Miasma", "Gravel", "Glass", "Copper Ore", "Iron Ore",
                                  "Gold Ore", "Firestone", "Adamantite", "Iron Plate", "Spikes"};
    return names[G.brushMat % 24];
}

// ================================================================ main update

static void updateCamera()
{
    Mob& pm = G.p.m;
    float tx = pm.cx() - G.vw / 2.0f, ty = pm.cy() - G.vh / 2.0f;
    G.camX += (tx - G.camX) * 0.12f;
    G.camY += (ty - G.camY) * 0.12f;
    if (world.wU() <= G.vw) G.camX = (world.wU() - G.vw) / 2.0f;
    else G.camX = clampf(G.camX, 0, (float)(world.wU() - G.vw));
    if (world.hU() <= G.vh) G.camY = (world.hU() - G.vh) / 2.0f;
    else G.camY = clampf(G.camY, 0, (float)(world.hU() - G.vh));
    syncRenderCamera();
    G.shake *= 0.88f;
}

// ---------------------------------------------------------------- the Whispering Dunes: tumbleweeds and blown sand

struct Tumble { float x, y, vx, vy, rot; int r; };
static std::vector<Tumble> tumbles;

static float windGust() { return 0.6f + 0.5f * std::sin(G.frame * 0.004f) + 0.25f * std::sin(G.frame * 0.017f); }

static void updateDunes()
{
    if (!G.duneEnd) { tumbles.clear(); return; }
    float gust = windGust();
    if (tumbles.size() < 3 && G.camX < G.duneEnd && G.camX > G.seaEnd && chance(140)) // (not out over the sea)
    {
        Tumble t{G.camX - 10, G.camY + G.vh * frange(0.15f, 0.45f), frange(0.5f, 1.0f), 0, 0, irange(3, 5)};
        for (int k = 0; k < 200 && isSolid((int)t.x, (int)t.y + t.r); k++) t.y -= 1;
        tumbles.push_back(t);
    }
    for (auto& t : tumbles)
    {
        t.vx += (gust * 1.4f - t.vx) * 0.02f;
        t.vy = std::min(t.vy + 0.12f, 3.0f);
        float nx = t.x + t.vx, ny = t.y + t.vy;
        if (isSolid((int)nx, (int)ny + t.r))
        {
            int k = 0;
            while (k < 5 && isSolid((int)nx, (int)ny + t.r)) { ny -= 1; k++; } // roll up the slope
            if (k >= 5) { nx = t.x; t.vx *= -0.3f; }
            t.vy = chance(20) ? -frange(1.2f, 2.4f) : -std::fabs(t.vy) * 0.25f; // bounce, and now and then a hop
        }
        t.x = nx;
        t.y = ny;
        t.rot += t.vx / t.r;
    }
    tumbles.erase(std::remove_if(tumbles.begin(), tumbles.end(), [](const Tumble& t) {
        return t.x > G.duneEnd + 60 || t.x > G.camX + G.vw + 30 || t.x < G.camX - 80 || t.y > world.hU();
    }), tumbles.end());
    if (!inDunes()) return;
    for (int k = 0; k < 2; k++) // sand streaming off the crests
    {
        int x = (int)G.camX + irand(G.vw), y = (int)G.camY;
        while (y < G.camY + G.vh && !isSolid(x, y)) y++;
        if (y >= G.camY + G.vh || world.matU(x, y) != M::Sand || !chance(2)) continue;
        Color c = {(unsigned char)irange(196, 236), (unsigned char)irange(168, 206), 120, (unsigned char)irange(90, 170)};
        spawnParticle((float)x, y - frange(0.5f, 3), gust * frange(1.2f, 2.2f), frange(-0.25f, 0.05f), irange(40, 90), c, 0.004f);
    }
}

static void drawTumbleweeds(int camX, int camY)
{
    for (auto& t : tumbles)
    {
        float cx = t.x - camX, cy = t.y - camY;
        for (int k = 0; k < 14; k++) // a ball of tangled stems: chords across a wobbly circle
        {
            float a1 = t.rot + k * 2.4f, a2 = a1 + 1.9f + hash2(k, t.r, 5);
            float r1 = t.r * (0.75f + 0.3f * hash2(k, 1, t.r)), r2 = t.r * (0.75f + 0.3f * hash2(k, 2, t.r));
            DrawLineEx({cx + std::cos(a1) * r1, cy + std::sin(a1) * r1}, {cx + std::cos(a2) * r2, cy + std::sin(a2) * r2}, 1,
                       k % 3 ? Color{142, 112, 70, 255} : Color{100, 76, 48, 255});
        }
    }
}

// A Norse longship: clinker hull, a row of shields, dragon prow, and a striped sail (`sail` 0 furled .. 1 set).
static void drawLongship(float x, float y, float sail)
{
    const Color ink = {50, 34, 22, 255}, wood = {112, 74, 42, 255}, woodD = {74, 48, 28, 255}, woodL = {150, 104, 62, 255}; // ink: tarred seams, not an outline
    const int L = 30;
    for (int dx = -L - 1; dx <= L + 1; dx++)
    {
        float u = std::fabs((float)dx) / L;
        int top = (int)(y - 9 - u * u * u * 7), bot = (int)(y - u * u * 7);
        for (int yy = top - 1; yy <= bot + 1; yy++)
        {
            bool edge = yy < top || yy > bot || std::abs(dx) > L;
            Color c = edge ? ink : ((yy - top) % 3 == 2 ? woodD : (yy == top ? woodL : wood)); // overlapping strakes
            DrawRectangle((int)x + dx, yy, 1, 1, c);
        }
    }
    auto stroke = [&](float x0, float y0, float x1, float y1, float th, Color c) { DrawLineEx({x0, y0}, {x1, y1}, th, c); };
    stroke(x + L, y - 15, x + L + 3, y - 25, 2, wood); // the dragon's neck, and its head looking out to sea
    DrawRectangle((int)x + L + 1, (int)y - 29, 7, 4, ink);
    DrawRectangle((int)x + L + 2, (int)y - 28, 5, 2, woodL);
    DrawRectangle((int)x + L + 5, (int)y - 28, 1, 1, {220, 60, 40, 255});
    stroke(x - L, y - 15, x - L - 2, y - 22, 2, wood); // the stern curls back over itself
    stroke(x - L - 2, y - 22, x - L + 1, y - 24, 1.5f, wood);
    float mx = x - 2;
    stroke(mx, y - 9, mx, y - 46, 1.5f, woodD); // mast and yard
    stroke(mx - 14, y - 44, mx + 14, y - 44, 1, woodD);
    if (sail > 0.02f)
    {
        int sh = (int)(26 * sail);
        for (int yy = 0; yy < sh; yy++)
            for (int dx = -13; dx <= 13; dx++)
            {
                float belly = std::sin(3.14159f * yy / 26.0f) * 2 * sail; // filled with wind
                Color c = ((dx + 13) / 5) % 2 ? Color{232, 222, 196, 255} : Color{168, 38, 40, 255};
                if (std::abs(dx) == 13 || yy == sh - 1) c = brighten(c, -60);
                DrawRectangle((int)(mx + dx + belly), (int)y - 43 + yy, 1, 1, c);
            }
    }
    else
    {
        DrawRectangle((int)mx - 13, (int)y - 44, 27, 3, ink);
        DrawRectangle((int)mx - 12, (int)y - 43, 25, 1, {200, 180, 150, 255});
    }
    static const Color shields[4] = {{176, 44, 40, 255}, {222, 184, 70, 255}, {44, 70, 140, 255}, {226, 220, 200, 255}};
    for (int k = 0; k < 7; k++) // shields hung along the gunwale
    {
        float sx = x - L + 9 + k * 7.3f, sy = y - 8;
        DrawCircleV({sx, sy}, 3.4f, ink);
        DrawCircleV({sx, sy}, 2.6f, shields[k % 4]);
        DrawRectangle((int)sx, (int)sy, 1, 1, {140, 140, 150, 255});
    }
}

// ---------------------------------------------------------------- havens

void shiftEntities(float dx, float dy)
{
    Player& P = G.p;
    P.m.x += dx; P.m.y += dy;
    P.hx += dx; P.hy += dy;
    P.wx += dx; P.wy += dy;
    for (int i = 0; i < CAPE_N; i++) { P.cape[i].x += dx; P.cape[i].y += dy; P.capePrev[i].x += dx; P.capePrev[i].y += dy; }
    for (auto& p : G.projs) { p.x += dx; p.y += dy; }
    for (auto& q : G.parts) { q.x += dx; q.y += dy; }
    for (auto& t : G.texts) { t.x += dx; t.y += dy; }
    for (auto& r : G.rags)
        for (int i = 0; i < 9; i++) { r.p[i].x += dx; r.p[i].y += dy; r.pp[i].x += dx; r.pp[i].y += dy; }
    shiftCorpses(dx, dy);
    tumbles.clear();
    G.camX += dx; G.camY += dy;
    syncRenderCamera();
}

// Walking into a haven drops its gate behind you: you're healed, the shrine is stocked for the depths
// ahead, the far gate opens, and the world ahead grows by another biome.
// The storm over Dunmoor: it gathers as you near the castle. Rain out of the open sky, lightning, thunder.
static void updateStorm()
{
    float px = G.p.m.cx(), target = 0;
    if (G.stormX1 > G.stormX0 && !G.inVillage) target = clampf((px - G.stormX0) / (G.stormX1 - G.stormX0), 0, 1);
    world.storm += (target - world.storm) * 0.02f;
    world.flash *= 0.8f;
    if (world.storm < 0.05f) return;
    if (world.storm > 0.45f && irand(260) == 0) // lightning, and the thunder after it
    {
        world.flash = frange(0.6f, 1.0f);
        G.thunderT = irange(15, 80);
    }
    if (G.thunderT > 0 && --G.thunderT == 0) { playSfx(SFX_EXPLODE, 0.25f + 0.3f * world.storm, frange(0.3f, 0.45f)); G.shake = std::max(G.shake, 1.5f); }
    for (int k = (int)(world.storm * 7); k > 0; k--) // rain, slanting, out of open sky only
    {
        float x = G.camX + frange(-20, G.vw + 40), y = G.camY + frange(-10, G.vh * 0.6f);
        int cx = (int)(x * world.scale), cy = (int)(y * world.scale);
        if (!world.in(cx, cy) || !world.skyOf(cx, cy) || world.get(cx, cy).material != M::Empty) continue;
        spawnParticle(x, y, -0.9f, 4.5f, 70, {150, 160, 190, 150}, 0.12f);
    }
}

static void updateHavens()
{
    Mob& pm = G.p.m;
    G.sanctuary = false;
    for (auto& h : G.havens)
    {
        bool inside = pm.cx() > h.x0 && pm.cx() < h.x1 && pm.cy() > h.top && pm.cy() < h.floor;
        if (inside) G.sanctuary = true;
        if (h.sealed || h.locked || !inside || pm.cx() < h.x0 + 48 || !pm.alive) continue;
        h.sealed = true;
        G.stage = h.stage + 1;
        G.bannerTimer = 240;
        message(std::string(h.name) + ". The land changes ahead." + (STAGES[G.stage].boss >= 0 ? " A guardian bars the way on, somewhere in these depths..." : ""));
        G.playX0 = h.x0;
        advanceWorld(h); // rebuilds G.havens: stop iterating
        return;
    }
}

void updateGame()
{
    if (G.hitstop > 0) { G.hitstop--; return; } // freeze frames sell the impact
    G.frame++;
    Mob& pm = G.p.m;

    if (G.sailT > 0) // aboard the longship: no control, just the voyage out (and a fade to black)
    {
        G.sailT++;
        for (auto& it : G.inter)
            if (it.type == IT_BOAT && !it.used)
            {
                it.x += std::min(1.4f, G.sailT * 0.012f);
                pm.x = it.x - 4 - pm.w / 2.0f;
                pm.y = it.y - 8 - pm.h;
            }
        pm.vx = pm.vy = 0;
        pm.facing = 1;
        simulate(((int)G.camX - 100) * world.scale, ((int)G.camY - 100) * world.scale, ((int)G.camX + G.vw + 100) * world.scale, ((int)G.camY + G.vh + 100) * world.scale);
        updateParticles();
        updateCamera();
        if (G.sailT > 180) { G.sailT = 0; travelOnward(); }
        return;
    }

    if (G.sandbox) sandboxTools();
    updatePlayer();
    if (pm.alive && pm.hp <= 0 && G.p.amulet == AM_VALKNUT) // Odin isn't done with you
    {
        wearAmulet(-1);
        pm.hp = pm.maxHp * 0.5f;
        pm.iframes = 120;
        G.shake = 10;
        playSfx(SFX_PORTAL, 0.8f, 0.7f);
        for (int k = 0; k < 60; k++) spawnParticle(pm.cx(), pm.cy(), frange(-2.5f, 2.5f), frange(-3, 0.5f), irange(30, 70), {220, 220, 240, 255}, 0.02f);
        message("The Valknut breaks. Odin sends you back - this once.");
    }
    if (pm.alive && pm.hp <= 0)
    {
        pm.alive = false;
        G.deadTimer = 120;
        for (int i = 0; i < 20; i++) spawnCellParticle(pm.cx(), pm.cy(), frange(-2, 2), frange(-3, 0), M::Blood, 0);
        ragdollForPlayer();
        bankRun();
        G.p.hook = 0;
    }
    if (!pm.alive && --G.deadTimer <= 0) G.state = GS_DEAD;

    float ax0 = G.camX - 250, ax1 = G.camX + G.vw + 250, ay0 = G.camY - 200, ay1 = G.camY + G.vh + 200;
    for (auto& m : G.mobs)
    {
        if (!m.alive) continue;
        float slack = m.boss ? 400 : 0; // guardians keep watch a little further out, but not across the whole world
        if (m.cx() < ax0 - slack || m.cx() > ax1 + slack || m.cy() < ay0 - slack || m.cy() > ay1 + slack) continue;
        updateStatus(m);
        updateEnemy(m);
        if (m.hp <= 0) killMob(m);
    }
    G.mobs.erase(std::remove_if(G.mobs.begin(), G.mobs.end(), [](const Mob& m) { return !m.alive; }), G.mobs.end());
    for (auto& m : pendingMobs) G.mobs.push_back(m);
    pendingMobs.clear();

    updateProjectiles();
    updatePickups();
    updateTraps();
    updateInteract();
    updateChests();
    updateLanterns();
    if (!G.sandbox && !G.inVillage) updateHavens();
    growDampCaves();

    simulate(((int)G.camX - 100) * world.scale, ((int)G.camY - 100) * world.scale, ((int)G.camX + G.vw + 100) * world.scale, ((int)G.camY + G.vh + 100) * world.scale);
    updateStructures();

    std::vector<Blast> bl;
    bl.swap(world.blasts);
    int n = 0;
    for (auto& b : bl)
    {
        if (n++ < 24) explode((float)b.x / world.scale, (float)b.y / world.scale, b.r, b.dmg, EL_FIRE, false, b.power); // sim blasts come in cells
        else world.blasts.push_back(b);
    }
    updateParticles();
    updateRagdolls();
    updateCorpses();
    updateStorm();
    updateDunes();
    updateVillagers();
    if (G.duneEnd && !G.duneCrossed && pm.x > G.duneEnd) { G.duneCrossed = true; G.bannerTimer = 240; } // the Greenmarch, at last

    for (auto& t : G.texts) { t.life--; t.y -= 0.3f; }
    G.texts.erase(std::remove_if(G.texts.begin(), G.texts.end(), [](const FloatText& t) { return t.life <= 0; }), G.texts.end());
    for (auto& m : G.msgs) m.second--;
    G.msgs.erase(std::remove_if(G.msgs.begin(), G.msgs.end(), [](const std::pair<std::string, int>& m) { return m.second <= 0; }), G.msgs.end());
    if (G.bannerTimer > 0) G.bannerTimer--;
    {
        static int lost = 0; // a gentle reminder if you've been far below the road for a while
        int rf = roadFloorAt((int)pm.cx());
        if (rf >= 0 && pm.alive && pm.y + pm.h > rf + 70) lost++;
        else lost = 0;
        if (lost > 0 && lost % 1200 == 0) message("Lost? Press Esc, then R to find your way back to the road.");
    }
    if (G.winTimer > 0 && --G.winTimer == 0) { bankRun(); G.state = GS_WIN; }

    updateCamera();
}

// ================================================================ drawing (render-texture space)

// A Norse market stall in Hearthwick: lashed posts, a linen awning, a rope of hanging goods and a counter
// under a woven runner. `kind` is the shop: 0 weaponsmith, 1 arcanist, 2 outfitter. Drawn in layers: 0 the
// back (behind the merchant), 1 the counter and its wares, 2 what moves or is already fine (weapons on
// show, flames, glows). Layers 0 and 1 are drawn once, upscaled, and reused (prepareStallArt).
static void drawStall(float fx, float fy, int kind, int layer)
{
    int x = (int)fx, y = (int)fy;
    auto R = [&](int x0, int y0, int w, int h, Color c) { DrawRectangle(x + x0, y + y0, w, h, c); };
    const Color post = {104, 70, 40, 255}, postD = {66, 44, 26, 255}, rope = {196, 166, 110, 255}, linen = {206, 196, 170, 255},
                linenD = {160, 150, 128, 255}, back = {40, 33, 28, 255}, plank = {120, 82, 48, 255}, plankD = {76, 52, 32, 255};
    static const Color accent[3] = {{164, 40, 34, 255}, {96, 60, 150, 255}, {44, 96, 120, 255}};
    const Color acc = accent[kind % 3];
    float fl = 0.85f + 0.15f * hash2(x, G.frame / 4, 3);

    if (layer == 0)
    {
    R(-22, -36, 44, 24, back); // the wool hanging behind
    for (int k = 0; k < 44; k += 5) R(-22 + k, -36, 1, 24, {48, 40, 34, 255});
    for (int sd : {-1, 1}) // posts, their tops crossed and lashed
    {
        int px = sd < 0 ? -25 : 22;
        R(px, -46, 3, 46, post);
        R(px + 2, -46, 1, 46, postD);
        DrawLine(x + px - 2, y - 50, x + px + 5, y - 43, post);
        DrawLine(x + px + 5, y - 50, x + px - 2, y - 43, post);
        R(px, -47, 3, 1, rope); R(px, -45, 3, 1, rope);
    }
    R(-24, -40, 48, 3, post); // the beam
    R(-24, -38, 48, 1, postD);
    for (int lx : {-12, 0, 12}) R(lx, -41, 2, 5, rope);
    for (int k = -22; k < 22; k++) // the awning, sagging between its ties
    {
        int sag = (int)(std::sin((k + 22) * PI / 11) * 1.2f + 1.2f);
        R(k, -45, 1, 5 + sag, (k + 22) % 11 < 2 ? linenD : linen);
        if ((k + 22) % 4 < 2) R(k, -40 + sag, 1, 2, acc); // a coloured hem
    }

    // the rope of hanging goods
    for (int k = -19; k <= 19; k++) R(k, -33 + (int)(1.5f * (1 - (k / 19.0f) * (k / 19.0f))), 1, 1, rope);
    for (int i = 0; i < 6; i++)
    {
        int hx = -16 + i * 6, hy = -32 + (int)(1.5f * (1 - (hx / 19.0f) * (hx / 19.0f)));
        R(hx, hy, 1, 2, rope);
        if (kind == 0) // the smith's ironmongery: horseshoes, tongs and hammers
        {
            const Color iron = {120, 122, 132, 255}, ironD = {78, 80, 90, 255};
            if (i % 3 == 0) { R(hx - 1, hy + 2, 3, 1, iron); R(hx - 1, hy + 3, 1, 3, iron); R(hx + 1, hy + 3, 1, 3, ironD); }
            else if (i % 3 == 1) { R(hx - 1, hy + 2, 1, 6, iron); R(hx + 1, hy + 2, 1, 6, ironD); R(hx - 1, hy + 3, 3, 1, ironD); }
            else { R(hx, hy + 2, 1, 5, {90, 58, 32, 255}); R(hx - 1, hy + 7, 3, 2, iron); }
        }
        else if (kind == 1) // dried herbs, charms and rune-bones
        {
            Color c = i % 3 == 0 ? Color{110, 140, 70, 255} : (i % 3 == 1 ? Color{222, 214, 190, 255} : Color{170, 120, 255, 255});
            R(hx - 1, hy + 2, 3, 4, c); R(hx, hy + 6, 1, 2, brighten(c, -40));
            if (i % 3 == 2) R(hx, hy + 3, 1, 1, WHITE);
        }
        else // pelts, rope coils and a net
        {
            Color c = i % 3 == 0 ? Color{128, 92, 60, 255} : (i % 3 == 1 ? rope : Color{160, 150, 128, 255});
            R(hx - 1, hy + 2, 3, 5 + i % 2, c); R(hx - 1, hy + 4, 3, 1, brighten(c, -40));
        }
    }

    if (kind == 1) // the ox skull and banner
    {
        for (int sd : {-1, 1}) DrawLine(x - 25 + sd * 3, y - 50, x - 25 + sd * 6, y - 54, {222, 214, 190, 255}); // horns
        R(-27, -50, 6, 5, {222, 214, 190, 255}); R(-26, -48, 1, 1, {30, 26, 26, 255}); R(-23, -48, 1, 1, {30, 26, 26, 255}); R(-26, -45, 4, 2, {200, 192, 168, 255});
        R(-29, -42, 6, 16, acc); for (int k = 0; k < 6; k += 2) R(-29 + k, -26, 1, 2, acc);
        R(-27, -38, 2, 2, linen); R(-27, -33, 2, 2, linen);
    }
    if (kind == 0) // the shield on the post
    {
        DrawCircle(x + 23, y - 30, 7, {80, 76, 72, 255});
        DrawCircle(x + 23, y - 30, 6, {164, 40, 34, 255});
        DrawLine(x + 17, y - 30, x + 29, y - 30, {214, 200, 170, 255});
        DrawLine(x + 23, y - 36, x + 23, y - 24, {214, 200, 170, 255});
        DrawCircle(x + 23, y - 30, 1.6f, {170, 170, 176, 255});
    }
    R(-21, -30, 1, 3, {60, 60, 64, 255}); R(-22, -27, 3, 4, {60, 60, 64, 255}); // the lantern on the left post
    // the shop sign, hung off an arm from the right post: a sword, a rune or a boot
    R(24, -46, 12, 1, post);
    R(27, -45, 1, 2, rope); R(34, -45, 1, 2, rope);
    R(26, -43, 10, 6, plank); R(26, -43, 10, 1, brighten(plank, 24)); R(26, -38, 10, 1, plankD);
    if (kind == 0) { R(30, -42, 1, 3, {206, 210, 218, 255}); R(29, -40, 3, 1, {215, 184, 77, 255}); R(30, -39, 1, 1, {90, 58, 32, 255}); }
    else if (kind == 1) { R(29, -42, 1, 4, {190, 140, 255, 255}); R(30, -42, 2, 1, {190, 140, 255, 255}); R(31, -41, 1, 1, {190, 140, 255, 255}); R(30, -40, 1, 1, {190, 140, 255, 255}); R(31, -39, 1, 1, {190, 140, 255, 255}); }
    else { R(29, -42, 2, 3, {150, 100, 56, 255}); R(29, -40, 4, 2, {150, 100, 56, 255}); R(29, -39, 4, 1, {90, 58, 32, 255}); }
    for (int k = -24; k < 24; k += 3) R(k, -1, 2, 1, k % 2 ? acc : Color{196, 160, 80, 255}); // a woven mat at its foot
    return;
    }

    if (layer == 1)
    {
    // the counter, under a woven runner with a fringe
    R(-19, -14, 38, 2, plank);
    R(-19, -12, 38, 11, plankD);
    for (int k = -19; k < 19; k += 6) R(k, -12, 1, 11, {60, 40, 24, 255});
    R(2, -14, 14, 10, acc);
    for (int k = 0; k < 14; k += 2) R(2 + k, -4, 1, 2, acc);
    for (int k = 0; k < 4; k++) { R(5 + k * 2, -11 + (k % 2) * 2, 2, 1, linen); R(13 - k * 2, -11 + (k % 2) * 2, 2, 1, linen); }

    switch (kind)
    {
    case 0: // weaponsmith: a barrel for the spear, and a grindstone (the wares themselves are drawn live, finer)
        R(26, -9, 8, 9, {110, 72, 40, 255}); R(26, -7, 8, 1, {70, 70, 76, 255}); R(26, -3, 8, 1, {70, 70, 76, 255});
        R(-35, -5, 9, 2, {92, 62, 38, 255}); R(-34, -3, 1, 3, {92, 62, 38, 255}); R(-28, -3, 1, 3, {92, 62, 38, 255});
        DrawCircle(x - 31, y - 9, 4.5f, {130, 124, 118, 255}); DrawCircle(x - 31, y - 9, 3.0f, {150, 144, 136, 255});
        R(-31, -9, 1, 1, {60, 58, 56, 255}); R(-31, -9, 5, 1, {92, 62, 38, 255}); // its axle and crank
        break;
    case 1: // arcanist: staves in an urn, scrolls and a candle on the counter
    {
        R(25, -8, 8, 8, {96, 80, 70, 255}); R(25, -8, 8, 1, {130, 110, 96, 255}); // urn of staves
        static const Color gems[3] = {{120, 200, 255, 255}, {200, 120, 255, 255}, {255, 140, 60, 255}};
        for (int k = 1; k < 3; k++) // old staves (the one for sale is drawn live)
        {
            int sx = 27 + k * 2, top = -26 - k * 3;
            DrawLine(x + sx, y - 8, x + sx + k - 1, y + top, {150, 116, 76, 255});
            R(sx + k - 2, top - 1, 3, 3, gems[k]);
        }
        R(-2, -18, 2, 4, {236, 230, 214, 255}); // a candle
        for (int k = 0; k < 3; k++) R(-35 + k * 3, -4 - k % 2 * 2, 2, 4 + k % 2 * 2, {226, 218, 196, 255}); // more on the ground
        DrawCircle(x + 13, y - 17, 2.5f, {120, 150, 210, 200}); R(11, -15, 5, 1, {80, 60, 50, 255}); // a seeing-stone on its stand
        break;
    }
    default: // outfitter: a flask, a folded cloak, a basket of wool, and a jar of light
    {
        R(-17, -18, 6, 4, {44, 70, 120, 255}); R(-17, -20, 6, 2, {164, 40, 34, 255}); // folded cloth
        R(-31, -6, 9, 6, {150, 110, 60, 255}); R(-31, -6, 9, 1, {110, 80, 40, 255}); // a basket of wool
        DrawCircle(x - 29, y - 7, 2, {164, 40, 34, 255}); DrawCircle(x - 25, y - 7, 2, {44, 70, 120, 255});
        break;
    }
    }
    return;
    }

    // layer 2: what's for sale, drawn as the real thing - an empty peg once it's yours - and every flame and glow
    static const float WPOS[6][3] = {{-18, -15, -PI / 2}, {4, -15, 0}, {30, -8, -PI / 2}, {-12, -15, -PI / 2}, {7, -26, 0}, {-5, -15, -PI / 2}};
    static const int SCROLL_X[5] = {-18, -13, -8, 4, 9};
    for (int i = 0, slot = 0; i < UNLOCK_COUNT; i++)
    {
        const Unlock& un = UNLOCKS[i];
        if (un.shop != kind) continue;
        int s = slot++;
        if (META.stocked[i] || META.owned[i]) continue;
        switch (un.kind)
        {
        case UK_WEAPON:
        {
            Weapon w; w.type = un.a; w.metal = un.b;
            const float* p = WPOS[std::min(s, 5)];
            drawWeaponSprite(w, {fx + p[0], fy + p[1]}, p[2], 0.5f, false);
            break;
        }
        case UK_STAFF: { Weapon w; w.type = W_STAFF; w.staff.gem = SKYBLUE; drawWeaponSprite(w, {fx + 27, fy - 8}, -PI / 2, 0.5f, false); break; }
        case UK_SPELL: // a scroll, sealed in the spell's colour
        {
            int sx = SCROLL_X[std::min(std::max(s - 1, 0), 4)];
            R(sx, -17, 4, 3, {226, 214, 180, 255}); R(sx, -17, 4, 1, {246, 238, 214, 255});
            R(sx + 1, -17, 1, 3, SPELLS[un.a].col);
            break;
        }
        case UK_HOOK: DrawCircleLines(x + 24, y - 28, 3, {150, 150, 160, 255}); R(23, -34, 2, 4, {150, 150, 160, 255}); break;
        case UK_ARMOUR: // a mail shirt hung on the back
        {
            Color mc = METALS[un.a].color;
            for (int yy = -33; yy < -20; yy++)
                for (int xx = -9; xx <= 1; xx++)
                {
                    bool arm = yy < -29 ? true : (xx > -8 && xx < 0);
                    if (!arm) continue;
                    R(xx, yy, 1, 1, brighten(mc, ((xx + yy) & 1) ? 18 : -22)); // the rings catch the light
                }
            R(-5, -34, 3, 1, {90, 58, 32, 255}); // its peg
            break;
        }
        case UK_FLASK: R(-8, -19, 3, 5, {214, 52, 52, 255}); R(-7, -21, 1, 2, {200, 196, 176, 255}); R(-8, -18, 1, 2, {255, 140, 140, 255}); break;
        case UK_WISP: // Baldr's light, caught in a jar
            R(26, -9, 6, 9, {150, 168, 178, 255}); R(26, -10, 6, 1, {110, 80, 40, 255});
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient(x + 29, y - 5, 7 * fl, {255, 225, 150, 120}, {255, 225, 150, 0});
            EndBlendMode();
            R(28, -6, 2, 2, {255, 240, 190, 255});
            break;
        }
    }
    if (kind == 1) R(-2, -20, 2, 2, {255, (unsigned char)(200 * fl), 80, 255}); // the candle's flame
    R(-21, -26, 1, 2, {255, (unsigned char)(210 * fl), 110, 255}); // the lantern's flame
}

// The still parts of each stall, drawn once at a pixel per unit, then doubled and shaded like the sprites.
static Texture2D stallArt[3][2];
static const int STALL_W = 72, STALL_H = 62, STALL_OX = 36, STALL_OY = 58; // canvas, and where the stall's foot sits in it

void prepareStallArt()
{
    if (!G.inVillage || stallArt[2][1].id) return;
    RenderTexture2D canvas = LoadRenderTexture(STALL_W, STALL_H);
    for (int kind = 0; kind < 3; kind++)
        for (int layer = 0; layer < 2; layer++)
        {
            BeginTextureMode(canvas);
            ClearBackground(BLANK);
            drawStall(STALL_OX, STALL_OY, kind, layer);
            EndTextureMode();
            Image img = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&img);
            ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            std::vector<Color> fine;
            detail2x((const Color*)img.data, STALL_W, STALL_H, fine);
            UnloadImage(img);
            Image out = {fine.data(), STALL_W * 2, STALL_H * 2, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            stallArt[kind][layer] = LoadTextureFromImage(out);
            SetTextureFilter(stallArt[kind][layer], TEXTURE_FILTER_POINT);
        }
    UnloadRenderTexture(canvas);
}

static void drawStallLayer(float x, float y, int kind, int layer)
{
    const Texture2D& t = stallArt[kind][layer];
    if (!t.id) { drawStall(x, y, kind, layer); return; } // not built yet: draw it plainly
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {std::floor(x) - STALL_OX, std::floor(y) - STALL_OY, (float)STALL_W, (float)STALL_H}, {0, 0}, 0, WHITE);
}



void drawEntities(int camX, int camY)
{
    // off screen (x, y relative to the camera): skipped, or a long run's whole world gets drawn every frame
    auto off = [](float x, float y, float m) { return x < -m || y < -m || x > G.vw + m || y > G.vh + m; };
    // interactables
    for (auto& it : G.inter)
    {
        float x = it.x - camX, y = it.y - camY;
        if (it.type == IT_ROPE ? off(x, 0, 8) || y > G.vh || it.data - camY < 0 : off(x, y, it.type == IT_BOAT ? 160 : 96)) continue;
        switch (it.type)
        {
        case IT_CHEST:
        {
            drawChest(x, y - CHEST_HH, it.ang, it.used);
            break;
        }
        case IT_ANVIL: drawSpriteBig(SPR_ANVIL, x, y, false, WHITE); break;
        case IT_SHRINE:
        {
            drawSpriteBig(SPR_SHRINE, x, y, false, WHITE);
            Color orb = it.used ? Color{90, 90, 100, 255} : ColorFromHSV((float)(G.frame % 360), 0.5f, 1.0f);
            float oy = y - SPR_SHRINE.h * 2 - 6 + std::sin(G.frame * 0.05f) * 1.5f;
            if (!it.used) { BeginBlendMode(BLEND_ADDITIVE); DrawCircleGradient((int)x, (int)oy, 16, {orb.r, orb.g, orb.b, 110}, {orb.r, orb.g, orb.b, 0}); EndBlendMode(); }
            DrawCircle((int)x, (int)oy, 4.5f, orb);
            break;
        }
        case IT_PROP:
        {
            Vector2 hb = bodyHalf(it);
            float cx = x, cy = y - hb.y, cs = std::cos(it.ang), sn = std::sin(it.ang);
            auto part = [&](float ox, float oy, float w, float h, Color c) { // a piece of it, turned with it (offsets from the centre)
                DrawRectanglePro({cx + ox * cs - oy * sn, cy + ox * sn + oy * cs, w, h}, {0, 0}, it.ang * RAD2DEG, c);
            };
            if (it.used) break;
            float W = hb.x * 2, H = hb.y * 2;
            if (it.style % 3 == 1) // a barrel: staves, two iron hoops, a darker rim
            {
                part(-hb.x, -hb.y, W, H, {84, 54, 30, 255});
                part(-hb.x + 1, -hb.y + 0.5f, W - 2, H - 1, {128, 84, 46, 255});
                for (float sx = -hb.x + 2.5f; sx < hb.x - 1; sx += 2) part(sx, -hb.y + 0.5f, 0.5f, H - 1, {96, 62, 34, 255});
                part(-hb.x + 1.5f, -hb.y + 0.5f, 1, H - 1, {156, 108, 62, 255}); // the light catching one side
                for (float hy : {-hb.y + 2, hb.y - 3}) part(-hb.x, hy, W, 1, {58, 58, 64, 255});
            }
            else // a plank crate (or a little box with a lid and a clasp)
            {
                bool box = it.style % 3 == 2;
                part(-hb.x, -hb.y, W, H, {72, 48, 28, 255});
                part(-hb.x + 1, -hb.y + 1, W - 2, H - 2, box ? Color{126, 82, 44, 255} : Color{150, 108, 62, 255});
                if (box) { part(-hb.x, -hb.y + 2, W, 0.7f, {72, 48, 28, 255}); part(-0.5f, -hb.y + 1.5f, 1, 2, {210, 176, 90, 255}); }
                else
                {
                    for (float py = -hb.y + 4; py < hb.y - 1; py += 4) part(-hb.x + 1, py, W - 2, 0.5f, {112, 78, 44, 255});
                    DrawLineEx({cx + (-hb.x + 1) * cs - (hb.y - 1) * sn, cy + (-hb.x + 1) * sn + (hb.y - 1) * cs},
                               {cx + (hb.x - 1) * cs - (-hb.y + 1) * sn, cy + (hb.x - 1) * sn + (-hb.y + 1) * cs}, 1.2f, {92, 62, 34, 255}); // the brace
                }
            }
            if (it.data == 1) { part(-hb.x * 0.5f, -hb.y + 1, 0.8f, hb.y, {30, 20, 12, 255}); part(-hb.x * 0.5f, 0, hb.x * 0.6f, 0.8f, {30, 20, 12, 255}); } // splitting
            break;
        }
        case IT_LANTERN:
        {
            if (it.used) break;
            Vector2 lp = lanternPos(it);
            float lx = lp.x - camX, ly = lp.y - camY, a = it.style ? it.ang : -it.ang, cs = std::cos(a), sn = std::sin(a);
            if (!it.style) // the chain, link by link
                for (int k = 0; k < it.data; k++)
                {
                    float t = k / (float)it.data;
                    DrawRectangle((int)std::floor(x + (lx - x) * t), (int)std::floor(y + (ly - y) * t), 1, 1, k % 2 ? Color{92, 92, 100, 255} : Color{52, 52, 58, 255});
                }
            auto part = [&](float ox, float oy, float w, float h, Color c) { // a piece of the lantern, turned with it
                DrawRectanglePro({lx + ox * cs - oy * sn, ly + ox * sn + oy * cs, w, h}, {0, 0}, a * RAD2DEG, c);
            };
            float fl = 0.85f + 0.15f * hash2((int)it.x, G.frame / 4, 9);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)lx, (int)ly + 4, 9 * fl, {255, 160, 70, 60}, {255, 160, 70, 0});
            EndBlendMode();
            part(-0.5f, 0, 1, 1, {70, 70, 76, 255});       // ring
            part(-2, 1, 4, 1, {46, 46, 52, 255});          // cap
            part(-1.5f, 2, 3, 4, {(unsigned char)(255 * fl), (unsigned char)(196 * fl), 110, 255}); // the flame behind the glass
            part(-0.5f, 3, 1, 2, {255, 244, 200, 255});
            part(-2, 2, 0.6f, 4, {40, 40, 46, 255});       // iron frame
            part(1.4f, 2, 0.6f, 4, {40, 40, 46, 255});
            part(-2.5f, 6, 5, 1, {58, 58, 64, 255});       // base
            break;
        }
        case IT_TORCH:
        {
            DrawRectangle((int)x - 1, (int)y - 11, 2, 11, {92, 60, 34, 255});
            DrawRectangle((int)x - 2, (int)y - 12, 4, 2, {70, 70, 76, 255});
            float fl = hash2((int)x, G.frame / 4, 9);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)x, (int)y - 15, 22 + fl * 3, {255, 140, 50, 70}, {255, 140, 50, 0});
            EndBlendMode();
            DrawRectangle((int)x - 1, (int)y - 16 - (fl > 0.5f), 3, 4, {255, 120, 30, 255});
            DrawRectangle((int)x - (fl > 0.7f ? 1 : 0), (int)y - 15, 2, 2, {255, 230, 120, 255});
            if (fl > 0.9f) spawnParticle(x + camX + 0.5f, y + camY - 17, frange(-0.2f, 0.2f), -0.5f, 24, {255, 170, 60, 255}, -0.005f);
            break;
        }
        case IT_SHOP:
            drawStallLayer(x, y, it.data, 0);
            drawSpriteNative(SPR_MERCHANT, x + 4, y - 7, true);
            drawStallLayer(x, y, it.data, 1);
            drawStall(x, y, it.data, 2);
            break;
        case IT_STONE:
        {
            const Weapon* w = it.used ? nullptr : &G.stoneLoot[it.data];
            const Color ink = {56, 40, 30, 255}, wood = {120, 80, 46, 255}, woodD = {80, 52, 30, 255}, stone = {120, 116, 124, 255};
            auto box = [&](float x0, float y0, float x1, float y1, Color c) {
                DrawRectangle((int)x0, (int)y0, (int)(x1 - x0), (int)(y1 - y0), c);
            };
            Vector2 wc = {x, y - 16}; // where the glow centres
            switch (it.style)
            {
            case DS_TARGET: // the crossbow leaning by an archery butt (the butt itself is painted on the back wall)
                if (w) { drawGlow(x - 3, y - 8, w->glow, 16); drawWorldWeapon(*w, {x - 5, y - 1}, -PI / 2 + 0.35f, 11); }
                wc = {x - 3, y - 8};
                break;
            case DS_TABLE: // a cellar table, the knife left on a cutting board
                box(x - 13, y - 10, x + 13, y - 8, wood);
                box(x - 11, y - 8, x - 9, y, woodD);
                box(x + 9, y - 8, x + 11, y, woodD);
                box(x - 6, y - 12, x + 6, y - 10, {170, 130, 80, 255});
                if (w) { drawGlow(x, y - 13, w->glow, 14); drawWorldWeapon(*w, {x - 4, y - 12}, -0.08f, 7); }
                wc = {x, y - 13};
                break;
            case DS_RACK: // a castle weapon rack
                box(x - 12, y - 30, x - 10, y, woodD);
                box(x + 10, y - 30, x + 12, y, woodD);
                box(x - 12, y - 27, x + 12, y - 25, wood);
                box(x - 12, y - 9, x + 12, y - 7, wood);
                if (w) { drawGlow(x, y - 16, w->glow, 22); drawWorldWeapon(*w, {x, y - 6}, -PI / 2, 18); }
                break;
            case DS_GRAVE: // a fresh barrow with a sword driven into it
                DrawEllipse((int)x, (int)y - 1, 13, 5, ink);
                DrawEllipse((int)x, (int)y - 1, 12, 4, {92, 64, 42, 255});
                box(x + 9, y - 20, x + 17, y - 2, stone);
                box(x + 12, y - 18, x + 14, y - 8, {80, 76, 84, 255});
                box(x + 10, y - 15, x + 16, y - 13, {80, 76, 84, 255});
                if (w) { drawGlow(x - 2, y - 14, w->glow, 22); drawWorldWeapon(*w, {x - 2, y - 24}, PI / 2, 20); }
                wc = {x - 2, y - 14};
                break;
            case DS_CART: // an abandoned minecart full of ore
                DrawCircleV({x - 7, y - 3}, 3, ink); DrawCircleV({x + 7, y - 3}, 3, ink);
                DrawCircleV({x - 7, y - 3}, 2, {70, 70, 76, 255}); DrawCircleV({x + 7, y - 3}, 2, {70, 70, 76, 255});
                box(x - 12, y - 14, x + 12, y - 5, {86, 90, 98, 255});
                for (int k = 0; k < 6; k++) DrawRectangle((int)x - 10 + k * 4, (int)y - 16, 3, 3, RES_COLORS[(k * 3) % RES_COUNT]);
                if (w) { drawGlow(x + 2, y - 20, w->glow, 20); drawWorldWeapon(*w, {x + 1, y - 13}, -PI / 2 - 0.45f, 15); }
                wc = {x + 2, y - 20};
                break;
            case DS_ICE: // a spear frozen in a block of ice
                if (w) { drawGlow(x, y - 15, w->glow, 22); drawWorldWeapon(*w, {x - 1, y - 3}, -PI / 2 + 0.1f, 22); }
                DrawRectangle((int)x - 11, (int)y - 29, 23, 29, ink);
                DrawRectangle((int)x - 10, (int)y - 28, 21, 28, {170, 220, 250, 120});
                DrawRectangle((int)x - 8, (int)y - 26, 3, 20, {230, 248, 255, 140});
                break;
            case DS_ANVIL: // laid on an anvil at the heart of the forge
                drawSpriteBig(SPR_ANVIL, x, y, false, WHITE);
                if (w) { drawGlow(x, y - 16, w->glow, 20); drawWorldWeapon(*w, {x - 8, y - 14}, -0.05f, 15); }
                break;
            case DS_ALTAR: // floating above a dark altar
            {
                box(x - 14, y - 12, x + 14, y - 8, stone);
                box(x - 11, y - 8, x - 7, y, {90, 86, 96, 255});
                box(x + 7, y - 8, x + 11, y, {90, 86, 96, 255});
                float bob = std::sin(G.frame * 0.05f) * 1.5f;
                if (w) { drawGlow(x, y - 22 + bob, w->glow, 24); drawWorldWeapon(*w, {x - 8, y - 22 + bob}, 0, 15); }
                wc = {x, y - 22};
                break;
            }
            default:
                DrawEllipse((int)x, (int)y - 4, 12, 7, ink);
                DrawEllipse((int)x, (int)y - 4, 11, 6, stone);
                if (w) { drawGlow(x, y - 16, w->glow, 26); drawWorldWeapon(*w, {x, y - 6}, -PI / 2, 18); }
                break;
            }
            if (w && G.frame % 5 == 0) spawnParticle(wc.x + camX + frange(-8, 8), wc.y + camY + frange(-8, 8), 0, -0.3f, 40, w->glow, -0.002f);
            break;
        }
        case IT_ROPE: // a knotted hemp rope down the shaft
            for (int yy = (int)y; yy < it.data - camY; yy++)
            {
                float sway = std::sin(G.frame * 0.03f + (yy + camY - it.y) * 0.02f) * clampf((yy + camY - it.y) / 120.0f, 0, 1);
                Color c = ((yy + camY) / 3) % 2 ? Color{168, 136, 88, 255} : Color{132, 102, 62, 255};
                if (((int)(yy + camY - it.y)) % 24 == 23) { DrawRectangle((int)(x + sway) - 1, yy, 3, 2, {110, 84, 50, 255}); continue; } // knots to grip
                DrawRectangle((int)(x + sway), yy, 1, 1, c);
            }
            break;
        case IT_CRATE: // flashes when struck
            if (!it.used && it.hit > 0)
            {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawRectangle((int)x, (int)(y - it.h), it.w, it.h, {255, 230, 190, (unsigned char)(it.hit * 12)});
                EndBlendMode();
            }
            break;
        case IT_BOAT:
        {
            float bob = it.used ? 0 : std::sin(G.frame * 0.04f) * 1.0f;
            drawLongship(x, y + bob, it.used ? 0 : clampf(G.sailT / 50.0f, 0, 1));
            break;
        }
        }
    }

    for (auto& l : G.lamps) // wall torches: the flame flickers, the light itself comes from the lighting pass
    {
        if (l.smoke && G.frame % 4 == 0 && !off(l.x - camX, l.y - camY, 60)) // a hall's hearth, smoking through the roof
            spawnParticle(l.x + frange(-1, 1), l.y, frange(0.02f, 0.12f), frange(-0.35f, -0.2f), irange(110, 170), {84, 80, 82, (unsigned char)irange(70, 120)}, -0.002f);
        if (!l.flame || off(l.x - camX, l.y - camY, 8)) continue;
        float x = l.x - camX, y = l.y - camY, fl = hash2((int)l.x, G.frame / 4, 9);
        DrawRectangle((int)x - 1, (int)y - (fl > 0.5f), 3, 4, {255, 120, 30, 255});
        DrawRectangle((int)x - (fl > 0.7f ? 1 : 0), (int)y + 1, 2, 2, {255, 230, 120, 255});
        if (fl > 0.92f) spawnParticle(l.x + 0.5f, l.y - 1, frange(-0.2f, 0.2f), -0.5f, 24, {255, 170, 60, 255}, -0.005f);
    }
    drawTumbleweeds(camX, camY);

    for (auto& pu : G.pickups)
    {
        float x = pu.b.x - camX, y = pu.b.y - camY + std::sin((G.frame + pu.age) * 0.08f) * 0.8f;
        if (off(x, y, 32)) continue;
        switch (pu.kind)
        {
        case PU_SPELL: drawSpriteBig(SPR_SCROLL, x + 4, y + 8, false, SPELLS[pu.spell].col); break;
        case PU_POTION: drawSpriteBig(SPR_POTION, x + 4, y + 8, false, WHITE); break;
        case PU_HEART: drawSpriteBig(SPR_HEART, x + 4, y + 8, false, WHITE); break;
        case PU_AMULET:
        {
            Color c = AMULETS[pu.spell].col;
            float p = 0.7f + 0.3f * std::sin((G.frame + pu.age) * 0.07f);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)x + 4, (int)y + 5, 11 * p, {c.r, c.g, c.b, 70}, {c.r, c.g, c.b, 0});
            EndBlendMode();
            drawAmulet(pu.spell, x - 2, y - 7, 0.75f);
            if (G.frame % 9 == 0) spawnParticle(pu.b.x + frange(0, 8), pu.b.y + frange(-2, 6), 0, -0.3f, 30, c, -0.002f);
            break;
        }
        case PU_MEAD:
        {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircleGradient((int)x + 4, (int)y + 3, 12, {255, 200, 100, 60}, {255, 200, 100, 0});
            EndBlendMode();
            drawPixelArt(MEAD_ART, 8, x - 1, y, 0.6f);
            break;
        }
        case PU_COIN:
        {
            float sp = std::fabs(std::sin((G.frame + pu.age) * 0.12f));
            DrawEllipse((int)x + 4, (int)y + 4, 2.6f * sp + 1.0f, 3.0f, {160, 118, 34, 255});
            DrawEllipse((int)x + 4, (int)y + 4, 2.6f * sp + 0.6f, 2.6f, {240, 196, 60, 255});
            if (sp > 0.5f) DrawRectangle((int)x + 3, (int)y + 2, 1, 1, {255, 246, 190, 255});
            break;
        }
        case PU_WEAPON:
        {
            if (pu.weapon.glow.a) drawGlow(x + 5, y + 3, pu.weapon.glow, 18);
            DrawEllipse((int)x + 5, (int)(pu.b.y - camY) + 9, 6, 1.3f, {0, 0, 0, 80}); // its shadow
            drawWeaponSprite(pu.weapon, {x + 5, y + 4}, -0.5f, 0.5f, true);
            float gl = (float)((G.frame + pu.age * 7) % 90); // a glint running up the blade now and then
            if (gl < 8) DrawRectangle((int)(x + 1 + gl), (int)(y + 7 - gl * 0.55f), 1, 1, WHITE);
            break;
        }
        }
    }

    drawRagdolls(camX, camY);
    drawCorpses(camX, camY);
    drawVillagers(camX, camY);
    for (auto& m : G.mobs)
        if (!off(m.cx() - camX, m.cy() - camY, 96)) drawMobAnimated(m, camX, camY);
    drawPlayerRig(camX, camY);
    for (auto& m : G.mobs)
        if (m.burn > 0 && !off(m.cx() - camX, m.cy() - camY, 40)) drawBurning(m, camX, camY);
    if (G.p.m.alive && G.p.m.burn > 0) drawBurning(G.p.m, camX, camY);
    if (G.p.m.alive && G.p.hasWisp)
    {
        float x = G.p.wx - camX, y = G.p.wy - camY;
        drawGlow(x, y, {255, 225, 150, 255}, 9);
        DrawRectangle((int)x - 1, (int)y - 1, 3, 3, {255, 240, 190, 255});
        DrawRectangle((int)x, (int)y, 1, 1, WHITE);
    }
    if (G.p.m.alive && G.p.hook)
    {
        Mob& pm = G.p.m;
        DrawLineEx({pm.cx() - camX, pm.y + pm.h * 0.4f - camY}, {G.p.hx - camX, G.p.hy - camY}, 1.5f, {150, 130, 100, 255});
        DrawRectangle((int)(G.p.hx - camX) - 1, (int)(G.p.hy - camY) - 1, 3, 3, {200, 200, 210, 255});
    }

    for (auto& p : G.projs)
    {
        float x = p.x - camX, y = p.y - camY;
        if (off(x, y, 32)) continue;
        float sp = std::hypot(p.vx, p.vy) + 0.01f, ux = p.vx / sp, uy = p.vy / sp;
        switch (p.kind)
        {
        case PK_BOLT:
        case PK_ARROW:
            DrawLineEx({x - ux * 7, y - uy * 7}, {x, y}, 1.5f, {120, 84, 50, 255});
            DrawLineEx({x - ux * 7, y - uy * 7}, {x - ux * 5, y - uy * 5}, 2, {220, 220, 220, 255});
            DrawRectangle((int)x - 1, (int)y - 1, 2, 2, p.kind == PK_BOLT ? p.col : Color{180, 180, 190, 255});
            break;
        case PK_BOMB:
            DrawCircle((int)x, (int)y, 2.5f, {30, 30, 34, 255});
            if (G.frame % 6 < 3) DrawRectangle((int)x, (int)y - 4, 1, 2, {255, 200, 60, 255});
            break;
        case PK_ROCK:
            DrawCircle((int)x, (int)y, 2.5f, p.col);
            break;
        default:
            if (p.spell == SP_LIGHTNING)
            {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawLineEx({x - p.vx * 1.5f, y - p.vy * 1.5f}, {x, y}, 4, {200, 200, 120, 90});
                EndBlendMode();
                DrawLineEx({x - p.vx, y - p.vy}, {x, y}, 1.5f, {255, 255, 220, 255});
            }
            else if (p.spell == SP_BOMB)
            {
                y -= 4; // drawn sitting on the ground, not sunk into it
                int blink = p.life < 50 ? 4 : 10; // the spark sputters faster as it nears the powder
                DrawCircle((int)x, (int)y, 5, {40, 40, 46, 255});
                DrawCircle((int)x - 2, (int)y - 2, 1.5f, {110, 110, 120, 255}); // glint
                DrawRectangle((int)x - 1, (int)y - 7, 3, 2, {90, 90, 96, 255}); // cap
                DrawLine((int)x + 1, (int)y - 7, (int)x + 3, (int)y - 10, {160, 130, 90, 255}); // fuse
                if (G.frame % blink < blink / 2) DrawRectangle((int)x + 2, (int)y - 11, 2, 2, {255, 210, 80, 255});
                if (p.life < 50 && G.frame % 6 < 3) DrawCircleLines((int)x, (int)y, 7, RED); // about to blow
            }
            else if (p.spell == SP_DIG)
                DrawRectangle((int)x, (int)y, 2, 2, p.col);
            else
            {
                // a comet: a tapering trail back along its flight, a pulsing halo, and a white-hot core
                float pulse = 0.85f + 0.15f * std::sin(G.frame * 0.5f + p.x);
                BeginBlendMode(BLEND_ADDITIVE);
                for (int k = 5; k >= 1; k--)
                {
                    float t = k / 5.0f;
                    DrawCircleV({x - p.vx * k * 0.9f, y - p.vy * k * 0.9f}, 3.0f * (1 - t * 0.7f), {p.col.r, p.col.g, p.col.b, (unsigned char)(90 * (1 - t))});
                }
                DrawCircleGradient((int)x, (int)y, 7 * pulse, {p.col.r, p.col.g, p.col.b, 130}, {p.col.r, p.col.g, p.col.b, 0});
                EndBlendMode();
                DrawCircleV({x, y}, 2.0f, p.col);
                DrawCircleV({x - ux * 0.4f, y - uy * 0.4f}, 1.0f, WHITE);
            }
        }
    }

    for (auto& q : G.parts)
        if (!off(q.x - camX, q.y - camY, 1)) DrawRectangle((int)(q.x - camX), (int)(q.y - camY), 1, 1, q.col);
}
