#pragma once
#include <raylib.h>
#include <vector>
#include <string>
#include <memory>
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

// ================================================================ spells (Noita-style staves)

enum SpellType { ST_PROJ, ST_MOD, ST_MULTI };
enum SpellId {
    SP_SPARK, SP_MISSILE, SP_FIREBALL, SP_ICE, SP_LIGHTNING, SP_ACID, SP_BOMB, SP_DIG, SP_WATER, SP_TRIGGER,
    SP_DMG, SP_SPEED, SP_BOUNCE, SP_HOMING, SP_PIERCE, SP_IGNITE, SP_FROST, SP_EXPLOSIVE,
    SP_DOUBLE, SP_TRIPLE,
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
    int id = 0;
    float x = 0, y = 0, vx = 0, vy = 0;
    int w = 6, h = 10;
    bool onGround = false, inLiquid = false, alive = true, aggro = false, los = false, boss = false;
    int wall = 0, facing = 1;
    float hp = 100, maxHp = 100, dmg = 0;
    int burn = 0, chill = 0, shock = 0, poison = 0, iframes = 0, hurtFlash = 0, envCd = 0;
    int cd = 0, timer = 0, state = 0, attackT = 0;
    int bleedMark = 0; // struck by a bleeding weapon: dies messily
    int dropT = 0;     // frames left falling through one-way platforms
    float anim = 0, squash = 1;
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

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
    float stamina = 100;
    int attackCd = 0, swingT = 0, swingDir = 1, recoil = 0;
    float aim = 0;
    int coyote = 0, jumpBuf = 0, onWall = 0;
    int hook = 0; // 0 none, 1 flying, 2 attached
    float hx = 0, hy = 0, hvx = 0, hvy = 0, rope = 0;
    int hookTravel = 0;
    int kills = 0;
    int combatT = 0; // weapon stays drawn this many frames after attacking
    int coins = 0;   // banked between runs
    bool hasHook = true, crouch = false, prone = false, climb = false; // posture: standing / crouched (16 tall) / prone (9 tall); climb = hanging on a rope
    // animation state
    float squash = 1, runPhase = 0;
    int rollT = 0, rollCd = 0, rollDir = 1;
    bool wasGround = false;
    Vector2 cape[7] = {}, capePrev[7] = {};
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

enum PickupKind { PU_SPELL, PU_WEAPON, PU_POTION, PU_HEART, PU_COIN };
struct Pickup
{
    Mob b;
    int kind = PU_SPELL;
    int spell = 0;
    Weapon weapon;
    int age = 0;
    bool alive = true;
};

enum InteractType { IT_CHEST, IT_ANVIL, IT_SHRINE, IT_PORTAL, IT_TORCH, IT_STONE, IT_SHOP, IT_ROPE, IT_CRATE, IT_BOAT };
// IT_BOAT: a longship; `used` = beached scenery, otherwise F sets sail. IT_TORCH style 1 = wall sconce.
// IT_ROPE: hangs from (x, y) down to row `data`. IT_CRATE: breakable obstacle, wood cells in [x, x+w) x [y-h, y), `data` = hits left.
struct Interact { int type; float x, y; bool used = false; int data = 0; int style = 0; int w = 0, h = 0, cells = 0, hit = 0; };
// How a stage shows off its special weapon (replaces the old sword-in-stone).
enum DisplayStyle { DS_ROCK, DS_TARGET, DS_TABLE, DS_RACK, DS_GRAVE, DS_CART, DS_ICE, DS_ANVIL, DS_ALTAR };

enum TrapType { TR_ARROW, TR_FLAME, TR_COLLAPSE };
struct Trap
{
    int type;
    int x, y, dir = 1, timer = 0;
    bool done = false;
    int rx0 = 0, ry0 = 0, rx1 = 0, ry1 = 0;
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
struct Lamp { float x, y, r; Color c; bool flame = false; };

struct FloatText { float x, y; std::string s; int life; Color col; };

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

enum GameState { GS_TITLE, GS_LOADING, GS_PLAY, GS_INVENTORY, GS_ANVIL, GS_SHRINE, GS_PAUSE, GS_DEAD, GS_WIN, GS_SHOP };
enum LoadTarget { LOAD_STAGE, LOAD_SANCTUARY, LOAD_SANDBOX, LOAD_VILLAGE };

// ================================================================ meta progression (meta.cpp)

enum UnlockKind { UK_WEAPON, UK_STAFF, UK_SPELL, UK_HOOK, UK_ARMOUR, UK_FLASK };
struct Unlock { const char* name; int shop, price, kind, a, b; const char* desc; };
extern const Unlock UNLOCKS[];
extern const int UNLOCK_COUNT;
extern const char* SHOP_NAMES[3];
struct Meta
{
    int bank = 0, runs = 0, deepest = 0;
    bool owned[64] = {}, equipped[64] = {};
};
extern Meta META;
void loadMeta();
void saveMeta();
void toggleEquip(int i);
bool buyUnlock(int i);
void bankRun();
void applyLoadout();

struct Game
{
    GameState state = GS_TITLE;
    bool sandbox = false, sanctuary = false, inVillage = false, banked = false;
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
    std::vector<FloatText> texts;
    std::vector<Ragdoll> rags;
    std::vector<Weapon> stoneLoot; // weapons held by sword-in-stone shrines (Interact::data indexes this)
    std::vector<std::pair<std::string, int>> msgs;
    float camX = 0, camY = 0, shake = 0;
    int rcx = 0, rcy = 0; // render camera (whole pixels)
    int scale = 3, vw = 0, vh = 0;
    int frame = 0, nextId = 1;
    bool portalOpen = true;
    int bossId = 0;
    int winTimer = 0, deadTimer = 0, bannerTimer = 0;
    int shrineChoice[3] = {0, 0, 0};
    SpellCard heldSpell;
    int invSel = 0;
    int nearInteract = -1;
    int brushMat = 2, brushR = 3;
    int loadTarget = LOAD_STAGE;
    int hitstop = 0;
    float uiScale = 1; // accessibility: UI size multiplier
    bool showHelp = false, reduceShake = false;
    int duneEnd = 0;  // stage 1 opens with the Whispering Dunes: x where they give way to the Greenmarch (0 = none)
    int sailT = 0;    // frames into the voyage out of Hearthwick (0 = not sailing)
    bool duneCrossed = false;
};
extern Game G;
inline bool inDunes() { return G.duneEnd > 0 && G.p.m.x < G.duneEnd && !G.sanctuary && !G.inVillage; }

// items.cpp
std::string weaponName(const Weapon& w);
float weaponDamage(const Weapon& w);
inline float spellPower() { return 1.5f * (1 + 0.2f * G.stage); } // your magic deepens as you descend
int weaponTier(const Weapon& w);
bool onPlatform(const Mob& m);
int weaponCooldown(const Weapon& w);
Weapon rollLegendary(int tier, bool fromStone, int forceType = -1);
Weapon themedWeapon(int style, int stage);
Weapon fryingPan();
const char* fxDescription(int bit);
Staff randomStaff(int tier);
Weapon randomWeapon(int tier);
int randomSpell(int tier);
bool castStaff(Staff& s, float x, float y, float ang, bool friendly);
void fireShots(const std::vector<Shot>& shots, float x, float y, float ang, float spread, bool friendly);
void recipeCost(int type, int metal, int out[RES_COUNT]);
bool canAfford(const int cost[RES_COUNT]);
void payCost(const int cost[RES_COUNT]);
float armourDef(int armour);
void castSelfTest();

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
void spawnOreBurst(float x, float y, int count);
void addPickupSpell(float x, float y, int spell);
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
int roadFloorAt(int x); // -1 when the level has no single road (castle, village...)
const char* sandboxBrushName();

// rig.cpp (animation)
void drawPlayerRig(int camX, int camY);
void drawMobAnimated(const Mob& m, int camX, int camY);
void burstSprite(const Mob& m);
void drawHeld(float ox, float oy);
void spawnRagdoll(float cx, float bottom, float h, float vx, float vy, Color head, Color body, Color limb, bool bony, CellMaterial gore);
void pushRagdolls(float x, float y, float radius, float force);
void updateRagdolls();
void drawRagdolls(int camX, int camY);
void ragdollForMob(const Mob& m);
void ragdollForPlayer();
struct Sprite;
void drawSpriteBig(const Sprite& s, float x, float bottom, bool flip, Color tint);
void drawSpriteNative(const Sprite& s, float x, float bottom, bool flip);

// levelgen.cpp
void generateStage(int s);
void generateSanctuary();
void generateSandbox();
void generateVillage();
void dumpStages(const char* dir);

// audio.cpp (everything is synthesised at startup)
enum Sfx {
    SFX_SWING, SFX_HIT, SFX_CLANG, SFX_EXPLODE, SFX_CAST, SFX_FIRE, SFX_ZAP, SFX_ICE, SFX_BOW, SFX_JUMP,
    SFX_LAND, SFX_STEP, SFX_HURT, SFX_DIE, SFX_PICKUP, SFX_ORE, SFX_CHEST, SFX_PORTAL, SFX_CLICK, SFX_CRAFT,
    SFX_ROLL, SFX_ROAR, SFX_POTION, SFX_SPLASH, SFX_HOOK, SFX_BARK, SFX_HOWL, SFX_KNOCK, SFX_SMASH, SFX_COUNT
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
void updateDrawShrine();
void updateDrawShop();
void initUI();
void uiText(const std::string& s, float x, float y, float size, Color c, int style = 0); // style: 0 body, 1 bold, 2 title
void uiTextCentered(const std::string& s, float cx, float y, float size, Color c, int style = 0);
float uiTextWidth(const std::string& s, float size, int style = 0);
void drawResIcon(int r, float x, float y, float size);
void cancelInventoryDrag();
void drawItemIcon(const Weapon& w, float x, float y, float size);
void drawSpellIcon(int spell, float x, float y, float size, bool highlight, int uses = -1);
