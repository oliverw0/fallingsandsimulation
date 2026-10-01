#include "game.h"
#include "util.h"
#include <algorithm>
#include <cmath>

const char* RES_NAMES[RES_COUNT] = {"Copper", "Iron", "Coal", "Gold", "Firestone", "Frostite", "Stormite", "Venomite", "Adamantite"};
const Color RES_COLORS[RES_COUNT] = {
    {214, 130, 70, 255}, {176, 150, 140, 255}, {70, 68, 72, 255}, {240, 200, 60, 255}, {240, 90, 30, 255},
    {130, 210, 250, 255}, {170, 130, 250, 255}, {100, 220, 100, 255}, {60, 210, 190, 255}};

//                 name          colour                dmg  mine element  armour  cost: Cu Fe  C Au Fi Fr St Ve Ad
const MetalDef METALS[METAL_COUNT] = {
    {"Copper",     {205, 120, 70, 255},  1.00f, 3, EL_PHYS,   0.15f, {20, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"Iron",       {165, 165, 172, 255}, 1.25f, 4, EL_PHYS,   0.22f, {0, 20, 0, 0, 0, 0, 0, 0, 0}},
    {"Steel",      {205, 214, 224, 255}, 1.55f, 5, EL_PHYS,   0.30f, {0, 20, 15, 0, 0, 0, 0, 0, 0}},
    {"Damascus",   {150, 162, 180, 255}, 1.90f, 6, EL_PHYS,   0.38f, {0, 30, 25, 8, 0, 0, 0, 0, 0}},
    {"Firestone",  {244, 96, 34, 255},   1.60f, 5, EL_FIRE,   0.28f, {0, 10, 0, 0, 20, 0, 0, 0, 0}},
    {"Frostite",   {130, 210, 250, 255}, 1.60f, 5, EL_ICE,    0.28f, {0, 10, 0, 0, 0, 20, 0, 0, 0}},
    {"Stormite",   {176, 140, 255, 255}, 1.60f, 5, EL_SHOCK,  0.28f, {0, 10, 0, 0, 0, 0, 20, 0, 0}},
    {"Venomite",   {104, 226, 100, 255}, 1.60f, 5, EL_POISON, 0.28f, {0, 10, 0, 0, 0, 0, 0, 20, 0}},
    {"Adamantium", {70, 226, 206, 255},  2.40f, 8, EL_PHYS,   0.50f, {0, 0, 15, 0, 0, 0, 0, 0, 25}},
};

//                 name         dmg range cd  arc  mine  cost
const WeaponTypeDef WTYPES[WTYPE_COUNT] = {
    {"Dagger",     5,  15, 14, 45, 0.25f, 0.6f},
    {"Sword",      9,  21, 26, 65, 0.35f, 1.0f},
    {"Battleaxe",  15, 23, 44, 85, 0.60f, 1.5f},
    {"Crossbow",   10, 0,  45, 0,  0.0f,  1.2f},
    {"Staff",      0,  0,  0,  0,  0.0f,  0.0f},
    {"Armour",     0,  0,  0,  0,  0.0f,  2.0f},
    {"Spear",      10, 28, 30, 18, 0.30f, 1.3f},
    {"Mace",       12, 17, 32, 70, 0.40f, 1.4f},
    {"Frying Pan", 5,  13, 22, 60, 0.0f,  0.0f},
};

// name, abbr, type, mana, delay, recharge, spread, dmg, speed, life, el, grav, blast, extra, minTier, colour, desc
const SpellDef SPELLS[SPELL_COUNT] = {
    {"Spark Bolt", "SB", ST_PROJ, 5, 4, 0, 2, 6, 7.0f, 40, EL_PHYS, 0, 0, 0, 0, {255, 190, 255, 255}, "A weak but quick magical bolt."},
    {"Arcane Missile", "AM", ST_PROJ, 25, 12, 0, 1, 18, 4.0f, 90, EL_PHYS, 0, 4, 0, 0, {170, 120, 255, 255}, "A slow orb that bursts on impact."},
    {"Fireball", "FB", ST_PROJ, 40, 20, 0, 3, 14, 4.5f, 120, EL_FIRE, 0.06f, 7, 0, 0, {255, 120, 30, 255}, "Arcing ball of flame. Sets the world alight."},
    {"Ice Shard", "IS", ST_PROJ, 20, 8, 0, 2, 10, 6.0f, 60, EL_ICE, 0.02f, 0, 0, 0, {160, 220, 255, 255}, "Chills foes and freezes water solid."},
    {"Lightning", "LB", ST_PROJ, 60, 25, 10, 0, 26, 14.0f, 30, EL_SHOCK, 0, 3, 0, 1, {240, 240, 130, 255}, "Near-instant bolt. Electrifies any water it strikes.", 8},
    {"Acid Orb", "AO", ST_PROJ, 30, 15, 0, 3, 6, 4.0f, 90, EL_POISON, 0.05f, 0, 0, 1, {130, 255, 80, 255}, "Bursts into a pool of corrosive acid.", 6},
    {"Bomb", "BO", ST_PROJ, 30, 40, 0, 0, 40, 3.0f, 100, EL_FIRE, 0.15f, 14, 0, 0, {170, 170, 180, 255}, "A heavy bomb with a short fuse. Mines ore.", 3},
    {"Digging Bolt", "DG", ST_PROJ, 3, 2, 0, 1, 2, 6.0f, 12, EL_PHYS, 0, 0, 0, 0, {200, 170, 120, 255}, "Carves tunnels through soft rock."},
    {"Water Orb", "WO", ST_PROJ, 15, 10, 0, 2, 3, 4.5f, 80, EL_PHYS, 0.05f, 0, 0, 0, {60, 120, 230, 255}, "Summons a burst of water. Douses flames.", 10},
    {"Trigger Bolt", "TB", ST_PROJ, 10, 6, 0, 2, 6, 7.0f, 40, EL_PHYS, 0, 0, 0, 1, {255, 255, 160, 255}, "Spark bolt that casts the next spell where it hits.", 15},
    {"Empower", "+D", ST_MOD, 10, 2, 0, 0, 10, 0, 0, EL_PHYS, 0, 0, 0, 0, {255, 90, 90, 255}, "Modifier: +10 damage."},
    {"Haste", ">>", ST_MOD, 5, 0, 0, 0, 0, 0, 0, EL_PHYS, 0, 0, 0, 0, {255, 230, 90, 255}, "Modifier: projectile speed x1.75."},
    {"Bounce", "BN", ST_MOD, 5, 0, 0, 0, 0, 0, 0, EL_PHYS, 0, 0, 0, 0, {120, 240, 200, 255}, "Modifier: projectiles bounce 3 times."},
    {"Homing", "HM", ST_MOD, 20, 0, 0, 0, 0, 0, 0, EL_PHYS, 0, 0, 0, 1, {255, 140, 220, 255}, "Modifier: projectiles seek enemies."},
    {"Piercing", "PI", ST_MOD, 15, 0, 0, 0, 0, 0, 0, EL_PHYS, 0, 0, 0, 1, {200, 200, 255, 255}, "Modifier: projectiles pass through foes."},
    {"Ignite", "IG", ST_MOD, 10, 0, 0, 0, 3, 0, 0, EL_FIRE, 0, 0, 0, 0, {255, 110, 40, 255}, "Modifier: fire damage and a burning trail."},
    {"Frostbind", "FR", ST_MOD, 10, 0, 0, 0, 0, 0, 0, EL_ICE, 0, 0, 0, 1, {150, 220, 255, 255}, "Modifier: converts damage to frost."},
    {"Explosive", "EX", ST_MOD, 25, 10, 0, 0, 0, 0, 0, EL_PHYS, 0, 5, 0, 1, {255, 170, 70, 255}, "Modifier: projectiles explode on impact.", 10},
    {"Double Cast", "x2", ST_MULTI, 0, 0, 0, 0, 0, 0, 0, EL_PHYS, 0, 0, 2, 0, {240, 240, 240, 255}, "Multicast: casts the next 2 spells at once."},
    {"Triple Cast", "x3", ST_MULTI, 2, 0, 0, 0, 0, 0, 0, EL_PHYS, 0, 0, 3, 1, {255, 255, 255, 255}, "Multicast: casts the next 3 spells at once.", 12},
};

SpellCard makeCard(int id) { return SpellCard{id, SPELLS[id].uses ? SPELLS[id].uses : -1}; }

// Weapons fall into five tiers by metal: I copper, II iron, III steel, IV damascus/elemental, V adamantium.
// Legendaries sit one tier above their metal.
static const int METAL_TIER[METAL_COUNT] = {1, 2, 3, 4, 4, 4, 4, 4, 5};
int weaponTier(const Weapon& w)
{
    if (w.type == W_STAFF || w.type == W_PAN) return 1;
    return std::min(5, METAL_TIER[w.metal] + (w.fx ? 1 : 0));
}

std::string weaponName(const Weapon& w)
{
    if (!w.title.empty())
        return w.title;
    if (w.dmgMul < 0.95f && w.type != W_STAFF) return "Worn " + std::string(METALS[w.metal].name) + " " + WTYPES[w.type].name;
    if (w.type == W_STAFF)
        return w.staff.name;
    if (w.type == W_CROSSBOW && w.metal == M_COPPER)
        return "Old Crossbow";
    return std::string(METALS[w.metal].name) + " " + WTYPES[w.type].name;
}

float armourDef(int armour) { return armour < 0 ? 0.08f : METALS[armour].armor; }

int randomSpell(int tier)
{
    for (int tries = 0; tries < 50; tries++)
    {
        int s = irand(SPELL_COUNT);
        if (SPELLS[s].minTier > tier)
            continue;
        // projectiles are more common than modifiers
        if (SPELLS[s].type != ST_PROJ && chance(2))
            continue;
        return s;
    }
    return SP_SPARK;
}

Staff randomStaff(int tier)
{
    static const char* A[] = {"Oaken", "Ashen", "Yew", "Bone", "Ebon", "Crystal", "Runed", "Gilded", "Thorned", "Hollow"};
    static const char* B[] = {"Staff", "Rod", "Stave", "Scepter", "Wand"};
    static const char* C[] = {"Embers", "Frost", "Storms", "Ruin", "the Moon", "Ash", "Whispers", "the Deep", "Thorns", "the Lich"};
    Staff s;
    s.name = std::string(A[irand(10)]) + " " + B[irand(5)] + " of " + C[irand(10)];
    int cap = std::min(12, irange(2, 4) + tier + irand(2));
    s.manaMax = (float)(80 + tier * 45 + irand(60));
    s.mana = s.manaMax;
    s.regen = (25 + tier * 12 + irand(25)) / 60.0f;
    s.delay = std::max(1, irange(3, 18) - tier);
    s.recharge = std::max(5, irange(20, 60) - tier * 5);
    s.spread = std::max(0.0f, frange(0, 10) - tier);
    s.perCast = chance(4) ? 2 : 1;
    s.slots.assign(cap, SpellCard{});
    int n = std::min(cap, irange(1, 3));
    for (int i = 0; i < n; i++)
        s.slots[i] = makeCard(randomSpell(tier));
    s.gem = ColorFromHSV(frange(0, 360), 0.6f, 1.0f);
    return s;
}

Weapon randomWeapon(int tier)
{
    Weapon w;
    if (chance(2))
    {
        w.type = W_STAFF;
        w.staff = randomStaff(tier);
        return w;
    }
    if (tier >= 1 && chance(30))
        return rollLegendary(tier, false); // a rare named weapon (never on the first stage)
    static const int types[] = {W_DAGGER, W_SWORD, W_AXE, W_SPEAR, W_MACE, W_CROSSBOW};
    w.type = types[irand(6)];
    const StageDef& sd = STAGES[std::min(tier, STAGE_COUNT - 1)];
    w.metal = sd.metals[irand(3)];
    return w;
}

// ---------------------------------------------------------------- legendary weapons
// Every legendary is rolled: base type, metal, damage, 1-3 effects, and a name and lore line
// assembled from word lists keyed to its main effect.

struct FxWords { int fx; const char* adj[3]; const char* noun[3]; Color glow; const char* desc; };
static const FxWords FXW[UF_COUNT] = {
    {UF_RANDOM, {"Whispering", "Capricious", "Fickle"}, {"Whispers", "Chance", "the Trickster"}, {190, 120, 255, 255}, "Wild edge: every cut lands differently"},
    {UF_BLEED, {"Crimson", "Thirsting", "Weeping"}, {"Wounds", "Red Rain", "the Butcher"}, {220, 40, 50, 255}, "Foes bleed out horribly"},
    {UF_BURN, {"Smouldering", "Burning", "Ember-kissed"}, {"Embers", "the Pyre", "Ash"}, {255, 130, 40, 255}, "Sets foes alight on contact"},
    {UF_CHILL, {"Frostbitten", "Rimed", "Wintry"}, {"Winter", "the Frozen Fjord", "Hoarfrost"}, {150, 220, 255, 255}, "Chills and slows foes"},
    {UF_CHAIN, {"Thundering", "Stormwrought", "Crackling"}, {"Storms", "Thunder", "the Sky-Father"}, {250, 240, 120, 255}, "Lightning leaps to nearby foes"},
    {UF_LEECH, {"Vampiric", "Hungering", "Sanguine"}, {"Hunger", "the Leech", "the Long Night"}, {200, 30, 90, 255}, "Heals you for part of the damage dealt"},
    {UF_KNOCK, {"Mighty", "Titanic", "Thudding"}, {"Giants", "the Mountain", "the Ox"}, {210, 190, 150, 255}, "Sends foes flying"},
    {UF_QUICK, {"Swift", "Darting", "Feathered"}, {"the Wind", "Haste", "the Hawk"}, {160, 255, 200, 255}, "Strikes much faster"},
    {UF_EXECUTE, {"Merciless", "Grim", "Final"}, {"the Headsman", "Endings", "the Reaper"}, {170, 170, 190, 255}, "Triple damage to badly wounded foes"},
    {UF_MINER, {"Delving", "Stonebiting", "Deep-forged"}, {"the Deep", "the Delvers", "Granite"}, {230, 190, 110, 255}, "Cuts through solid rock"},
    {UF_POISON, {"Venomous", "Blighted", "Fenborn"}, {"Venom", "the Fen", "the Serpent"}, {120, 230, 90, 255}, "Poisons foes"},
    {UF_MULTISHOT, {"Hailing", "Volleying", "Many-mouthed"}, {"Hail", "the Volley", "Arrows"}, {200, 220, 255, 255}, "Looses three bolts at once"},
    {UF_EXPLOSIVE, {"Wyrmfire", "Bursting", "Ruinous"}, {"Wyrms", "Ruin", "the Forge"}, {255, 170, 60, 255}, "Bolts burst on impact"},
};

const char* fxDescription(int bit)
{
    for (auto& f : FXW)
        if (f.fx == bit) return f.desc;
    return "";
}

template <size_t N> static const char* pickOf(const char* const (&arr)[N]) { return arr[irand((int)N)]; }

static const char* typeWord(int type)
{
    static const char* dagger[] = {"Dagger", "Dirk", "Knife", "Stiletto", "Sgian", "Seax"};
    static const char* sword[] = {"Blade", "Sword", "Longsword", "Brand", "Edge", "Claymore"};
    static const char* axe[] = {"Axe", "Cleaver", "Bearded Axe", "Splitter", "Dane-axe"};
    static const char* spear[] = {"Spear", "Pike", "Lance", "Glaive", "Javelin"};
    static const char* mace[] = {"Mace", "Hammer", "Maul", "Morningstar", "Cudgel"};
    static const char* xbow[] = {"Crossbow", "Arbalest", "Bolt-thrower", "Windlass"};
    switch (type)
    {
    case W_DAGGER: return pickOf(dagger);
    case W_AXE: return pickOf(axe);
    case W_SPEAR: return pickOf(spear);
    case W_MACE: return pickOf(mace);
    case W_CROSSBOW: return pickOf(xbow);
    default: return pickOf(sword);
    }
}

Weapon rollLegendary(int tier, bool fromStone, int forceType)
{
    static const char* myth[] = {"Hrunting", "Tyrfing", "Gram", "Skofnung", "Caladbolg", "Fragarach", "Mistilteinn", "Angurvadal",
                                 "Hofud", "Naegling", "Dyrnwyn", "Moralltach", "Laevateinn", "Gae Dearg", "Dainsleif", "Claiomh Solais"};
    static const char* owners[] = {"Fenrir", "Odin", "Cu Chulainn", "Fionn", "Thor", "Freyja", "the Morrigan", "Wayland", "Beowulf", "Grendel", "Skadi", "Lugh"};
    static const char* titles[] = {"the Oathbreaker", "the Last Light", "the Widowmaker", "the Kinslayer", "the Dawnbringer", "the Wolfsbane", "the Unbowed", "the Grave-Singer"};
    static const char* banes[] = {"Trolls", "Draugr", "Wyrms", "the Fair Folk", "Kelpies", "Giants", "Liches"};
    static const char* places[] = {"in the halls of Svartalfheim", "at Wayland's smithy", "in a Pictish hillfort", "in the fires beneath Dunmoor", "by dwarven hands in Nidavellir", "on a Hebridean storm-beach"};
    static const char* heroes[] = {"a drowned jarl", "the Red King of Alba", "a forgotten valkyrie", "a hermit of the isles", "the last of the Fianna", "a shield-maiden of Birka"};
    static const char* sites[] = {"a flooded barrow", "the belly of a troll", "a kelpie's loch", "an ash-choked crypt", "the roots of Yggdrasil", "a standing stone at midwinter"};

    Weapon w;
    static const int types[] = {W_DAGGER, W_SWORD, W_SWORD, W_AXE, W_SPEAR, W_MACE, W_CROSSBOW};
    w.type = forceType >= 0 ? forceType : types[irand(7)];
    // metal from the top two tiers allowed at this depth (stones reach one tier further)
    int maxTier = std::min(5, 1 + tier + (fromStone ? 1 : 0));
    std::vector<int> metals;
    for (int m = 0; m < METAL_COUNT; m++)
        if (METAL_TIER[m] <= maxTier && METAL_TIER[m] >= maxTier - 1) metals.push_back(m);
    w.metal = metals.empty() ? M_COPPER : metals[irand((int)metals.size())];

    // effects valid for this weapon type
    std::vector<int> pool;
    for (int i = 0; i < UF_COUNT; i++)
    {
        int f = FXW[i].fx;
        bool ranged = w.type == W_CROSSBOW;
        if ((f == UF_MULTISHOT || f == UF_EXPLOSIVE) && !ranged) continue;
        if ((f == UF_MINER || f == UF_KNOCK || f == UF_LEECH) && ranged) continue;
        pool.push_back(i);
    }
    int count = 1 + (chance(2) ? 1 : 0) + ((fromStone || (tier >= 3 && chance(3))) ? 1 : 0);
    int primary = -1;
    for (int k = 0; k < count && !pool.empty(); k++)
    {
        int idx = irand((int)pool.size());
        int fi = pool[idx];
        pool.erase(pool.begin() + idx);
        w.fx |= FXW[fi].fx;
        if (primary < 0) primary = fi;
    }
    const FxWords& pw = FXW[primary];
    w.dmgMul = frange(1.1f, 1.3f) + (fromStone ? 0.15f : 0.0f);
    w.glow = pw.glow;

    std::string tw = typeWord(w.type);
    switch (irand(4))
    {
    case 0: w.title = std::string(pw.adj[irand(3)]) + " " + tw; break;
    case 1: w.title = tw + " of " + pw.noun[irand(3)]; break;
    case 2: w.title = std::string(pickOf(myth)) + ", " + (chance(3) ? std::string("Bane of ") + pickOf(banes) : std::string(pickOf(titles))); break;
    default: w.title = std::string(pickOf(owners)) + "'s " + tw; break;
    }
    switch (irand(3))
    {
    case 0: w.lore = std::string("Forged ") + pickOf(places) + "."; break;
    case 1: w.lore = std::string("Once carried by ") + pickOf(heroes) + "."; break;
    default: w.lore = std::string("Recovered from ") + pickOf(sites) + "."; break;
    }
    return w;
}

// The special weapon each stage displays. The first stage gets honest named gear, deeper
// stages a legendary shaped to fit its display (a sword in a grave, a spear frozen in ice...).
Weapon themedWeapon(int style, int stage)
{
    const Color pale = {255, 240, 200, 255};
    if (stage == 0)
    {
        Weapon w;
        w.metal = M_IRON;
        w.glow = pale;
        if (style == DS_TARGET)
        {
            w.type = W_CROSSBOW;
            w.title = "Hunter's Crossbow";
            w.lore = "Left beside the butts by a careless yeoman.";
            w.dmgMul = 1.1f;
        }
        else
        {
            w.type = W_DAGGER;
            w.title = "Kitchen Knife";
            w.lore = "Sharper than it has any right to be.";
            w.dmgMul = 1.15f;
        }
        return w;
    }
    int type = -1;
    switch (style)
    {
    case DS_GRAVE: type = W_SWORD; break;
    case DS_CART: type = W_AXE; break;
    case DS_ICE: type = W_SPEAR; break;
    case DS_ANVIL: type = chance(2) ? W_MACE : W_SWORD; break;
    default: break;
    }
    Weapon w = rollLegendary(stage + 1, true, type);
    if (style == DS_CART) w.fx |= UF_MINER; // a delver's axe
    return w;
}

Weapon fryingPan()
{
    Weapon w;
    w.type = W_PAN;
    w.metal = M_IRON;
    w.title = "Frying Pan";
    w.lore = "Seasoned by a hundred breakfasts. Better than nothing.";
    w.dmgMul = 0.8f;
    return w;
}

float weaponDamage(const Weapon& w) { return WTYPES[w.type].dmg * METALS[w.metal].dmg * w.dmgMul * 0.85f; } // steel trails magic a little
int weaponCooldown(const Weapon& w) { return (int)(WTYPES[w.type].cooldown * ((w.fx & UF_QUICK) ? 0.6f : 1.0f)); }

void recipeCost(int type, int metal, int out[RES_COUNT])
{
    for (int r = 0; r < RES_COUNT; r++)
        out[r] = (int)std::ceil(METALS[metal].cost[r] * WTYPES[type].costMult);
}

bool canAfford(const int cost[RES_COUNT])
{
    for (int r = 0; r < RES_COUNT; r++)
        if (G.p.res[r] < cost[r])
            return false;
    return true;
}

void payCost(const int cost[RES_COUNT])
{
    for (int r = 0; r < RES_COUNT; r++)
        G.p.res[r] -= cost[r];
}

// ---------------------------------------------------------------- casting

static void applyMod(Mods& m, int sp)
{
    switch (sp)
    {
    case SP_DMG: m.dmg += 10; break;
    case SP_SPEED: m.speedMul *= 1.75f; break;
    case SP_BOUNCE: m.bounce += 3; break;
    case SP_HOMING: m.homing = true; break;
    case SP_PIERCE: m.pierce = true; break;
    case SP_IGNITE: m.el = EL_FIRE; m.elSet = true; m.trailFire = true; m.dmg += 3; break;
    case SP_FROST: m.el = EL_ICE; m.elSet = true; break;
    case SP_EXPLOSIVE: m.blast += 5; break;
    }
}

// Draw the next spell from the staff. The deck may wrap around once per cast.
static int drawSpell(Staff& s, bool& wrapped)
{
    int n = (int)s.slots.size();
    for (int guard = 0; guard < n * 2 + 1; guard++)
    {
        if (s.cursor >= n)
        {
            if (wrapped)
                return -1;
            s.cursor = 0;
            wrapped = true;
        }
        int slot = s.cursor++;
        if (s.slots[slot].id >= 0)
            return slot;
    }
    return -1;
}

// Noita-ish: modifiers draw again, multicasts add draws, projectiles consume a draw.
static std::vector<int> spellsSpent; // filled during a cast so the caller can announce them

static std::vector<Shot> castGroup(Staff& s, int draws, bool& wrapped, int& delay, int& recharge)
{
    std::vector<Shot> shots;
    Mods mods;
    while (draws > 0)
    {
        int slot = drawSpell(s, wrapped);
        if (slot < 0)
            break;
        int sp = s.slots[slot].id;
        const SpellDef& d = SPELLS[sp];
        if (s.mana < d.mana)
            continue; // not enough mana: the spell fizzles
        s.mana -= d.mana;
        if (s.slots[slot].uses > 0 && --s.slots[slot].uses == 0)
        {
            s.slots[slot] = SpellCard{}; // last charge spent: the spell is gone for good
            spellsSpent.push_back(sp);
        }
        delay += d.delay;
        recharge += d.recharge;
        if (d.type == ST_MOD)
        {
            applyMod(mods, sp);
            continue;
        }
        if (d.type == ST_MULTI)
        {
            draws += d.extra - 1;
            continue;
        }
        draws--;
        Shot sh{sp, Mods{}, nullptr};
        if (sp == SP_TRIGGER)
        {
            auto pl = std::make_shared<std::vector<Shot>>(castGroup(s, 1, wrapped, delay, recharge));
            if (!pl->empty())
                sh.payload = pl;
        }
        shots.push_back(sh);
    }
    for (auto& sh : shots)
        sh.mods = mods;
    return shots;
}

bool castStaff(Staff& s, float x, float y, float ang, bool friendly)
{
    if (s.cd > 0 || s.slots.empty())
        return false;
    bool wrapped = false;
    int delay = s.delay, recharge = 0;
    std::vector<Shot> shots = castGroup(s, s.perCast, wrapped, delay, recharge);
    if (s.cursor >= (int)s.slots.size())
        wrapped = true;
    if (wrapped)
    {
        s.cursor = 0;
        s.cd = std::max(delay, s.recharge + recharge);
    }
    else
        s.cd = std::max(delay, 1);
    if (shots.empty())
    {
        spellsSpent.clear();
        s.cd = std::max(s.cd, 10);
        return false;
    }
    fireShots(shots, x, y, ang, s.spread, friendly);
    for (int sp : spellsSpent)
        message(std::string(SPELLS[sp].name) + " has run out of charges.");
    spellsSpent.clear();
    return true;
}

void fireShots(const std::vector<Shot>& shots, float x, float y, float ang, float spread, bool friendly)
{
    for (const Shot& sh : shots)
    {
        float sp = (spread + SPELLS[sh.spell].spread) * DEG2RAD;
        if (shots.size() > 1)
            sp += 4 * DEG2RAD; // multicasts fan out a little
        spawnSpell(sh, x, y, ang + frange(-sp, sp), friendly);
    }
}

// Self-check for the staff casting rules: `sand.exe --selftest`
#include <cassert>
#include <cstdio>
void castSelfTest()
{
    auto mk = [](std::vector<int> ids) { Staff s; for (int id : ids) s.slots.push_back(id < 0 ? SpellCard{} : makeCard(id)); s.mana = s.manaMax = 1000; s.recharge = 30; s.delay = 5; return s; };
    bool wrapped = false;
    int delay = 0, rech = 0;

    Staff a = mk({SP_DOUBLE, SP_SPARK, SP_SPARK, SP_FIREBALL});
    auto shots = castGroup(a, 1, wrapped, delay, rech);
    assert(shots.size() == 2 && a.cursor == 3 && !wrapped);

    Staff b = mk({SP_TRIGGER, SP_FIREBALL});
    wrapped = false;
    shots = castGroup(b, 1, wrapped, delay, rech);
    assert(shots.size() == 1 && shots[0].payload && shots[0].payload->size() == 1 && (*shots[0].payload)[0].spell == SP_FIREBALL);

    Staff c = mk({SP_DMG, SP_HOMING, SP_SPARK});
    wrapped = false;
    shots = castGroup(c, 1, wrapped, delay, rech);
    assert(shots.size() == 1 && shots[0].mods.dmg == 10 && shots[0].mods.homing);

    Staff d = mk({SP_SPARK, -1, SP_SPARK});
    castStaff(d, 0, 0, 0, true);           // first spark, not wrapped yet
    assert(d.cursor == 1 && d.cd == 5 + SPELLS[SP_SPARK].delay);
    d.cd = 0;
    castStaff(d, 0, 0, 0, true);           // second spark reaches the end -> recharge
    assert(d.cursor == 0 && d.cd >= 30);

    Staff e = mk({SP_LIGHTNING});
    e.mana = 10;                           // not enough mana: fizzles
    wrapped = false;
    shots = castGroup(e, 1, wrapped, delay, rech);
    assert(shots.empty());

    Staff f = mk({SP_BOMB});                // limited charges are consumed and the spell vanishes
    for (int i = 0; i < 3; i++) { f.cd = 0; castStaff(f, 0, 0, 0, true); }
    assert(f.slots[0].id == -1);

    G.projs.clear();
    G.msgs.clear();
    std::printf("cast self-test passed\n");
}
