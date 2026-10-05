#pragma once
#include <raylib.h>
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include "world.h"

// ================================================================ elements

enum Element { EL_PHYS, EL_FIRE, EL_ICE, EL_SHOCK, EL_POISON, EL_COUNT };
extern const char* ELEMENT_NAMES[EL_COUNT];
extern const Color ELEMENT_COLORS[EL_COUNT];

extern const char* RES_NAMES[RES_COUNT];
extern const Color RES_COLORS[RES_COUNT];

// ================================================================ weapons & armour

enum Metal { M_COPPER, M_IRON, M_STEEL, M_DAMASCUS, M_FIRESTONE, M_FROSTITE, M_STORMITE, M_VENOMITE, M_ADAMANTIUM, METAL_COUNT };

struct MetalDef
{
    const char* name;
    Color color;
    float dmg;    // damage multiplier
    int mine;     // hardest material it can break
    Element el;
    float armor;  // damage reduction as armour
    int cost[RES_COUNT];
};
extern const MetalDef METALS[METAL_COUNT];

enum WeaponType { W_DAGGER, W_SWORD, W_AXE, W_CROSSBOW, W_STAFF, W_ARMOUR, W_SPEAR, W_MACE, W_PAN, WTYPE_COUNT };
inline bool isMelee(int t) { return t == W_DAGGER || t == W_SWORD || t == W_AXE || t == W_SPEAR || t == W_MACE || t == W_PAN; }

// Special effects rolled onto legendary weapons (bit flags).
enum WeaponFx {
    UF_RANDOM = 1, UF_BLEED = 2, UF_BURN = 4, UF_CHILL = 8, UF_CHAIN = 16, UF_LEECH = 32, UF_KNOCK = 64,
    UF_QUICK = 128, UF_EXECUTE = 256, UF_MINER = 512, UF_POISON = 1024, UF_MULTISHOT = 2048, UF_EXPLOSIVE = 4096,
    UF_COUNT = 13
};

struct WeaponTypeDef
{
    const char* name;
    float dmg;
    int range;
    int cooldown;
    float arc;        // degrees either side of aim
    float mineChance;
    float costMult;
};
extern const WeaponTypeDef WTYPES[WTYPE_COUNT];

// ================================================================ scrolls: one-shot spells, found or bought (items.cpp)

enum ScrollId { SC_FIREBOLT, SC_LIGHTNING, SC_BLOODSPEAR, SC_FROSTNOVA, SC_METEOR, SC_VENOM, SC_COUNT };
struct ScrollDef { const char* name; const char* desc; Color col; int minTier; };
extern const ScrollDef SCROLLS[SC_COUNT];
const int SCROLL_CASE = 5; // how many a player can carry

// ================================================================ spells (the casting engine scrolls are built on)

enum SpellType { ST_PROJ, ST_MOD, ST_MULTI };
enum SpellId {
    SP_SPARK, SP_MISSILE, SP_FIREBALL, SP_ICE, SP_LIGHTNING, SP_ACID, SP_BOMB, SP_DIG, SP_WATER, SP_TRIGGER,
    SP_DMG, SP_SPEED, SP_BOUNCE, SP_HOMING, SP_PIERCE, SP_IGNITE, SP_FROST, SP_EXPLOSIVE,
    SP_DOUBLE, SP_TRIPLE,
    SP_BLOODSPEAR, // scroll-only: a heavy, fast, piercing spear of red iron
    SPELL_COUNT
};

struct SpellDef
{
    const char* name;
    const char* abbr;
    SpellType type;
    int mana, delay, recharge;
    float spread, dmg, speed;
    int life;
    Element el;
    float grav;
    int blast;
    int extra;    // multicast count
    int minTier;  // earliest stage it drops
    Color col;
    const char* desc;
    int uses;     // charges per spell; 0 = unlimited. Charges never come back.
};
extern const SpellDef SPELLS[SPELL_COUNT];

struct Mods
{
    float dmg = 0, speedMul = 1, grav = 0;
    int bounce = 0, blast = 0;
    bool homing = false, pierce = false, trailFire = false, elSet = false;
    Element el = EL_PHYS;
};

struct Shot
{
    int spell;
    Mods mods;
    std::shared_ptr<std::vector<Shot>> payload; // trigger spells carry the next cast
};

// A spell as an item: which spell, and how many charges are left (-1 = unlimited).
struct SpellCard
{
    int id = -1;
    int uses = -1;
};
SpellCard makeCard(int id);

struct Staff
{
    std::string name;
    float manaMax = 100, mana = 100, regen = 0.5f;
    int delay = 10, recharge = 30, perCast = 1, cd = 0, cursor = 0;
    float spread = 0;
    std::vector<SpellCard> slots; // id -1 = empty
    Color gem = SKYBLUE;
};

struct Weapon
{
    int type = W_SWORD;
    int metal = M_COPPER;
    Staff staff; // only used when type == W_STAFF
    // legendary / named weapons
    std::string title, lore;
    int fx = 0;
    float dmgMul = 1;
    Color glow = BLANK; // a = 0 means not legendary
};

// ================================================================ creatures

enum EnemyType {
    E_GOBLIN, E_BOMBER, E_SKELETON, E_ARCHER, E_BAT, E_SLIME, E_CULTIST, E_KNIGHT,
    E_IMP, E_WRAITH, E_GOLEM,
    E_WOLF, E_REDCAP, E_DRAUGR, E_TROLL, E_BANSHEE, E_KELPIE, E_GUARD, // Norse & Scottish folklore
    E_RISEN, // the dead levy on the field before Dunmoor
    E_SERPENT, E_SCORPION, E_RAIDER, // the open sea; the desert east of Dunmoor
    E_BLACKKNIGHT, E_LICH, ENEMY_COUNT
};
enum AIType { AI_WALK, AI_RANGED, AI_FLY, AI_HOP, AI_BOMB, AI_FLYCAST, AI_BOSS_KNIGHT, AI_BOSS_LICH };

struct EnemyDef
{
    const char* name;
    int w, h;
    float hp, speed, dmg;
    AIType ai;
    Element el;
    float resist[EL_COUNT]; // damage multipliers
    int range, cooldown;
    bool noclip;
    Color blood;
    CellMaterial gore;
};
extern const EnemyDef ENEMIES[ENEMY_COUNT];

struct Mob
{
    int type = -1; // -1 = player
    int tier = 0;  // the depth it was made for (G.stage when spawned): scales its drops
    bool sea = false; // it lives in the open sea: it may carry an Önd orb
    int id = 0;
    float x = 0, y = 0, vx = 0, vy = 0;
    int w = 6, h = 10;
    bool onGround = false, inLiquid = false, alive = true, aggro = false, los = false, boss = false;
    int wall = 0, facing = 1;
    float hp = 100, maxHp = 100, dmg = 0;
    int burn = 0, chill = 0, shock = 0, poison = 0, iframes = 0, hurtFlash = 0, envCd = 0;
    int wet = 0, oily = 0, bloody = 0; // frames left coated: wet won't catch fire but conducts; oil burns longer and hotter
    int cd = 0, timer = 0, state = 0, attackT = 0;
    int bleedMark = 0; // struck by a bleeding weapon: dies messily
    int dropT = 0;     // frames left falling through one-way platforms
    int stuckT = 0;    // frames spent wedged inside rock or rubble
    int stagger = 0, hitT = 0, hitSwing = 0; // reeling from a blow (no control), the recoil animation, the last swing that struck it
    float hitDir = 0;  // which way that blow knocked it
    int atkPhase = 0, atkT = 0; // a melee attack: 1 winding up, 2 striking, 3 recovering; frames left in the phase
    bool atkDone = false;       // this attack has already landed
    float anim = 0, squash = 1;
    // the last direct blow (HitKind), its direction and force: how the body goes when it dies
    uint8_t lastHit = 0, nWounds = 0;
    float lastAng = 0, lastK = 0;
    // stab and bolt wounds, on a limb of its rig (segment woundS, woundT along it, the blow's angle woundA off
    // the limb's): they bleed, and a bolt stays stuck in the body, moving with the limb
    float woundT[4] = {}, woundA[4] = {};
    uint8_t woundS[4] = {}, woundK[4] = {};
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

// Amulets: Norse charms worn on a cord, one at a time. Found rarely in chests, on the fallen, and on
// every guardian; walk over one bare-necked to put it on, or press F over it to swap.
enum AmuletId { AM_MJOLNIR, AM_VALKNUT, AM_HELM, AM_VEGVISIR, AM_TROLLCROSS, AM_YGGDRASIL, AM_NJORD, AM_JORMUNGANDR, AM_SKADI, AM_BROKKR, AMULET_COUNT };
struct AmuletDef { const char* name; const char* desc; Color col; const char* art[20]; };
extern const AmuletDef AMULETS[AMULET_COUNT];
void wearAmulet(int id); // -1 takes it off
int randomAmulet();      // one you're not already wearing
// Pixel art from char rows (see items.cpp:artColor), `px` units (or pixels) a pixel, top-left at (x, y).
void drawPixelArt(const char* const* rows, int n, float x, float y, float px);
void drawAmulet(int id, float x, float y, float px);
extern const char* const MEAD_ART[8];

const int CAPE_N = 12; // points down the player's cape
struct Player
{
    Mob m;
    std::vector<Weapon> hotbar;
    int sel = 0;
    std::vector<SpellCard> bag; // loose spells
    int res[RES_COUNT] = {};
    int pending[RES_COUNT] = {};
    int armour = -1; // metal index, -1 = padded gambeson
    int potions = 2;
    int bombs = 0; // rune bombs: rare finds, thrown with B
    int ondT = 0; // frames left of Önd: picked up (only the sea gives it), you need no air
    int amulet = -1; // AmuletId worn, -1 none
    float stamina = 100;
    float breath = 100; // under water it runs out, then you drown
    int attackCd = 0, swingT = 0, swingDir = 1, recoil = 0;
    int atkStyle = 0, atkLen = 1, atkHitAt = 0, combo = 0, comboT = 0, swingId = 0; // the melee attack under way (swingT: frames left)
    float aim = 0;
    int coyote = 0, jumpBuf = 0, onWall = 0;
    int mantleT = 0, mantleDir = 0; // pulling up over a ledge: frames left, and which way
    float mx0 = 0, my0 = 0, mx1 = 0, my1 = 0;
    int hook = 0; // 0 none, 1 flying, 2 attached
    float hx = 0, hy = 0, hvx = 0, hvy = 0, rope = 0;
    int hookTravel = 0;
    bool hasWisp = false; // Baldr's Offering: a light spirit that follows you
    float wx = 0, wy = 0;
    int kills = 0;
    int combatT = 0; // weapon stays drawn this many frames after attacking
    int coins = 0;   // banked between runs
    std::vector<int> scrolls; // ScrollIds carried, read one at a time (R); the selected one is `scrollSel`
    int scrollSel = 0;
    int readT = 0;            // frames left of the casting pose after reading a scroll
    bool hasMap = false;      // the Wayfinder's Map: a minimap in the HUD
    bool hasHook = true, crouch = false, prone = false, climb = false; // posture: standing / crouched (16 tall) / prone (9 tall); climb = hanging on a rope
    // animation state
    float squash = 1, runPhase = 0;
    int rollT = 0, rollCd = 0, rollDir = 1;
    bool wasGround = false;
    Vector2 cape[CAPE_N] = {}, capePrev[CAPE_N] = {};
    bool capeInit = false;
};

// ================================================================ world objects

enum ProjKind { PK_SPELL, PK_BOLT, PK_ARROW, PK_BOMB, PK_ROCK };

struct Proj
{
    int kind = PK_SPELL, spell = -1;
    float x = 0, y = 0, vx = 0, vy = 0;
    float dmg = 0;
    Element el = EL_PHYS;
    int life = 60, bounce = 0, blast = 0, power = 3;
    float grav = 0;
    bool homing = false, pierce = false, friendly = true, trailFire = false, fuse = false, alive = true;
    Color col = WHITE;
    std::shared_ptr<std::vector<Shot>> payload;
    std::vector<int> hits;
};

enum PickupKind { PU_SCROLL, PU_WEAPON, PU_POTION, PU_HEART, PU_COIN, PU_AMULET, PU_MEAD, PU_BOMB, PU_OND }; // PU_AMULET: `spell` = AmuletId; PU_MEAD heals you whole
struct Pickup
{
    Mob b;
    int kind = PU_SCROLL; // PU_SCROLL: `spell` = ScrollId
    int spell = 0;
    Weapon weapon;
    int age = 0;
    bool alive = true;
};

enum InteractType { IT_CHEST, IT_ANVIL, IT_SHRINE, IT_TORCH, IT_STONE, IT_SHOP, IT_ROPE, IT_CRATE, IT_BOAT, IT_LANTERN, IT_PROP, IT_DECOR };
// IT_DECOR: a background sprite standing on the floor at (x, y): `data` = DecorKind, `style` = variant (bit 6 flips it),
// `w` = the kind's one free dimension in units (a table's length, a post's height, a hanging's drop). decor.cpp paints
// everything from DK_DRESSER on; decorAnchor says whether (x, y) is its foot, its hanging point or its centre.
enum DecorKind { DK_CACTUS, DK_BUSH, DK_SKELETON, DK_GIANT,
                 DK_DRESSER, DK_TABLE, DK_PICTURE, DK_TOOL, DK_SHELF, DK_RACK, DK_ARROWS, DK_BUNK, DK_HEARTH, DK_SHIELD, DK_POST, DK_LADDER, DK_YARD,
                 DK_TAPESTRY, DK_DRAPE, DK_CHAIN, DK_BLOOD, DK_HORNS, DK_ANTLERS, DK_DRAGONPILLAR, DK_IDOL, DK_CRANE, DK_SPIKE,
                 DK_LEANSHIELD, DK_SPEARPOST, DK_TARGET, DK_BOWRACK, DK_COBWEB, DK_LEAK };
Image decorImageFine(int kind, int var, int size);
int decorAnchor(int kind, int var);
Color clothTone(int var, int t);
void exportDecorSheet(const char* path);
const int LS_CX = 65, LS_KEEL = 47, LM_CX = 52, LM_H = 132; // the hull image's centre column and keel row; the mast image's centre column and height (tools/ship.py)
Image longshipImage(int part); // decor.cpp: 0 hull, 1 mast, 2 broken hull, 3 broken mast (half a unit per pixel)
void loadStep(float frac, const char* what); // main.cpp: redraws the loading screen with a progress bar (no-op without a window)
Image stallImageFine(int kind, int layer); // Hearthwick's market stalls, layers 0 (back) and 1 (counter), 144 x 124 px
// IT_BOAT: a longship; `used` = beached scenery, otherwise F sets sail. IT_TORCH style 1 = wall sconce.
// IT_ROPE: hangs from (x, y) down to row `data`. IT_CRATE: breakable obstacle, wood cells in [x, x+w) x [y-h, y), `data` = hits left.
struct Interact { int type; float x, y; bool used = false; int data = 0; int style = 0; int w = 0, h = 0, cells = 0, hit = 0;
                  float vx = 0, vy = 0, ang = 0, va = 0; int rest = 99; int fade = 0; }; // chests are little rigid bodies: velocity, spin, frames at rest; fade: an opened chest's frames left before it is gone (-1 gone)
// IT_LANTERN: an oil lantern on a chain from (x, y), `data` units long, swinging at `ang`. `style` 1 = the chain
// snapped and it's falling free (x, y is then the lantern itself); `used` = smashed.
inline Vector2 lanternPos(const Interact& it) { return it.style ? Vector2{it.x, it.y} : Vector2{it.x + std::sin(it.ang) * it.data, it.y + std::cos(it.ang) * it.data}; }
const float CHEST_HW = 9.5f, CHEST_HH = 7.2f; // a chest's half size in units; (x, y - CHEST_HH) is its centre
// IT_PROP: a loose crate (style 0), barrel (1) or small box (2): a rigid body like a chest, knocked about by
// blows, bolts, blasts and anyone walking into it. bodyHalf: the half size of any such body.
inline Vector2 bodyHalf(const Interact& it)
{
    static const Vector2 PROP[3] = {{6, 6}, {4.5f, 6.5f}, {4, 3.5f}}, PLANK[3] = {{5, 1.5f}, {8, 1.5f}, {12, 1.5f}}; // styles 3, 6, 9: a floating plank, short, medium, long
    return it.type == IT_PROP ? (it.style >= 3 ? PLANK[(it.style / 3 - 1) % 3] : PROP[it.style % 3]) : Vector2{CHEST_HW, CHEST_HH};
}
inline bool isBody(const Interact& it) { return (it.type == IT_CHEST && it.fade >= 0) || it.type == IT_PROP; }
// How a stage shows off its special weapon (replaces the old sword-in-stone).
enum DisplayStyle { DS_ROCK, DS_TARGET, DS_TABLE, DS_RACK, DS_GRAVE, DS_CART, DS_ICE, DS_ANVIL, DS_ALTAR };

enum TrapType { TR_ARROW, TR_FLAME, TR_COLLAPSE };
struct Trap
{
    int type;
    int x, y, dir = 1, timer = 0;
    bool done = false;
    int rx0 = 0, ry0 = 0, rx1 = 0, ry1 = 0;
    int hp = 3, hit = 0; // dart traps: blows left before the head breaks off, frames of the hit flash
};

struct Particle
{
    float x, y, vx, vy;
    int life;
    Color col;
    float grav;
    CellMaterial toCell = CellMaterial::Empty;
    uint8_t cellFlags = 0;
};

// A light with no object of its own (lanterns, lit windows); `flame` also draws a flickering flame.
struct Lamp { float x, y, r; Color c; bool flame = false; bool smoke = false; float beam = 0, w = 0, wh = 0, dim = 1; }; // smoke: no light, just a curl of smoke (a hall's roof); beam: a window's moonlight, falling this far (w wide)

// A safe area between two biomes. You walk in through `x0`, the gate drops behind you, and the far
// gate at `x1` opens onto the next biome. A boss's haven stays barred until the boss falls.
const int HAVEN_WALL = 8, HAVEN_DOOR = 46; // gates fill the doorway through each end wall
struct Haven
{
    int stage = 0;           // the biome it leads out of
    int x0 = 0, x1 = 0, top = 0, floor = 0;
    bool sealed = false, locked = false, announced = false;
    int bossId = 0;
    const char* name = "";
};

struct FloatText { float x, y; std::string s; int life; Color col; };

// How a creature was last struck, which decides how it falls: cut in two, flung, blown apart.
enum HitKind { HK_NONE, HK_SLASH, HK_CHOP, HK_BLUNT, HK_PIERCE, HK_BOLT, HK_BLAST };
enum WoundKind { WK_PIERCE, WK_BOLT, WK_CUT };

// A little rigid body: centre, velocity, angle and spin, frames at rest (asleep past 40).
struct RigidBody { float x = 0, y = 0, vx = 0, vy = 0, ang = 0, va = 0; int rest = 0; };
// One step against the terrain: `pts` is its outline about the centre, `I` its inertia per unit mass;
// `square` snaps it level when it settles nearly so. Returns whether it's touching anything.
bool rigidStep(RigidBody& b, const std::vector<Vector2>& pts, float I, bool square, float buoy = 0); // buoy: lift per step when fully under water (0 = sinks)

// A dead body: the creature's limb rig (rig.cpp) as a Verlet ragdoll - joints as points, limbs as sticks.
// Fixed arrays: nothing is allocated while it falls.
struct RigSpec;
const int RJ_COUNT = 18, RST_MAX = 32, RCW_MAX = 10;
struct CorpseWound { uint8_t a, b; float t, ang; int kind; float pressure; }; // on joints a->b at t, pointing `ang` off that line
struct Corpse
{
    const RigSpec* rig = nullptr;
    int facing = 1;
    Vector2 p[RJ_COUNT] = {}, pp[RJ_COUNT] = {}; // points now, and a frame ago
    Vector2 snap[RJ_COUNT] = {};                 // where they were a few frames back (to tell when it's stopped)
    uint8_t sa[RST_MAX] = {}, sb[RST_MAX] = {}, sg[RST_MAX] = {}; // sticks: ends, and what cutting it severs
    float sl[RST_MAX] = {};
    int ns = 0;
    uint32_t used = 0; // joints in use
    uint8_t cut = 0;   // severed groups
    CorpseWound w[RCW_MAX] = {};
    int nw = 0;
    CellMaterial gore = CellMaterial::Blood;
    int life = 0, rest = 0, awakeT = 0; // frames alive; frames still; frames since last woken
};

// Verlet ragdoll: 9 joints joined by sticks, colours picked from the creature.
struct Ragdoll
{
    Vector2 p[9], pp[9];
    float len[9];
    float scale = 1;
    int life = 0;
    Color head, body, limb;
    bool bony = false;
    CellMaterial gore = CellMaterial::Blood;
};

// ================================================================ stages

struct Weighted { int id, w; };
struct StageDef
{
    const char* name;
    const char* subtitle;
    Color bgA, bgB;
    CellMaterial base, alt, alt2, top;
    Weighted ores[6];
    Weighted enemies[6];
    CellMaterial liquid1, liquid2;
    int liquidCount, enemyCount;
    int crates, kegs, vats, spikes, arrows, flames, collapses, miasma;
    int boss; // enemy type or -1
    bool surface;
    int metals[3]; // what weapon drops are made from
    int kind;      // StageKind
    int hang;      // ceiling decoration: -1 none, 0 roots, 1 icicles, 2 chains
};
enum StageKind { SK_PLAINS, SK_CASTLE, SK_CRYPT, SK_MINES, SK_CAVE };
extern const StageDef STAGES[];
extern const int STAGE_COUNT;

// ================================================================ game

enum GameState { GS_TITLE, GS_LOADING, GS_PLAY, GS_INVENTORY, GS_ANVIL, GS_PAUSE, GS_DEAD, GS_WIN, GS_SHOP };
enum LoadTarget { LOAD_STAGE, LOAD_SANDBOX, LOAD_VILLAGE };

// ================================================================ meta progression (meta.cpp)

enum UnlockKind { UK_WEAPON, UK_SCROLL, UK_HOOK, UK_ARMOUR, UK_FLASK, UK_WISP, UK_MAP };
struct Unlock { const char* name; int shop, price, kind, a, b; const char* desc; };
extern const Unlock UNLOCKS[];
extern const int UNLOCK_COUNT;
extern const char* SHOP_NAMES[3];
// Weapons and scrolls are bought for the next run only (`stocked`); the permanent gear (hook, armour, flask, wisp, map) is kept for good.
inline bool isKitKind(int kind) { return kind == UK_WEAPON || kind == UK_SCROLL; }
struct Meta
{
    int bank = 0, runs = 0, deepest = 0;
    bool owned[64] = {}, equipped[64] = {}, stocked[64] = {};
};
extern Meta META;
void loadMeta();
void saveMeta();
void toggleEquip(int i);
bool buyUnlock(int i);
void sellBack(int i);  // a readied weapon or scroll, refunded
void spendKit();       // setting sail: the readied weapons and scrolls go with you, and are gone from the stalls
void bankRun();
void applyLoadout();

struct Game
{
    GameState state = GS_TITLE;
    bool sandbox = false, inVillage = false, banked = false;
    bool sanctuary = false; // standing inside a haven
    int shopId = 0;
    int stage = 0;
    Player p;
    std::vector<Mob> mobs;
    std::vector<Proj> projs;
    std::vector<Pickup> pickups;
    std::vector<Particle> parts;
    std::vector<Trap> traps;
    std::vector<Interact> inter;
    std::vector<Lamp> lamps;
    std::vector<Haven> havens;
    std::vector<FloatText> texts;
    std::vector<Ragdoll> rags;
    std::vector<Corpse> corpses;
    std::vector<Weapon> stoneLoot; // weapons held by sword-in-stone shrines (Interact::data indexes this)
    std::vector<std::pair<std::string, int>> msgs;
    float camX = 0, camY = 0, shake = 0;
    int rcx = 0, rcy = 0; // render camera (whole pixels)
    int scale = 3, vw = 0, vh = 0;
    int frame = 0, nextId = 1;
    int winTimer = 0, deadTimer = 0, bannerTimer = 0;
    SpellCard heldSpell;
    int invSel = 0;
    int nearInteract = -1;
    int brushMat = 2, brushR = 3;
    int loadTarget = LOAD_STAGE;
    int hitstop = 0;
    float uiScale = 1; // accessibility: UI size multiplier
    bool showHelp = false, reduceShake = false;
    bool dev = false, devGod = false, devFly = false, devMap = false; // dev tools (F2, dev.cpp)
    int playX0 = 0;   // the gate you last came through: nothing behind it is reachable any more
    int seaEnd = 0;   // the run's west sea: x where the beach takes over from it (0 = none)
    Rectangle desert{}; // the Scorched Reach east of Dunmoor, in units (width 0 = none)
    float roamX0 = 0, roamX1 = 0; // in Hearthwick: how far you may walk either way (0, 0 = anywhere)
    bool underwater = false; // the player's head is under
    float stormX0 = 0, stormX1 = 0; // the storm over Dunmoor gathers from x0 to x1 (0 = none)
    int thunderT = 0;               // frames till the thunder after a flash
    int duneEnd = 0;  // stage 1 opens with the Whispering Dunes: x where they give way to the Greenmarch (0 = none)
    int sailT = 0;    // frames into the voyage out of Hearthwick (0 = not sailing)
    bool duneCrossed = false;
};
extern Game G;
// The named stretches that aren't a stage of their own: 1 the dunes, 2 the open sea, 3 the desert (0: the stage's own name).
inline int regionId()
{
    if (G.sanctuary || G.inVillage || G.sandbox) return 0;
    float x = G.p.m.cx(), y = G.p.m.cy();
    if (G.seaEnd && x < G.seaEnd - 60) return 2;
    if (G.desert.width > 0 && x > G.desert.x + 40 && y < G.desert.y + G.desert.height) return 3;
    if (G.duneEnd > 0 && G.p.m.x < G.duneEnd && G.p.m.x > G.seaEnd) return 1;
    return 0;
}
inline const char* regionName(int r) { static const char* n[] = {"", "The Whispering Dunes", "The Drowned Deep", "The Scorched Reach"}; return n[r]; }
inline const char* regionSub(int r) { static const char* n[] = {"", "Only the wind lives here", "It grows darker, and hungrier, the further out you swim", "Sand, sun-bleached bones, and things with stings"}; return n[r]; }
inline bool inDunes() { return G.duneEnd > 0 && G.p.m.x < G.duneEnd && G.p.m.x > G.seaEnd && !G.sanctuary && !G.inVillage; }

// items.cpp
std::string weaponName(const Weapon& w);
float weaponDamage(const Weapon& w);
inline float spellPower() { return 1.5f * (1 + 0.2f * G.stage); } // your magic deepens as you descend
int weaponTier(const Weapon& w);
bool onPlatform(const Mob& m);
int weaponCooldown(const Weapon& w);
Weapon rollLegendary(int tier, bool fromStone, int forceType = -1);
Weapon themedWeapon(int style, int stage);
Weapon starterSword(); // the weathered Norse sword every run begins with
const char* fxDescription(int bit);
Staff randomStaff(int tier);
Weapon randomWeapon(int tier);
int randomSpell(int tier);
int randomScroll(int tier);
bool castScroll(int id, float x, float y, float ang); // reads a scroll's spell once, from (x, y) toward ang
bool readScroll();                                      // the selected scroll, from the player (entities.cpp)
void drawMinimap(float x, float y, float size, float u); // minimap.cpp
void drawScroll(int sc, float cx, float cy, float w, float glow); // a parchment scroll sealed in its colour (ui.cpp), w wide
bool castStaff(Staff& s, float x, float y, float ang, bool friendly);
void fireShots(const std::vector<Shot>& shots, float x, float y, float ang, float spread, bool friendly);
void recipeCost(int type, int metal, int out[RES_COUNT]);
bool canAfford(const int cost[RES_COUNT]);
void payCost(const int cost[RES_COUNT]);
float armourDef(int armour);
void castSelfTest();
void collapseSelfTest();

// entities.cpp
enum DamageFlags { DMG_HIT = 1 };
void newGameKit(bool sandbox);
void updateGame();
void drawEntities(int camX, int camY);
Mob makeEnemy(int type, float x, float y);
void damageMob(Mob& m, float dmg, Element el, float kx, float ky, int flags);
void explode(float x, float y, int r, float dmg, Element el, bool friendly, int power);
void spawnParticle(float x, float y, float vx, float vy, int life, Color col, float grav);
void spawnSpell(const Shot& sh, float x, float y, float ang, bool friendly);
void spawnOreBurst(float x, float y, int count, float power = 1); // power: how hard it is flung
void addPickupScroll(float x, float y, int scroll);
void addPickupWeapon(float x, float y, const Weapon& w);
void addPickup(float x, float y, int kind);
void addCoins(float x, float y, int count, int value);
void placeDisplay(float x, float y, int style, const Weapon& w);
void hitCrateAt(int x, int y, int dmg, float kx);
void drawGlow(float x, float y, Color c, float r);
void drawWorldWeapon(const Weapon& w, Vector2 grip, float ang, float len);
void addText(float x, float y, const std::string& s, Color col);
void message(const std::string& s);
bool boxSolid(float x, float y, int w, int h);
Vector2 mouseWorld();
void syncRenderCamera();
void travelOnward();
void returnToRoad();
int roadFloorAt(int x);
// Dev profiler: milliseconds per section of a frame, eased over ~10 frames (shown by the F3 dev overlay).
enum ProfSec { PF_PLAYER, PF_MOBS, PF_PROJ, PF_ITEMS, PF_DAMP, PF_SIM, PF_STRUCT, PF_BLAST, PF_FX, PF_CORPSE, PF_WORLD, PF_LIGHT, PF_DRAW, PF_HUD, PF_COUNT };
extern float PROF[PF_COUNT];
extern const char* const PROF_NAMES[PF_COUNT];
inline void profLap(double& t0, int sec) { double t = GetTime(); PROF[sec] += ((float)((t - t0) * 1000) - PROF[sec]) * 0.1f; t0 = t; } // -1 where there's no single road (castle, village...)
void shiftEntities(float dx, float dy); // the world was re-cut: move everything that lives in world coordinates
const char* sandboxBrushName();
// dev.cpp: F2 dev tools; devUpdate runs before updateGame each play frame, devDraw over the HUD
void devUpdate();
void devDraw();
std::vector<Rectangle> devPieceRects(); // each biome in the live world, in units (levelgen.cpp)
int fallingBodies();                    // collapsed slabs in flight (entities.cpp)

// rig.cpp (animation)
void drawPlayerRig(int camX, int camY); // the old posed-limb hero (rig.cpp), no longer used for the player
void drawPlayerViking(int camX, int camY); // the player from the baked Viking sheets (viking.cpp)
void drawMobAnimated(const Mob& m, int camX, int camY);
int windupFor(int type); // an enemy's melee wind-up, in frames
void drawFlame(float x, float y, float s, int seed); // a live flame rooted at (x, y), ~7s units tall
void drawBurning(const Mob& m, int camX, int camY); // flames licking up off anything on fire
void burstSprite(const Mob& m);
void drawHeld(float ox, float oy);
void detail2x(const Color* src, int w, int h, std::vector<Color>& out); // pixel art doubled: Scale2x, a lit rim, shade
void prepareStallArt(); // builds the stalls' upscaled art (outside any render texture)
enum AttackStyle { ATK_SLASH, ATK_STAB, ATK_THRUST, ATK_CHOP, ATK_SLAM, ATK_SWEEP };
void attackPose(float& ang, float& ext, int back);
void startAttack(const Weapon& w);
void drawWeaponSprite(const Weapon& w, Vector2 at, float ang, float scale, bool centred);
bool weapon3dDraw(const Weapon& w, Vector2 at, float ang, float scale, bool centred); // viking.cpp: the 3D-modelled weapon icons
float weapon3dLength(const Weapon& w);
float weaponLength(const Weapon& w);
void spawnRagdoll(float cx, float bottom, float h, float vx, float vy, Color head, Color body, Color limb, bool bony, CellMaterial gore);
void pushRagdolls(float x, float y, float radius, float force);
void updateRagdolls();
void drawRagdolls(int camX, int camY);
void spawnCorpse(const Mob& m);
bool rigLocate(const Mob& m, Vector2 at, float ang, uint8_t& seg, float& t, float& rel); // which limb a blow at `at` struck
Vector2 rigWoundPos(const Mob& m, int k);
Vector2 corpseCentre(const Corpse& c);
void corpseKick(Corpse& c, Vector2 at, Vector2 v);
void shiftCorpses(float dx, float dy);
void updateCorpses();
void drawCorpses(int camX, int camY);
void clearCorpses();
void ragdollForPlayer();
struct Sprite;
void drawSpriteBig(const Sprite& s, float x, float bottom, bool flip, Color tint);
void drawSpriteNative(const Sprite& s, float x, float bottom, bool flip);
void drawChest(float cx, float cy, float ang, bool open, Color tint = WHITE, float sink = 0); // centre, in render-texture units
void drawBomb(float cx, float cy, float ang, float scale);
void drawOnd(float cx, float cy, float scale); // an Önd orb, centred
void drawDecor(const Interact& it, float x, float y); // IT_DECOR, feet at (x, y) on screen (units) // a rune bomb, centred on its body
void drawDartTrap(float faceX, float mouthY, int dir, bool broken, int hit, int hp = 3); // the wall face it's set in, the height it fires at
int folkLooks(int kind); // how many looks a villager kind (0 man, 1 woman, 2 child) has
void drawFolk(int kind, int look, float anim, bool walking, int dir, float x, float y, Color coat, int seed); // a villager, feet at (x, y)
void drawSpriteTint(const Sprite& s, float x, float bottom, bool flip, Color tint); // native size, 'a' tinted

// levelgen.cpp
void startRun();      // the first biomes of a new run, stitched together
void advanceWorld(Haven& h); // h just sealed: grow the next biome on ahead
void setGate(int x0, int x1, int y0, int y1, bool closed);
void generateSandbox();
void generateVillage();
void dumpStages(const char* dir);
void growDampCaves(); // the damp caves fill in as the camera nears them

// audio.cpp (everything is synthesised at startup)
enum Sfx {
    SFX_SWING, SFX_HIT, SFX_CLANG, SFX_EXPLODE, SFX_CAST, SFX_FIRE, SFX_ZAP, SFX_ICE, SFX_BOW, SFX_JUMP,
    SFX_LAND, SFX_STEP, SFX_HURT, SFX_DIE, SFX_PICKUP, SFX_ORE, SFX_CHEST, SFX_PORTAL, SFX_CLICK, SFX_CRAFT,
    SFX_ROLL, SFX_ROAR, SFX_POTION, SFX_SPLASH, SFX_HOOK, SFX_BARK, SFX_HOWL, SFX_KNOCK, SFX_SMASH, SFX_THRUST, SFX_SLAM, SFX_HEAVY, SFX_XBOW, SFX_DING, SFX_DRAW, SFX_WHOOSH, SFX_COUNT
};
void initAudio();
void closeAudio();
void updateAudio(bool inGame);
void playSfx(int id, float vol = 1, float pitch = 1, float pan = 0.5f);
void playAt(int id, float x, float y, float vol = 1, float pitch = 1);

// ui.cpp
void drawHUD();
void updateDrawInventory();
void updateDrawAnvil();
void updateDrawShop();
void initUI();
void uiText(const std::string& s, float x, float y, float size, Color c, int style = 0); // style: 0 body, 1 bold, 2 title
void uiTextCentered(const std::string& s, float cx, float y, float size, Color c, int style = 0);
float uiTextWidth(const std::string& s, float size, int style = 0);
void drawResIcon(int r, float x, float y, float size);
void cancelInventoryDrag();
void drawItemIcon(const Weapon& w, float x, float y, float size);
void drawSpellIcon(int spell, float x, float y, float size, bool highlight, int uses = -1);
void drawSpellPickup(int spell, float cx, float cy, float phase); // a spell lying in the world: a glowing rune-tablet
