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
    {"Scroll of Firebolt", 1, 80, UK_SCROLL, SC_FIREBOLT, 0, "Start with a scroll: one great, bursting ball of fire."},
    {"Scroll of Lightning", 1, 100, UK_SCROLL, SC_LIGHTNING, 0, "Start with a scroll: a bolt that tears clean through a line of foes."},
    {"Scroll of Blood Spear", 1, 90, UK_SCROLL, SC_BLOODSPEAR, 0, "Start with a scroll: a spear of red iron that nothing stops."},
    {"Scroll of Frost Nova", 1, 70, UK_SCROLL, SC_FROSTNOVA, 0, "Start with a scroll: a ring of ice shards, all about you."},
    {"Scroll of Meteor", 1, 120, UK_SCROLL, SC_METEOR, 0, "Start with a scroll: a rock called down where you point."},
    {"Scroll of Venom", 1, 60, UK_SCROLL, SC_VENOM, 0, "Start with a scroll: a spray of acid orbs."},
    {"Grappling Hook", 2, 1200, UK_HOOK, 0, 0, "Hold right mouse to swing across chasms. Yours for good."},
    {"Copper Armour", 2, 1800, UK_ARMOUR, M_COPPER, 0, "Begin every run in copper mail."},
    {"Spare Flask", 2, 1000, UK_FLASK, 0, 0, "Begin every run with an extra healing flask."},
    {"Baldr's Offering", 2, 2200, UK_WISP, 0, 0, "A light spirit drifts at your shoulder, lighting the dark. Yours for good."},
    {"Wayfinder's Map", 2, 1500, UK_MAP, 0, 0, "A minimap in the top left that follows you everywhere. Yours for good."},
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

// The loadout is kept modest: one weapon, two scrolls.
void toggleEquip(int i)
{
    if (!META.owned[i]) return;
    bool on = !META.equipped[i];
    const Unlock& u = UNLOCKS[i];
    if (on && u.kind == UK_WEAPON)
        for (int k = 0; k < UNLOCK_COUNT; k++)
            if (UNLOCKS[k].kind == UK_WEAPON) META.equipped[k] = false;
    if (on && u.kind == UK_SCROLL)
    {
        int n = 0;
        for (int k = 0; k < UNLOCK_COUNT; k++)
            if (UNLOCKS[k].kind == UK_SCROLL && META.equipped[k]) n++;
        if (n >= 2) { message("You can only carry two starting scrolls."); return; }
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
        if (n >= (u.kind == UK_SCROLL ? 2 : 1))
        {
            message(u.kind == UK_SCROLL ? "You can only carry two starting scrolls. Sell one back first." : "You already have a weapon for the next run. Sell it back first.");
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

// Build the run's starting kit: a weathered Norse sword, the readied weapon and scrolls, and the equipped gear.
void applyLoadout()
{
    Player& P = G.p;
    P.hotbar = {starterSword()};
    P.bag.clear();
    P.scrolls.clear();
    P.scrollSel = 0;
    P.hasHook = false;
    P.hasWisp = false;
    P.hasMap = false;
    P.armour = -1;
    P.potions = 1;
    for (int i = 0; i < UNLOCK_COUNT; i++)
    {
        if (!META.stocked[i] && !(META.owned[i] && META.equipped[i])) continue;
        const Unlock& u = UNLOCKS[i];
        switch (u.kind)
        {
        case UK_WEAPON: { Weapon w; w.type = u.a; w.metal = u.b; w.dmgMul = 0.85f; P.hotbar.push_back(w); break; } // "worn" starter gear
        case UK_SCROLL: P.scrolls.push_back(u.a); break;
        case UK_HOOK: P.hasHook = true; break;
        case UK_ARMOUR: P.armour = u.a; break;
        case UK_FLASK: P.potions++; break;
        case UK_WISP: P.hasWisp = true; break;
        case UK_MAP: P.hasMap = true; break;
        }
    }
}
