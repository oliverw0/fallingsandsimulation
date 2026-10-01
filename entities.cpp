#include "game.h"
#include "util.h"
#include "sprites.h"
#include <cmath>
#include <algorithm>

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

// The render camera keeps a whole-pixel offset from the player, so the hero sits still on
// screen while the world scrolls under them (no 1px shimmer between sprite and terrain).
void syncRenderCamera()
{
    Mob& pm = G.p.m;
    G.rcx = (int)std::floor(pm.x) + (int)std::lround(G.camX - pm.x);
    G.rcy = (int)std::floor(pm.y) + (int)std::lround(G.camY - pm.y);
    if (world.w > G.vw) G.rcx = std::max(0, std::min(world.w - G.vw, G.rcx));
    if (world.h > G.vh) G.rcy = std::max(0, std::min(world.h - G.vh, G.rcy));
}

Vector2 mouseWorld()
{
    Vector2 m = GetMousePosition();
    return {m.x / G.scale + G.rcx, m.y / G.scale + G.rcy};
}

bool boxSolid(float x, float y, int w, int h)
{
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    int x1 = (int)std::floor(x + w - 0.01f), y1 = (int)std::floor(y + h - 0.01f);
    for (int yy = y0; yy <= y1; yy++)
        for (int xx = x0; xx <= x1; xx++)
            if (isSolid(xx, yy))
                return true;
    return false;
}

static bool platformRow(float x, int w, int row)
{
    for (int xx = (int)std::floor(x); xx <= (int)std::floor(x + w - 0.01f); xx++)
        if (world.mat(xx, row) == M::Platform) return true;
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
    float vx = m.vx;
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
    if (rich || chance(85)) addPickupWeapon(x, y, randomWeapon(tier + (rich ? 1 : 0)));
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
    Cell& c = world.at(x, y);
    spawnParticle(x + 0.5f, y + 0.5f, kx * frange(0.3f, 1.2f) + frange(-power, power), frange(-power * 1.5f, -0.3f), irange(40, 90), cellColor(c, x, y), 0.15f);
    c = Cell{};
}

static void breakCrate(Interact& it, float kx, bool loud)
{
    it.used = true;
    for (int y = (int)it.y - it.h; y < (int)it.y; y++)
        for (int x = (int)it.x; x < (int)it.x + it.w; x++)
            if (world.in(x, y) && world.at(x, y).material == M::Wood) splinter(x, y, kx, 1.8f);
    for (int k = 0; k < 14; k++) // dust
        spawnParticle(it.x + frange(0, (float)it.w), it.y - frange(0, (float)it.h), frange(-0.6f, 0.6f), frange(-0.8f, -0.1f), irange(20, 40), {150, 130, 104, 160}, -0.01f);
    if (loud) { playAt(SFX_SMASH, it.x + it.w / 2.0f, it.y, 0.9f, frange(0.9f, 1.1f)); G.shake = std::max(G.shake, 3.0f); }
    if (chance(3)) addCoins(it.x + it.w / 2.0f, it.y - 4, irange(1, 3), 1);
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
        if (world.in(x, y) && world.at(x, y).material == M::Wood) splinter(x, y, kx, 1.0f);
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
    const Color ink = {24, 18, 28, 255}, brown = {110, 70, 40, 255};
    Color mc = w.type == W_STAFF ? Color{170, 140, 100, 255} : METALS[w.metal].color;
    Vector2 d = {std::cos(ang), std::sin(ang)}, p = {-d.y, d.x};
    auto at = [&](float t) { return Vector2{g.x + d.x * t, g.y + d.y * t}; };
    auto ln = [&](Vector2 a, Vector2 b, float th, Color c) { DrawLineEx(a, b, th + 2, ink); DrawLineEx(a, b, th, c); };
    switch (w.type)
    {
    case W_SPEAR:
        ln(at(-len * 0.25f), at(len - 3), 1.5f, brown);
        ln(at(len - 4), at(len + 1), 2.5f, mc);
        break;
    case W_AXE:
    case W_MACE:
        ln(at(-2), at(len), 1.5f, brown);
        if (w.type == W_MACE) { DrawCircleV(at(len + 1), 4, ink); DrawCircleV(at(len + 1), 3, mc); }
        else ln({at(len - 1).x - p.x * 3, at(len - 1).y - p.y * 3}, {at(len - 1).x + p.x * 3, at(len - 1).y + p.y * 3}, 3, mc);
        break;
    case W_CROSSBOW:
        ln(at(0), at(len), 2, brown);
        ln({at(len - 2).x - p.x * 4, at(len - 2).y - p.y * 4}, {at(len - 2).x + p.x * 4, at(len - 2).y + p.y * 4}, 1.2f, mc);
        break;
    default: // swords and daggers
        ln(at(-3), at(0), 1.5f, brown);
        ln({g.x - p.x * 2.5f, g.y - p.y * 2.5f}, {g.x + p.x * 2.5f, g.y + p.y * 2.5f}, 1.2f, {215, 184, 77, 255});
        ln(at(0), at(len), 2, mc);
        DrawLineEx(at(1), at(len - 1), 1, brighten(mc, 55));
        break;
    }
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

void damageMob(Mob& m, float dmg, Element el, float kx, float ky, int flags)
{
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
        case EL_FIRE: m.burn = std::max(m.burn, 180); break;
        case EL_ICE: m.chill = std::max(m.chill, 150); break;
        case EL_SHOCK: m.shock = std::max(m.shock, 25); break;
        case EL_POISON: m.poison = std::max(m.poison, 300); break;
        default: break;
        }
    }
    m.hp -= d;
    if (flags & DMG_HIT)
    {
        m.vx += kx;
        m.vy += ky;
        m.hurtFlash = 6;
        if (isP) { m.iframes = 40; if (d >= 1) { G.hitstop = std::max(G.hitstop, 2); playSfx(SFX_HURT, 0.8f); } }
        Color bc = isP ? FLESH : ENEMIES[m.type].blood;
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

void explode(float x, float y, int r, float dmg, Element el, bool friendly, int power)
{
    explodeCells((int)x, (int)y, r, power);
    pushRagdolls(x, y, r * 2.2f, r * 0.35f);
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
            int cx = (int)x + dx, cy = (int)y + dy;
            if (!world.in(cx, cy)) continue;
            M m = world.at(cx, cy).material;
            if (m == M::Water || m == M::Blood) setCell(cx, cy, M::Ice);
            else if (m == M::Lava && chance(2)) setCell(cx, cy, M::Obsidian);
            else if (m == M::Fire) world.at(cx, cy) = Cell{};
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
        if (m.alive && m.inLiquid && std::hypot(m.cx() - x, m.cy() - y) < 70)
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
                if (world.in(fx, fy))
                {
                    if (world.at(fx, fy).material == M::Empty) setCell(fx, fy, M::Fire);
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
    for (int i = 0; i < 6; i++)
        spawnParticle(x, y, frange(-1, 1), frange(-1, 1), irange(6, 14), p.col, 0.02f);
}

static void hitMob(Proj& p, Mob& m)
{
    float sp = std::hypot(p.vx, p.vy) + 0.01f;
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
        if (!world.in(cx, cy)) { p.alive = false; return; }
        Cell& c = world.at(cx, cy);
        Kind k = props(c.material).kind;
        bool solid = isSolid(cx, cy);
        if (p.kind == PK_SPELL && p.spell == SP_DIG && solid)
        {
            if (props(c.material).hardness <= 4)
            {
                if (props(c.material).ore) c.flags |= CF_LOOSE;
                else c = Cell{};
                p.life--;
            }
            else { p.alive = false; return; }
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
        spawnParticle(p.x, p.y, frange(-0.15f, 0.15f), frange(-0.15f, 0.15f), irange(6, 14), p.col, 0);
    if (p.fuse && G.frame % 4 == 0)
        spawnParticle(p.x, p.y - 1, frange(-0.3f, 0.3f), -0.4f, 8, {255, 200, 80, 255}, 0);
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
    if (m.chill > 0) m.chill--;
    if (m.shock > 0) m.shock--;
    if (m.envCd > 0) m.envCd--;

    int lava = 0, acid = 0, fire = 0, water = 0, miasma = 0;
    int x0 = (int)m.x, y0 = (int)m.y, x1 = (int)(m.x + m.w), y1 = (int)(m.y + m.h);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
        {
            if (!world.in(xx, yy)) continue;
            const Cell& c = world.at(xx, yy);
            switch (c.material)
            {
            case M::Lava: lava++; break;
            case M::Acid: acid++; break;
            case M::Fire: fire++; break;
            case M::Water: water++; break;
            case M::Miasma: miasma++; break;
            default: break;
            }
            if (c.flags & CF_BURNING) fire++;
        }
    bool spikes = false;
    for (int xx = x0; xx < x1; xx++)
        if (world.mat(xx, y1) == M::Spikes || world.mat(xx, y1 - 1) == M::Spikes) spikes = true;

    bool fireImmune = isP ? (G.p.armour >= 0 && METALS[G.p.armour].el == EL_FIRE) : ENEMIES[m.type].resist[EL_FIRE] <= 0;
    if (lava) { damageMob(m, 0.6f, EL_FIRE, 0, 0, 0); if (!fireImmune) m.burn = std::max(m.burn, 180); }
    if (acid) damageMob(m, 0.35f, EL_POISON, 0, 0, 0);
    if (fire && !fireImmune) m.burn = std::max(m.burn, 120);
    if (water && m.burn) m.burn = 0;
    if (miasma && (isP || ENEMIES[m.type].resist[EL_POISON] > 0)) m.poison = std::max(m.poison, 60);
    if (spikes && m.envCd == 0 && !(m.type >= 0 && ENEMIES[m.type].noclip))
    {
        damageMob(m, 14, EL_PHYS, 0, -2.5f, DMG_HIT);
        m.envCd = 30;
    }
    if (m.burn > 0)
    {
        m.burn--;
        damageMob(m, 0.12f, EL_FIRE, 0, 0, 0);
        if (G.frame % 3 == 0)
            spawnParticle(m.x + frand() * m.w, m.y + frand() * m.h, frange(-0.2f, 0.2f), -0.5f, irange(8, 16), {255, (unsigned char)irange(90, 200), 30, 255}, -0.01f);
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

static void meleeAttack(const Weapon& w)
{
    const WeaponTypeDef& t = WTYPES[w.type];
    const MetalDef& md = METALS[w.metal];
    int fx = w.fx;
    Mob& pm = G.p.m;
    float ox = pm.cx(), oy = pm.y + pm.h * 0.45f;
    float aim = G.p.aim;
    float arc = t.arc * DEG2RAD;
    float R = (float)t.range;
    bool heavy = w.type == W_AXE || w.type == W_MACE;
    playSfx(SFX_SWING, 0.7f, w.type == W_DAGGER ? 1.3f : (heavy ? 0.75f : (w.type == W_SPEAR ? 1.15f : 1.0f)));

    for (auto& m : G.mobs)
    {
        if (!m.alive) continue;
        float dx = m.cx() - ox, dy = m.cy() - oy;
        float d = std::sqrt(dx * dx + dy * dy);
        float reach = std::max(m.w, m.h) * 0.5f;
        if (d > R + reach) continue;
        float da = std::fabs(std::remainder(std::atan2(dy, dx) - aim, 2 * PI));
        if (da > arc && d > reach + 2) continue;
        float dmg = weaponDamage(w) * frange(0.9f, 1.1f);
        if (fx & UF_RANDOM) dmg *= frange(0.4f, 2.2f);
        if ((fx & UF_EXECUTE) && m.hp < m.maxHp * 0.3f) dmg *= 3;
        float knock = (heavy || w.type == W_PAN ? 3.5f : 2.5f) * ((fx & UF_KNOCK) ? 2.5f : 1.0f);
        damageMob(m, dmg, md.el, std::cos(aim) * knock, (fx & UF_KNOCK) ? -3.0f : -1.2f, DMG_HIT);
        if (md.el == EL_SHOCK || (fx & UF_CHAIN)) chainShock(m, dmg * 0.5f);
        if (fx & UF_BURN) m.burn = std::max(m.burn, 240);
        if (fx & UF_CHILL) m.chill = std::max(m.chill, 200);
        if (fx & UF_POISON) m.poison = std::max(m.poison, 400);
        if (fx & UF_BLEED)
        {
            m.bleedMark = 900;
            for (int k = 0; k < 6; k++) spawnCellParticle(m.cx(), m.cy(), std::cos(aim) * frange(0.5f, 2) + frange(-1, 1), frange(-2, 0), ENEMIES[m.type].gore, 0);
        }
        if (fx & UF_LEECH) pm.hp = std::min(pm.maxHp, pm.hp + dmg * 0.15f);
        if (heavy) G.hitstop = std::max(G.hitstop, 2);
        bool armoured = m.type == E_KNIGHT || m.type == E_GOLEM || m.type == E_BLACKKNIGHT || m.type == E_SKELETON || m.type == E_GUARD || m.type == E_DRAUGR;
        if (w.type == W_PAN) playSfx(SFX_CLANG, 0.9f, 1.8f); // bonk
        else playSfx(armoured ? SFX_CLANG : SFX_HIT, 0.8f);
        for (int k = 0; k < 6; k++)
            spawnParticle(m.cx(), m.cy(), std::cos(aim) * frange(0.5f, 2) + frange(-0.6f, 0.6f), std::sin(aim) * frange(0.5f, 2) + frange(-0.6f, 0.6f), irange(5, 10), {255, 250, 220, 255}, 0);
    }

    for (auto& it : G.inter) // crates and barrels in the swing
    {
        if (it.type != IT_CRATE || it.used) continue;
        float nx = clampf(ox, it.x, it.x + it.w), ny = clampf(oy, it.y - it.h, it.y);
        float dx = nx - ox, dy = ny - oy;
        if (dx * dx + dy * dy > (R + 2) * (R + 2)) continue;
        if (dx * dx + dy * dy > 16 && std::fabs(std::remainder(std::atan2(dy, dx) - aim, 2 * PI)) > arc + 0.3f) continue;
        damageCrate(it, heavy ? 2 : 1, std::cos(aim) * 2);
        if (heavy) G.hitstop = std::max(G.hitstop, 2);
    }

    int r = (int)R;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
        {
            float d = std::sqrt((float)(dx * dx + dy * dy));
            if (d > R || d < 2) continue;
            if (std::fabs(std::remainder(std::atan2((float)dy, (float)dx) - aim, 2 * PI)) > arc) continue;
            int x = (int)ox + dx, y = (int)oy + dy;
            if (!world.in(x, y)) continue;
            Cell& c = world.at(x, y);
            M m = c.material;
            if (m == M::Empty) continue;
            const MaterialProps& p = props(m);
            if (m == M::Keg) { ignite(x, y); continue; }
            if ((md.el == EL_FIRE || (fx & UF_BURN)) && p.flammable && chance(6)) ignite(x, y);
            if ((md.el == EL_ICE || (fx & UF_CHILL)) && m == M::Water && chance(3)) { setCell(x, y, M::Ice); continue; }
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
            if ((fx & UF_MINER) && p.kind == Kind::Solid && p.hardness <= md.mine && !p.ore && frand() < t.mineChance + 0.4f)
            {
                c = Cell{}; // delving weapons cut the rock itself
                continue;
            }
            // otherwise blades only chip ore loose; the rock itself stays put
            if (!p.ore || (c.flags & CF_LOOSE) || p.hardness > md.mine || frand() > std::max(t.mineChance, 0.15f)) continue;
            c.flags |= CF_LOOSE;
            if (chance(3)) spawnParticle(x + 0.5f, y + 0.5f, frange(-1, 1), frange(-1.5f, 0), 8, {255, 240, 180, 255}, 0.1f);
        }
}

static void collectOre()
{
    Mob& m = G.p.m;
    for (int yy = (int)m.y - 2; yy < (int)(m.y + m.h) + 2; yy++)
        for (int xx = (int)m.x - 2; xx < (int)(m.x + m.w) + 2; xx++)
        {
            if (!world.in(xx, yy)) continue;
            Cell& c = world.at(xx, yy);
            if (!(c.flags & CF_LOOSE)) continue;
            int ore = props(c.material).ore;
            if (!ore) continue;
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
            P.stamina -= 0.6f;
            if (G.frame % 6 == 0) spawnParticle(P.onWall > 0 ? m.x + m.w : m.x, m.y + m.h, 0, 0.3f, 10, {120, 110, 100, 255}, 0.1f);
        }
        else
            m.vy = std::min(m.vy, 0.8f);
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
    if (m.y > world.h + 20) m.hp = 0;

    // hotbar
    int n = (int)P.hotbar.size();
    for (int k = 0; k < 6; k++)
        if (IsKeyPressed(KEY_ONE + k) && k < n) P.sel = k;
    float wh = GetMouseWheelMove();
    if (wh != 0 && n > 0) P.sel = ((P.sel - (wh > 0 ? 1 : -1)) % n + n) % n;
    if (P.sel >= n) P.sel = std::max(0, n - 1);

    for (auto& w : P.hotbar)
        if (w.type == W_STAFF)
        {
            if (w.staff.cd > 0) w.staff.cd--;
            w.staff.mana = std::min(w.staff.manaMax, w.staff.mana + w.staff.regen);
        }

    if (P.attackCd > 0) P.attackCd--;
    if (P.combatT > 0) P.combatT--;
    if (P.swingT > 0) P.swingT--;
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
                playSfx(SFX_BOW, 0.8f);
            }
            else
            {
                meleeAttack(w);
                P.attackCd = weaponCooldown(w);
                P.swingT = 10;
                P.swingDir = -P.swingDir;
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

static bool findFloorNear(float x, float y, float& ox, float& oy, int w, int h)
{
    for (int tries = 0; tries < 30; tries++)
    {
        int tx = (int)x + irange(-80, 80), ty = (int)y + irange(-40, 20);
        if (!world.in(tx, ty) || isSolid(tx, ty)) continue;
        int fy = ty;
        while (fy < world.h - 1 && !isSolid(tx, fy + 1) && fy - ty < 60) fy++;
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
        if (isSolid(col, feet + k) || world.mat(col, feet + k) == M::Platform) return; // ground ahead
    M below = M::Empty;
    for (int k = 0; k < 40 && below == M::Empty; k++) below = world.mat(col, feet + k);
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
    M below = world.mat(fx, fy);
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
    if (m.type == E_TROLL && m.burn == 0 && m.hp < m.maxHp) m.hp += 0.04f; // trolls regenerate unless burned
    if (m.shock > 0) spd *= 0.2f;
    int fdir = dx > 0 ? 1 : -1;
    bool flying = d.ai == AI_FLY || d.ai == AI_FLYCAST || d.ai == AI_BOSS_LICH;
    float aimAng = std::atan2(dy, dx);

    switch (d.ai)
    {
    case AI_WALK:
        if (m.aggro)
        {
            m.facing = fdir;
            float tv = std::fabs(dx) < d.range * 0.5f ? 0 : fdir * spd;
            if (m.onGround || std::fabs(m.vx) < std::fabs(tv)) m.vx += clampf(tv - m.vx, -0.2f, 0.2f); // keep a leap's momentum
            if (tv != 0) traverse(m, fdir, dy);
            if (m.onGround && m.vy == 0 && dy < -24 && chance(40)) m.vy = -3.9f;
            if (m.onGround && dy > 24 && onPlatform(m) && chance(30)) m.dropT = 12;
            if (m.cd == 0 && std::fabs(dx) < d.range + (m.w + pm.w) * 0.5f && std::fabs(dy) < m.h)
            {
                m.cd = d.cooldown;
                m.attackT = 12;
                damageMob(pm, m.dmg, d.el, fdir * 2.2f, -1.5f, DMG_HIT);
            }
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
        if (m.aggro)
        {
            if (m.state <= 1 && m.state >= -1 && m.timer > 0)
            {
                float sp = spd * 1.3f;
                m.vx += (std::cos(aimAng) * sp - m.vx) * 0.08f;
                m.vy += (std::sin(aimAng) * sp + std::sin(m.timer * 0.2f) * 0.6f - m.vy) * 0.08f;
                if (overlap(m, pm)) { touchDamage(m, 1); m.state = 5; m.timer = 0; }
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
        if (m.type == E_WRAITH || m.type == E_BANSHEE) touchDamage(m, 0.6f);
        break;
    }

    case AI_HOP:
        if (m.onGround)
        {
            m.vx *= 0.8f;
            if (m.aggro && m.cd == 0)
            {
                m.vx = fdir * 1.7f * pace * (m.chill > 0 ? 0.5f : 1.0f);
                m.vy = -3.4f;
                m.cd = d.cooldown + irand(30);
            }
            else if (!m.aggro && m.timer % 150 == 0)
            {
                m.vx = (irand(2) ? 1 : -1) * 0.8f;
                m.vy = -2.5f;
            }
        }
        m.facing = fdir;
        touchDamage(m, 1);
        break;

    case AI_BOSS_KNIGHT:
        if (!m.aggro) break;
        if (m.state == 0 || m.state == 1 || m.state == -1)
        {
            m.facing = fdir;
            float tv = std::fabs(dx) < 10 ? 0 : fdir * spd;
            m.vx += clampf(tv - m.vx, -0.2f, 0.2f);
            if (m.onGround && m.wall == fdir) m.vy = -4.5f;
            if (m.cd == 0 && std::fabs(dx) < d.range + 8 && std::fabs(dy) < m.h)
            {
                m.cd = d.cooldown;
                m.attackT = 14;
                damageMob(pm, m.dmg, EL_PHYS, fdir * 3.0f, -2.0f, DMG_HIT);
            }
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
        touchDamage(m, 0.8f);
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
            if (m.aggro && py < cy) m.vy -= m.type == E_KELPIE ? 0.3f : 0.12f;
        }
        else
            m.vy = std::min(m.vy + 0.26f, 6.0f);
    }
    if (d.noclip)
    {
        m.x = clampf(m.x + m.vx, 4, world.w - m.w - 4.0f);
        m.y = clampf(m.y + m.vy, 4, world.h - m.h - 4.0f);
    }
    else
    {
        float vyB = m.vy;
        bool wasG = m.onGround;
        moveMob(m);
        if (m.onGround && !wasG && vyB > 1.5f) m.squash = 0.7f;
    }
    m.squash += (1 - m.squash) * 0.2f;
    if (m.y > world.h + 10) m.hp = 0;
    m.anim += std::fabs(m.vx) * 0.15f + (flying ? 0.15f : 0);
}

static void killMob(Mob& m)
{
    m.alive = false;
    const EnemyDef& d = ENEMIES[m.type];
    G.p.kills++;
    G.hitstop = std::max(G.hitstop, 3);
    ragdollForMob(m);
    playAt(SFX_DIE, m.cx(), m.cy(), 0.7f, m.boss ? 0.5f : frange(0.9f, 1.3f));
    for (int i = 0; i < 14 + m.w * 2; i++)
        spawnCellParticle(m.cx() + frange(-2, 2), m.cy() + frange(-3, 3), m.vx * 0.5f + frange(-1.8f, 1.8f), frange(-3.0f, 0), d.gore, 0);
    bleed(m, d.blood, 12);
    if (m.bleedMark > 0) // cut by a bleeding weapon: a fountain of gore
        for (int i = 0; i < 50; i++)
            spawnCellParticle(m.cx(), m.cy(), frange(-2.5f, 2.5f), frange(-4.0f, -0.5f), d.gore, 0);
    addCoins(m.cx(), m.cy(), m.boss ? 25 : irange(1, 3) + G.stage / 2, m.boss ? 5 : 1);
    if (m.type == E_SLIME) paintCircle((int)m.cx(), (int)m.cy(), 2, M::Acid, true);
    if (m.type == E_BOMBER) explode(m.cx(), m.cy(), 5, 10, EL_FIRE, false, 3);
    dropLoot(m.cx(), m.cy(), m.boss);
    if (m.boss)
    {
        G.portalOpen = true;
        G.bossId = 0;
        G.shake = 14;
        message(std::string(d.name) + " has fallen!");
        if (m.type == E_LICH) G.winTimer = 200;
        else message("The way forward is open.");
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
            int cx = (int)std::floor(nx), cy = (int)std::floor(ny);
            if (!world.in(cx, cy)) { q.life = 0; continue; }
            if (world.at(cx, cy).material != M::Empty || q.life <= 0)
            {
                int px = (int)std::floor(q.x), py = (int)std::floor(q.y);
                if (world.in(px, py) && world.at(px, py).material == M::Empty)
                {
                    setCell(px, py, q.toCell);
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
        Particle p{d.x, d.y, d.vx, d.vy, 200, c, 0.15f};
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
            pu.b.vx *= 0.95f;
            moveMob(pu.b);
        }
        if (pu.b.y > world.h + 10) pu.alive = false;
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
                    if (world.in(t.x, t.y - k) && world.at(t.x, t.y - k).material == M::Empty && chance(2))
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
                for (int y = t.ry0; y <= t.ry1; y++)
                    for (int x = t.rx0; x <= t.rx1; x++)
                    {
                        if (!world.in(x, y)) continue;
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
            for (int x = (int)it.x; x < (int)it.x + it.w; x++) n += world.mat(x, y) == M::Wood;
        if (n * 5 < it.cells * 2) breakCrate(it, 0, false);
    }
    for (auto& it : G.inter) // chests settle onto whatever is beneath them (and ride falling sand down)
    {
        if (it.type != IT_CHEST) continue;
        bool support = false;
        for (int dx = -4; dx <= 4 && !support; dx++) support = isSolid((int)it.x + dx, (int)it.y);
        if (!support && it.y < world.h - 2) it.y += 1.5f;
        else
            for (int k = 0; k < 4 && isSolid((int)it.x, (int)it.y - 2); k++) it.y -= 1; // buried: pop up
    }

    Mob& pm = G.p.m;
    G.nearInteract = -1;
    float bd = 26;
    for (int i = 0; i < (int)G.inter.size(); i++)
    {
        Interact& it = G.inter[i];
        if ((it.type == IT_CHEST && it.used) || it.type == IT_TORCH || it.type == IT_ROPE || it.type == IT_CRATE) continue;
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
        if (chance(6)) addPickupWeapon(it.x, it.y - 6, randomWeapon(G.stage));
        if (chance(4)) addPickup(it.x, it.y - 6, PU_HEART);
        if (G.stage >= 1 && chance(20)) addPickupWeapon(it.x, it.y - 6, rollLegendary(G.stage + 1, false));
        addCoins(it.x, it.y - 6, irange(6, 14) + G.stage * 2, 1);
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
    case IT_SHRINE:
        if (it.used) message("The shrine is silent.");
        else G.state = GS_SHRINE;
        break;
    case IT_PORTAL:
        if (!G.portalOpen) message("The portal is sealed. Defeat the guardian.");
        else { playSfx(SFX_PORTAL, 0.8f); travelOnward(); }
        break;
    }
}

void travelOnward()
{
    if (G.sandbox) return;
    if (G.inVillage) // set out on a fresh run with the chosen loadout
    {
        newGameKit(false);
        G.stage = 0;
        G.loadTarget = LOAD_STAGE;
        G.state = GS_LOADING;
        return;
    }
    if (G.sanctuary)
    {
        G.stage++;
        G.loadTarget = LOAD_STAGE;
    }
    else
    {
        if (G.stage >= STAGE_COUNT - 1) { bankRun(); G.state = GS_WIN; return; }
        G.loadTarget = LOAD_SANCTUARY;
    }
    G.state = GS_LOADING;
}

// Pause-menu escape hatch: if you ever get stuck, walk back to the nearest torch on the road.
void returnToRoad()
{
    Mob& m = G.p.m;
    float bd = 1e9f, tx = -1, ty = 0;
    for (auto& it : G.inter)
        if (it.type == IT_TORCH)
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

void updateGame()
{
    if (G.hitstop > 0) { G.hitstop--; return; } // freeze frames sell the impact
    G.frame++;
    Mob& pm = G.p.m;

    if (G.sandbox) sandboxTools();
    updatePlayer();
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
        if (!m.boss && (m.cx() < ax0 || m.cx() > ax1 || m.cy() < ay0 || m.cy() > ay1)) continue;
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

    simulate((int)G.camX - 100, (int)G.camY - 100, (int)G.camX + G.vw + 100, (int)G.camY + G.vh + 100);

    std::vector<Blast> bl;
    bl.swap(world.blasts);
    int n = 0;
    for (auto& b : bl)
    {
        if (n++ < 24) explode((float)b.x, (float)b.y, b.r, b.dmg, EL_FIRE, false, b.power);
        else world.blasts.push_back(b);
    }
    updateParticles();
    updateRagdolls();

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

    // camera
    float tx = pm.cx() - G.vw / 2.0f, ty = pm.cy() - G.vh / 2.0f;
    G.camX += (tx - G.camX) * 0.12f;
    G.camY += (ty - G.camY) * 0.12f;
    if (world.w <= G.vw) G.camX = (world.w - G.vw) / 2.0f;
    else G.camX = clampf(G.camX, 0, (float)(world.w - G.vw));
    if (world.h <= G.vh) G.camY = (world.h - G.vh) / 2.0f;
    else G.camY = clampf(G.camY, 0, (float)(world.h - G.vh));
    syncRenderCamera();
    G.shake *= 0.88f;
}

// ================================================================ drawing (render-texture space)

void drawEntities(int camX, int camY)
{
    // interactables
    for (auto& it : G.inter)
    {
        float x = it.x - camX, y = it.y - camY;
        switch (it.type)
        {
        case IT_CHEST:
        {
            drawSpriteBig(it.used ? SPR_CHEST_OPEN : SPR_CHEST, x, y, false, WHITE);
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
        {
            static const Color stripe[3] = {{180, 40, 40, 255}, {50, 70, 170, 255}, {50, 130, 60, 255}};
            drawSpriteNative(SPR_MERCHANT, x + 4, y - 7, true);
            DrawRectangle((int)x - 17, (int)y - 9, 34, 9, {24, 18, 28, 255});
            DrawRectangle((int)x - 16, (int)y - 8, 32, 7, {130, 88, 50, 255});
            DrawRectangle((int)x - 16, (int)y - 8, 32, 2, {160, 112, 66, 255});
            for (int px2 : {-16, 15}) DrawRectangle((int)x + px2, (int)y - 34, 2, 26, {90, 58, 32, 255});
            for (int k = 0; k < 9; k++)
                DrawRectangle((int)x - 18 + k * 4, (int)y - 38, 4, 6, (k % 2) ? Color{236, 230, 214, 255} : stripe[it.data % 3]);
            for (int k = 0; k < 9; k++) DrawRectangle((int)x - 18 + k * 4, (int)y - 32, 4, 2 + (k % 2), (k % 2) ? Color{236, 230, 214, 255} : stripe[it.data % 3]);
            // wares on the counter
            Color ware = it.data == 0 ? Color{170, 176, 186, 255} : (it.data == 1 ? Color{170, 120, 255, 255} : Color{214, 52, 52, 255});
            for (int k = 0; k < 3; k++) DrawRectangle((int)x - 12 + k * 9, (int)y - 12, 3, 3, ware);
            break;
        }
        case IT_STONE:
        {
            const Weapon* w = it.used ? nullptr : &G.stoneLoot[it.data];
            const Color ink = {24, 18, 28, 255}, wood = {120, 80, 46, 255}, woodD = {80, 52, 30, 255}, stone = {120, 116, 124, 255};
            auto box = [&](float x0, float y0, float x1, float y1, Color c) {
                DrawRectangle((int)x0 - 1, (int)y0 - 1, (int)(x1 - x0) + 2, (int)(y1 - y0) + 2, ink);
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
        case IT_PORTAL:
            for (int k = 0; k < 48; k++)
            {
                float a = k / 48.0f * 6.2832f + G.frame * 0.04f;
                float r = 1.0f - 0.15f * std::sin(G.frame * 0.1f + k);
                Color c = G.portalOpen ? ColorFromHSV(260 + 40 * std::sin(k * 0.5f + G.frame * 0.05f), 0.7f, 1.0f) : Color{90, 90, 96, 255};
                DrawRectangle((int)(x + std::cos(a) * 11 * r), (int)(y - 21 + std::sin(a) * 20 * r), 2, 2, c);
            }
            if (G.portalOpen)
                DrawEllipse((int)x, (int)y - 21, 8, 17, {60, 20, 90, 200});
            break;
        }
    }

    for (auto& pu : G.pickups)
    {
        float x = pu.b.x - camX, y = pu.b.y - camY + std::sin((G.frame + pu.age) * 0.08f) * 0.8f;
        switch (pu.kind)
        {
        case PU_SPELL: drawSpriteBig(SPR_SCROLL, x + 4, y + 8, false, SPELLS[pu.spell].col); break;
        case PU_POTION: drawSpriteBig(SPR_POTION, x + 4, y + 8, false, WHITE); break;
        case PU_HEART: drawSpriteBig(SPR_HEART, x + 4, y + 8, false, WHITE); break;
        case PU_COIN:
        {
            float sp = std::fabs(std::sin((G.frame + pu.age) * 0.12f));
            DrawEllipse((int)x + 4, (int)y + 4, 2.6f * sp + 1.6f, 3.6f, {24, 18, 28, 255});
            DrawEllipse((int)x + 4, (int)y + 4, 2.6f * sp + 0.6f, 2.6f, {240, 196, 60, 255});
            if (sp > 0.5f) DrawRectangle((int)x + 3, (int)y + 2, 1, 1, {255, 246, 190, 255});
            break;
        }
        case PU_WEAPON:
        {
            if (pu.weapon.glow.a) drawGlow(x + 5, y + 3, pu.weapon.glow, 18);
            Color c = pu.weapon.type == W_STAFF ? pu.weapon.staff.gem : METALS[pu.weapon.metal].color;
            DrawLineEx({x, y + 8}, {x + 9, y - 1}, 2, {104, 66, 36, 255});
            DrawLineEx({x + 4, y + 3}, {x + 10, y - 3}, 2, c);
            if (G.frame % 40 < 4) DrawRectangle((int)x + 9, (int)y - 4, 1, 1, WHITE);
            break;
        }
        }
    }

    drawRagdolls(camX, camY);
    for (auto& m : G.mobs) drawMobAnimated(m, camX, camY);
    drawPlayerRig(camX, camY);
    if (G.p.m.alive && G.p.hook)
    {
        Mob& pm = G.p.m;
        DrawLineEx({pm.cx() - camX, pm.y + pm.h * 0.4f - camY}, {G.p.hx - camX, G.p.hy - camY}, 1.5f, {150, 130, 100, 255});
        DrawRectangle((int)(G.p.hx - camX) - 1, (int)(G.p.hy - camY) - 1, 3, 3, {200, 200, 210, 255});
    }

    for (auto& p : G.projs)
    {
        float x = p.x - camX, y = p.y - camY;
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
                DrawCircle((int)x, (int)y, 3, {50, 50, 56, 255});
                if (G.frame % 8 < 4) DrawRectangle((int)x, (int)y - 1, 2, 2, RED);
            }
            else if (p.spell == SP_DIG)
                DrawRectangle((int)x, (int)y, 2, 2, p.col);
            else
            {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCircleGradient((int)x, (int)y, 6, {p.col.r, p.col.g, p.col.b, 120}, {p.col.r, p.col.g, p.col.b, 0});
                EndBlendMode();
                DrawCircle((int)x, (int)y, 1.8f, p.col);
                DrawRectangle((int)x, (int)y, 1, 1, WHITE);
            }
        }
    }

    for (auto& q : G.parts)
        DrawRectangle((int)(q.x - camX), (int)(q.y - camY), 1, 1, q.col);
}
