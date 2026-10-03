// Between-run progression: coins banked from every run buy permanent unlocks in the village,
// which can then be equipped as a starting loadout.
#include "game.h"
#include "util.h"
#include <fstream>
#include <sstream>
#include <algorithm>

const Unlock UNLOCKS[] = {
    {"Copper Sword", 0, 50, UK_WEAPON, W_SWORD, M_COPPER, "A plain, honest blade."},
    {"Copper Dagger", 0, 50, UK_WEAPON, W_DAGGER, M_COPPER, "Quick little stabber."},
    {"Iron Spear", 0, 65, UK_WEAPON, W_SPEAR, M_IRON, "Long reach. Keeps trouble at arm's length."},
    {"Iron Mace", 0, 70, UK_WEAPON, W_MACE, M_IRON, "Knocks foes clean off their feet."},
    {"Hunting Crossbow", 0, 80, UK_WEAPON, W_CROSSBOW, M_COPPER, "Strike from a safe distance."},
    {"Iron Battleaxe", 0, 95, UK_WEAPON, W_AXE, M_IRON, "Slow, brutal and deeply satisfying."},
    {"Apprentice's Staff", 1, 75, UK_STAFF, 0, 0, "A four-slot staff with a Spark Bolt already inside."},
    {"Digging Bolt", 1, 50, UK_SPELL, SP_DIG, 0, "Start with a tunnelling spell."},
    {"Water Orb", 1, 55, UK_SPELL, SP_WATER, 0, "Start with a splash of water (10 charges)."},
    {"Haste", 1, 60, UK_SPELL, SP_SPEED, 0, "Start with a speed modifier."},
    {"Bomb", 1, 65, UK_SPELL, SP_BOMB, 0, "Start with three bombs."},
    {"Fireball", 1, 90, UK_SPELL, SP_FIREBALL, 0, "Start with a fireball."},
    {"Grappling Hook", 2, 100, UK_HOOK, 0, 0, "Hold right mouse to swing across chasms."},
    {"Copper Armour", 2, 120, UK_ARMOUR, M_COPPER, 0, "Begin each run in copper mail."},
    {"Spare Flask", 2, 60, UK_FLASK, 0, 0, "Begin each run with an extra healing flask."},
    {"Baldr's Offering", 2, 90, UK_WISP, 0, 0, "A light spirit drifts at your shoulder, lighting the dark."},
};
const int UNLOCK_COUNT = (int)(sizeof(UNLOCKS) / sizeof(UNLOCKS[0]));
const char* SHOP_NAMES[3] = {"Weaponsmith", "Arcanist", "Outfitter"};

Meta META;
static const char* SAVE_FILE = "sands_save.txt";

void loadMeta()
{
    META = Meta{};
    std::ifstream f(SAVE_FILE);
    std::string key;
    int v;
    while (f >> key >> v)
    {
        if (key == "bank") META.bank = v;
        else if (key == "runs") META.runs = v;
        else if (key == "deepest") META.deepest = v;
        else if (key == "owned" && v >= 0 && v < UNLOCK_COUNT) META.owned[v] = true;
        else if (key == "equip" && v >= 0 && v < UNLOCK_COUNT) META.equipped[v] = true;
        else if (key == "stock" && v >= 0 && v < UNLOCK_COUNT) META.stocked[v] = true;
    }
    // Saves from before weapons and spells were per-run: they're refunded, so every run starts with the pan
    // and whatever you buy for it. Happens once; the old `owned` lines aren't written back.
    bool migrated = false;
    for (int i = 0; i < UNLOCK_COUNT; i++)
    {
        if (!isKitKind(UNLOCKS[i].kind) || !META.owned[i]) continue;
        META.bank += UNLOCKS[i].price;
        META.owned[i] = META.equipped[i] = false;
        migrated = true;
    }
    if (migrated) saveMeta();
}

void saveMeta()
{
    std::ofstream f(SAVE_FILE);
    f << "bank " << META.bank << "\nruns " << META.runs << "\ndeepest " << META.deepest << "\n";
    for (int i = 0; i < UNLOCK_COUNT; i++)
    {
        if (META.owned[i]) f << "owned " << i << "\n";
        if (META.equipped[i]) f << "equip " << i << "\n";
        if (META.stocked[i]) f << "stock " << i << "\n";
    }
}

// The loadout is kept modest: one weapon, one staff, two spells.
void toggleEquip(int i)
{
    if (!META.owned[i]) return;
    bool on = !META.equipped[i];
    const Unlock& u = UNLOCKS[i];
    if (on && u.kind == UK_WEAPON)
        for (int k = 0; k < UNLOCK_COUNT; k++)
            if (UNLOCKS[k].kind == UK_WEAPON) META.equipped[k] = false;
    if (on && u.kind == UK_SPELL)
    {
        int n = 0;
        for (int k = 0; k < UNLOCK_COUNT; k++)
            if (UNLOCKS[k].kind == UK_SPELL && META.equipped[k]) n++;
        if (n >= 2) { message("You can only carry two starting spells."); return; }
    }
    META.equipped[i] = on;
    saveMeta();
}

bool buyUnlock(int i)
{
    const Unlock& u = UNLOCKS[i];
    if (META.owned[i] || META.stocked[i]) return false;
    if (META.bank < u.price) { message("Not enough coins - delve deeper and bring more back."); return false; }
    if (isKitKind(u.kind)) // for the next run only, within the loadout's limits
    {
        int n = 0;
        for (int k = 0; k < UNLOCK_COUNT; k++) n += META.stocked[k] && UNLOCKS[k].kind == u.kind;
        if (n >= (u.kind == UK_SPELL ? 2 : 1))
        {
            message(u.kind == UK_SPELL ? "You can only carry two starting spells. Sell one back first."
                                       : u.kind == UK_STAFF ? "You already have a staff for the next run. Sell it back first."
                                                            : "You already have a weapon for the next run. Sell it back first.");
            return false;
        }
        META.bank -= u.price;
        META.stocked[i] = true;
        saveMeta();
        return true;
    }
    META.bank -= u.price;
    META.owned[i] = true;
    META.equipped[i] = false;
    toggleEquip(i); // equip straight away where the loadout allows it
    saveMeta();
    return true;
}

void sellBack(int i)
{
    if (!META.stocked[i]) return;
    META.stocked[i] = false;
    META.bank += UNLOCKS[i].price;
    saveMeta();
}

void spendKit()
{
    for (int i = 0; i < UNLOCK_COUNT; i++) META.stocked[i] = false;
    saveMeta();
}

// Called once when a run ends (death or victory).
void bankRun()
{
    if (G.sandbox || G.banked) return;
    G.banked = true;
    META.bank += G.p.coins;
    META.runs++;
    META.deepest = std::max(META.deepest, G.stage + 1);
    saveMeta();
}

// Build the run's starting kit: a frying pan, the readied weapons and spells, and the equipped gear.
void applyLoadout()
{
    Player& P = G.p;
    P.hotbar = {fryingPan()};
    P.bag.clear();
    P.hasHook = false;
    P.hasWisp = false;
    P.armour = -1;
    P.potions = 1;
    Weapon staff;
    bool haveStaff = false;
    std::vector<int> spells;
    for (int i = 0; i < UNLOCK_COUNT; i++)
    {
        if (!META.stocked[i] && !(META.owned[i] && META.equipped[i])) continue;
        const Unlock& u = UNLOCKS[i];
        switch (u.kind)
        {
        case UK_WEAPON: { Weapon w; w.type = u.a; w.metal = u.b; w.dmgMul = 0.85f; P.hotbar.push_back(w); break; } // "worn" starter gear
        case UK_STAFF:
            staff.type = W_STAFF;
            staff.staff.name = "Apprentice's Staff";
            staff.staff.manaMax = staff.staff.mana = 110;
            staff.staff.regen = 35 / 60.0f;
            staff.staff.delay = 9; staff.staff.recharge = 28; staff.staff.spread = 3;
            staff.staff.slots = {makeCard(SP_SPARK), SpellCard{}, SpellCard{}, SpellCard{}};
            staff.staff.gem = SKYBLUE;
            haveStaff = true;
            break;
        case UK_SPELL: spells.push_back(u.a); break;
        case UK_HOOK: P.hasHook = true; break;
        case UK_ARMOUR: P.armour = u.a; break;
        case UK_FLASK: P.potions++; break;
        case UK_WISP: P.hasWisp = true; break;
        }
    }
    for (size_t k = 0; k < spells.size(); k++)
    {
        if (haveStaff && k + 1 < staff.staff.slots.size()) staff.staff.slots[k + 1] = makeCard(spells[k]);
        else P.bag.push_back(makeCard(spells[k]));
    }
    if (haveStaff) P.hotbar.push_back(staff);
}
