# Sands of Sorcery

A falling-sand roguelike in the spirit of Noita, set in a medieval world. Every pixel is simulated: sand falls, water flows, oil burns, gunpowder chains into explosions and acid eats through stone.

Every run starts in **Hearthwick**, a Norse harbour village where coins banked from earlier runs buy permanent unlocks (starting weapons, a staff, starter spells, armour, a spare flask and the grappling hook). Walk out along the pier and press F at the longship to set sail with a frying pan plus whatever you've equipped. It's always night: the moon lights the open ground, and torches and lanterns light buildings and mineshafts.

There are no portals. The world is one continuous descent through seven biomes, joined by **havens**: themed safe halls with an anvil and a shrine. Walk in and the gate crashes down behind you (you're healed, and the way back is gone for good). Pray at the shrine to open the far gate onto the next biome: it offers one gift from each of three kinds - an upgrade for the weapon in your hand (a dwarven whetstone, a Norse rune carving an effect into it, or for a staff a binding that adds a slot and mana), a perk that lasts the run (Troll's Blood, Sleipnir's Stride, Bear Hide, Berserker's Fury, Hugin's Breath, Mead of the Hall; they stack), or a spell. A boss's haven stays barred until the boss falls. Behind the scenes, everything behind a sealed gate is cut away and the biome after next is built on ahead, so the world stays one piece without growing without limit.

1. **The Whispering Dunes, then the Greenmarch** - you run aground on a moonlit beach and cross quiet dunes (cacti, tumbleweeds, a giant's bones, only the wind for company; nothing hunts you here, and very rarely there's a buried chest). Then comes a mostly horizontal march across rolling plains, past farmhouses (walk-through, with upstairs rooms and cellars linked by tunnels), watchtowers and palisade gates to the moat and **Dunmoor Gatehouse**, the first haven (dire wolves, goblins, redcaps, tower archers). The caves beneath run as deep as the castle's foundations; one old mineshaft per run drops by rope into them: optional, but full of chests, dropped weapons and the skeletons of those who went before, plus a few sealed hollows you can only dig into (follow the gold seams)
2. **Castle Dunmoor** - out of the gatehouse into the open castle grounds, through the great doors into the keep's stained-glass great hall, then down through cellars and basements: tall halls joined by ramps, drop-downs and plank-capped shafts, ending at **The Crypt Gate** (castle guards, knights, cultists)
3. **Forsaken Crypts** - masonry tunnels, coffins, acid vats and arrow traps (skeletons, archers, cultists)
4. **Deepdelve Mines** - powder kegs, gunpowder crates, miasma pockets and collapsing ceilings. Boss: **The Black Knight**
5. **Frostdeep Caverns** - ice and snow (frost wraiths, draugr, kelpies, trolls)
6. **The Infernal Forge** - lava, flame vents, firestone (fire imps, golems)
7. **The Lich's Citadel** - everything at once. Boss: **The Lich King**

## Build

Needs raylib 5.5 and a C++17 compiler.

**Linux** (Fedora: `sudo dnf install raylib-devel gcc-c++`; Debian/Ubuntu: `sudo apt install libraylib-dev g++`. If your distro's raylib is older than 5.x, build it from source at github.com/raysan5/raylib.)

```
g++ -std=c++17 -O2 *.cpp -o sand -lraylib -lm
./sand
```

**Windows** (MSYS2: `pacman -S mingw-w64-ucrt-x86_64-raylib`):

```
g++ -std=c++17 -O2 *.cpp -o sand.exe -lraylib -lopengl32 -lgdi32 -lwinmm
```

Dev tools: `--selftest` checks the staff casting rules; `--dump <dir>` renders every biome to a PNG, then starts a run and walks through each haven, saving the live world after every cut (`world0.png`, `world1.png`, ...).

## Controls

| Key | Action |
| --- | --- |
| A / D | Move |
| W / Space | Jump (W also swims) |
| Hold toward a wall + W | Climb (uses stamina) |
| Space while on a wall | Wall-jump |
| Left Shift | Dodge roll (brief invulnerability, costs stamina) |
| W / S by a rope | Grab and climb (Space or A / D lets go) |
| S | Drop through plank platforms; otherwise crouch (hold S + Ctrl, or push into a low gap, to drop prone and crawl) |
| Left mouse | Attack / fire / cast |
| Hold right mouse | Grappling hook, once unlocked (W / S to reel in and out, Space to let go) |
| 1-6, mouse wheel | Switch item |
| Q | Drink a healing flask |
| G | Drop the current item |
| F | Interact (chests, anvil, shrine, the longship) |
| Tab | Inventory: drag spells between bag and staff slots, drag hotbar items to reorder or onto the bin to drop; right-click to quick equip / unequip |
| Esc | Pause (UI size with - / =, reduce screen shake with K) |
| F1 | Controls overlay |

## Systems

- **Weapons:** daggers, swords, battleaxes and crossbows in Copper, Iron, Steel, Damascus, Firestone (fire), Frostite (frost), Stormite (shock, chains between foes), Venomite (poison) and Adamantium. Melee weapons chip ore loose from the rock (harder metals can mine harder ores); use bombs or digging bolts to tunnel.
- **Staves:** Noita-style wand building. Each staff has its own mana, regen, cast delay, recharge, spread and spell slots. Spells are cast left to right: modifiers (Empower, Haste, Bounce, Homing, Piercing, Ignite, Frostbind, Explosive) apply to the next projectile, multicasts (Double/Triple Cast) fire several at once, and Trigger Bolt casts the next spell where it lands. Some spells (Bomb, Lightning, Acid Orb, Water Orb, Trigger Bolt, Explosive, Triple Cast) have limited charges: each cast spends one and the spell disappears when it runs out.
- **Crafting:** ore sits in veins in the rock. Break it with weapons, bombs or digging bolts and it falls as loose nuggets; walk over them to collect. At a haven anvil, forge weapons and armour (armour reduces damage; elemental armour resists its element) or bind a new staff from gold.
- **Weapon tiers:** I copper, II iron, III steel, IV damascus and elemental metals, V adamantium (legendaries sit one tier higher). Drops scale with depth; starting gear bought in the village is "worn" (weaker).
- **Legendary weapons:** rare, procedurally rolled weapons (type, metal, damage, 1-3 effects such as bleeding, burning, chain lightning, life steal, execution or rock-cutting) with generated names and lore, marked by a Noita-style glow (never on the first stage). Each stage also displays a special weapon in a fitting spot: a crossbow by an archery butt and a kitchen knife in a cellar on the plains, a weapon rack in the castle, a sword in a grave in the crypts, a minecart in the mines, a spear frozen in ice, a blade on the forge's anvil and an altar in the citadel.
- **Coins:** enemies and chests drop coins; they are banked when a run ends and spent in Hearthwick. Saved to `sands_save.txt`.
- **Never stuck:** every cave connects to the road (on the plains, to the cave system under the mineshaft), crates and barrels blocking doorways smash with any weapon or bolt, any weapon shovels aside rubble, and the pause menu's R returns you to the nearest road torch.
- **Hazards:** spike pits, arrow traps, flame vents, collapsing ceilings, gunpowder crates, powder kegs, acid vats, oil, lava and flammable miasma.
- **Audio:** every sound effect and the ambient music are synthesised in code at startup; there are no audio files.
- **Sandbox:** press S on the title screen. CTRL + mouse paints, `[` / `]` change material, `-` / `=` change brush size, E spawns a foe.
