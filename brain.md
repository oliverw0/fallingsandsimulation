# brain.md — Sands of Sorcery

The agent's long-term memory for this project. **Read it in full at the start of every session.** Update it at the end of every session: fill unknowns, log decisions, add gotchas. Terse and accurate: wrong information here is worse than missing information. Anything still unknown is marked `TODO(audit)`; fill it from the code with a `file:function` cite, never guess.

Last full audit: 2026-10-02 (working tree on top of `b53c14a`, with large uncommitted changes).

---

## 1. What this game is

A falling-sand roguelike in the spirit of Noita, set in a medieval/Norse world. Every pixel is simulated. It is complete and playable: improve it, don't rebuild it.

- **Hub:** Hearthwick, a Norse harbour village (`levelgen.cpp:generateVillage`). Banked coins buy permanent unlocks at 3 stalls (`meta.cpp:UNLOCKS`). Sail by pressing F at the longship (`IT_BOAT`).
- **Run:** one continuous world, **no portals, no safe rooms** (since 2026-10-03). Biomes meet at **waystones** (still `game.h:Haven` / `levelgen.cpp:placeHaven`): an open clearing with a runestone, an anvil, torches and a full-heal mead horn (`PU_MEAD`). Walking past one builds the biome after next (`updateHavens` → `advanceWorld`), the soft-load point. Only a boss's waystone is barred (a Masonry plug with a portcullis) until the boss dies. Each seam's rock and back wall blend into the other biome's (`compose`). Nothing behind you is discarded.
- **Amulets** replaced perks and shrine boons: 10 Norse charms (`items.cpp:AMULETS`, `Player::amulet`, one worn). They drop from chests (1/12), kills (1/400) and bosses. Walk over one to wear it, F to swap.

| # | Stage (`STAGES[]`, levelgen.cpp) | Kind | Notes | Boss |
|---|---|---|---|---|
| 1 | Whispering Dunes → The Greenmarch → the battlefield | SK_PLAINS | W = 1800+D; dunes `G.duneEnd`; farmhouses, watchtowers, palisades; mineshaft rope into an optional cave net; the last ~600 units are the battlefield before Dunmoor (`decorateBattlefield`: BloodEarth, blood pools, painted fallen with spears, Risen Levy, storm `G.stormX0..X1`) | — |
| 2 | Castle Dunmoor | SK_CASTLE | grounds → keep → cellars; ramps, drop-downs, plank shafts; haven = The Crypt Gate | — |
| 3 | Forsaken Crypts | SK_CRYPT | masonry tunnels, coffins, acid vats, arrow traps | — |
| 4 | Deepdelve Mines | SK_MINES | kegs, gunpowder crates, miasma, collapsing ceilings | Black Knight |
| 5 | Frostdeep Caverns | SK_CAVE | ice, snow | — |
| 6 | The Infernal Forge | SK_CAVE | lava, flame vents, firestone | — |
| 7 | The Lich's Citadel | SK_CAVE | everything | Lich King |

Special weapon displays: `DisplayStyle` (game.h) → `placeDisplay` (entities.cpp).

**Systems in brief:** weapons (dagger/sword/spear/axe/mace/crossbow/pan; 9 metals `METALS[]`, tiers I–V), legendaries (`items.cpp:rollLegendary`, never stage 1), staves (`items.cpp:castStaff`/`fireShots`), crafting at haven anvils, perks (`PERKS[]`), hazards (`Trap`, `IT_CRATE`, kegs), movement (`entities.cpp:updatePlayer`), sandbox (S on the title: Ctrl+mouse paints, `[`/`]` material, `-`/`=` brush, E spawns a foe).

---

## 2. Hard constraints (never violate without explicit approval)

1. **Build is fixed:** `g++ -std=c++17 -O2 *.cpp -o sand.exe -lraylib -lopengl32 -lgdi32 -lwinmm` (C++17, raylib 5.5, MSYS2 UCRT64). Flat directory, no build system. New `.cpp` files must compile with exactly this.
2. **No new libraries or build changes** without asking. Propose options, then wait.
3. **No asset files:** no PNG/WAV/OGG/fonts, no loaders. Sprites are char arrays + palettes or maths. Shaders as string literals are OK.
   - ⚠️ **Open issue:** `sounds.h` (new, untracked, 1.9 MB) embeds *recorded* audio (raylib `ExportWaveAsCode` of downloaded "free" sound effects: sword, crossbow, hammer, pan). It's compiled-in code, not a file the game loads, but it breaks "all audio synthesised" and the README's claim. Licensing is unknown. **Ask the user** before building on it (§7).
4. **Never-stuck guarantee:** every cave connects to the road (on the plains, to the cave net under the mineshaft; `levelgen.cpp:connectPockets`). Doorway crates/barrels smash with any weapon or bolt (`entities.cpp:hitCrateAt`). Rubble shovels aside. Pause → R → nearest road torch (`entities.cpp:returnToRoad`).
5. **Save compatibility:** `sands_save.txt` is plain `key value` lines (§4). Ask before touching `meta.cpp`.
6. **Staff casting semantics are frozen**; `--selftest` (`items.cpp:castSelfTest`) guards them.
7. **Gameplay invariants:** hardness gates mining and blasts (`MetalDef::mine` vs `MaterialProps::hardness`), no legendaries on stage 1, drops scale with depth.

---

## 3. Dev tools and verification

| Tool | Use |
|---|---|
| `./sand.exe --selftest` | `castSelfTest` + `collapseSelfTest`. Run after any staff/spell/structure change; extend it when you add rules |
| `./sand.exe --dump <dir>` | `levelgen.cpp:dumpStages`. `stageN.png` per biome (1 unit/px; red = mobs, yellow = objects, orange = traps, green = player), with **gen time, full-map sim ms/frame, unreachable-open-cell count, haven reachability**. Then `world0..6.png` (live stitched world after each haven cut, ½ scale), chunk memory, and a "frame cost" line. Compare before and after |
| `./sand.exe --shot <dir>` | (main.cpp) lit screenshots: enemy line-up, combat sheet, icons, pickups, a cave, the stalls. Needs a hidden window (GL) |
| Sandbox | manual material tests; write repro steps for the user |
| Bench harness | not in repo. Pattern: a scratch `bench.cpp` doing `#define main game_main` + `#include ".../main.cpp"`, linked with every other `.cpp`, so it can time the `static` `buildLight`/`renderScene` (see §9) |

**Done means:** builds with the fixed command, `--selftest` passes, relevant `--dump` looks right, frame time measured before and after.

---

## 4. Architecture

**Files**
- `world.h/.cpp`: the grid, the cell simulation, explosions on cells, CPU world rendering.
- `cells/`: `cell.h` (Cell), `material.h` (`CellMaterial` enum + `props()` table), `reactions.h` (pair table).
- `entities.cpp` (3k lines): the game loop `updateGame`, player, mobs, AI, projectiles, particles, traps, interactables, havens, collapse bodies, `drawEntities`.
- `levelgen.cpp` (2.9k): per-stage building, stitching pieces into one world, village, sandbox, `dumpStages`.
- `items.cpp`: weapons, staves, spells, casting, legendaries, crafting costs.
- `rig.cpp`: animation, character rigs, ragdolls, Scale2x `detail2x`, sprite drawing.
- `ui.cpp`: HUD, inventory, anvil, shrine, shop.
- `audio.cpp`: synth + embedded recordings.
- `meta.cpp`: save and unlocks.
- `main.cpp`: window, view, **lighting**, `renderScene`, state machine, dev flags.
- Headers: `game.h` (all game types and globals `G`, `META`), `util.h` (xorshift RNG, hash/value noise/fbm), `sprites.h`, `sprites_hd.h`, `sounds.h`.
- `tools/*.py` generate the sprite headers (edit the .py, then re-emit).

**Cell grid** (`world.h`)
- `Cell` = 4 bytes: `material` u8, `shade` u8 (colour lerp a→b), `flags` u8 (`CF_CLOCK`, `CF_LOOSE` = ore nugget acting as powder, `CF_BURNING`), `life` u8.
- Stored in **64×64 `Chunk`s** (`CS=64`), allocated lazily. A null chunk reads as `world.fill` (plain rock). Each chunk also holds a back wall `bg[]` and a `sky[]` mask at `bgShift` resolution, plus `hash` and an `awake` counter.
- **Units vs cells:** levels are generated at 1 cell per unit, then `upscaleWorld(2)`. In play, `world.scale = 2` cells per unit. Gameplay and entities work in units (`atU/matU/isSolid/setCell`); the sim and renderer work in cells (`atq/setCellC/isSolidC`). `main.cpp:SUB` must equal `world.scale`.
- Accessors: `at()` allocates and wakes the chunk; `atq()` is the sim's own (allocates, doesn't wake); `get()`/`mat()` never allocate.
- Sizes: biomes are 1960×1200 (plains + dunes), 1800×1300 (castle) or **1100×1400 (deep biomes, since 2026-10-02)** units, ×2 in cells. The live stitched world reaches **11760×3270 cells**, with 12.3k of 37.9k chunks allocated (≈193 MB) by stage 6.

**Materials**: one table `cells/material.h:props()` with name, two colours, `Kind` (Air/Solid/Powder/Liquid/Gas/Fire), density, hardness (255 = never), flammable %, burnTime, ore→`Res`, dispersion. There are 43 materials, with a static_assert keeping the table in sync. Behaviour is **not** all table-driven (see §6 #3).

**Update loop** (`world.cpp:simulate`, called from `entities.cpp:updateGame` once per frame)
- The rect is the camera ±100 units. **Cells outside it are frozen.**
- `world.clock ^= CF_CLOCK`. Rows run bottom→top. Horizontal direction alternates **per row and per frame** (`(y+frame)&1`).
- A null or `!awake` chunk is skipped in one jump. A cell whose clock bit already matches is skipped (moved this tick). Inert solids (not burning, not loose, not reactive) are skipped.
- `updateCell` order: burning → one random-neighbour pair reaction → acid → lava ignition → movement by kind.
- After the scan, each awake chunk in the rect is FNV-hashed (clock bit masked). Changed → `awake=4` and neighbours ≥2; unchanged → `awake--`.
- Single buffer, in place. Powder and liquid fall up to `3*scale` cells per tick.
- Order of play in `updateGame`: player → mobs (update-culled to the camera ±250) → projectiles → pickups → traps → interact → havens → **simulate** → `updateStructures` (collapse) → drain `world.blasts` (≤24/frame → `explode`) → particles (drain `world.debris`) → ragdolls → camera.

**Rendering** (`main.cpp:renderScene`)
- `world.cpp:renderWorld` fills a CPU `Color` buffer at 1 texel per cell (912×512 at 1366×768) → `UpdateTexture(worldTex)`.
- Per-cell colour is `lut[mat][shade]` plus animated tweaks (`cellColor`: lava wave, liquid shimmer, gem sparkle, loose/burning flicker) and a rim light on exposed solid edges. Translucent cells are alpha-composited over `bg`. Sky shows a procedural moon and stars.
- Draws into `RenderTexture rt` → `drawEntities` under `rlScalef(SUB)` → multiplies the light map → blits to the screen at integer `G.scale` (3) with shake.
- **Lighting** (`main.cpp:buildLight`): a half-res light map built on the CPU.
  - Moonlight per column down to the first opaque cell, box-blurred.
  - Emissive glow from fire/burning/lava/acid, box-blurred twice.
  - Ray-marched shadowing `pointLight`s for torches, lamps, shrines, spells, the wisp and the player.
  - Uploaded with `UpdateTexture`, bilinear, `BLEND_MULTIPLIED`.
- **No shaders anywhere.**

**Sprites**
- `sprites.h`: `const char*` rows, `'.'` transparent, one global palette `paletteColor(ch, tint)` (`a A ; ,` = tint shades). Source: `tools/art.py`.
- `sprites_hd.h`: per-sprite palettes over `HD_ALPHABET`, half a cell per pixel. Source: `tools/art_hd.py`.
- `rig.cpp`: inline head arrays; `detail2x` (Scale2x + rim + shade) makes "Big" sprites.
- `rig.cpp:drawBig` issues **one `DrawRectangle` per sprite pixel** (see the perf section below).

**Entities vs the grid**
- AABB `Mob`s in units collide via `isSolid` (Solid or Powder cells; loose ore and Platform are excluded) and `boxSolid`. Liquids set `inLiquid`.
- Projectiles step and impact cells (`entities.cpp:~750`). Spells paint materials (`paintCircle`: Acid Orb, Water Orb), set fire (`trailFire`), or freeze water (`freezeArea`, chill weapons → Ice).
- Melee mines cells: `MetalDef::mine` vs hardness; ore → `CF_LOOSE` nuggets or debris.
- `explode` → `explodeCells` → `blastCells`: hardness vs power; ore becomes loose; solids crumble (`crumbleOf`) to gravel/snow; cells eject as `Debris` → `Particle` with `toCell` → re-enter the grid on contact (`updateParticles`).
- The sim posts `Blast`s (gunpowder r4/p3, keg r16/p6) and `Disturbance`s (burnt or dissolved solids).
- `updateStructures`: flood fill from the disturbance ring (`checkSupport`). An unanchored piece (< 3000·scale² cells, not touching bedrock) becomes a `Body` that **falls straight down a cell at a time** (no rotation). It damages mobs, then shatters (`landBody`) into loose rubble.
- **Limb rigs** (since 2026-10-03, `rig.cpp` "limb rigs"): every enemy but the slime is painted as parts by `tools/art_hd.py` (head, torso, upper arm, forearm, thigh, shin, weapon, shield, wing, tail; each a canvas with a pivot and a bone end) and emitted into `sprites_hd.h` with a `RigSpec` per creature (`RIG_GUARD`...; kinds RK_BIPED/RK_QUAD/RK_BAT/RK_GHOST). Style: Noita-like full figure turned side-on (broad torso, both shoulders and legs, three-quarter heads with two eyes, chunky blocky limbs), half a unit per pixel, humans about 24-26 units tall on unchanged hitboxes. `rigPose` lays out 18 joints (`J_*`) each frame from walk phase (`m.anim`), attack phase (`atkPhase/atkT`, `attackT`), recoil (`hitT`), flying; `drawRig` draws the parts (`DrawTexturePro`, far side tinted 0.68, flash = white silhouettes, coatings = tint). The old one-piece sprites (`mobArt`/`drawBig`) are only used for the slime, for ghost/slime bursts, and for `drawBurning`.
- Ragdolls: 9-joint Verlet, cosmetic (`rig.cpp`), **now only for the player**. Enemies leave **corpses** (`G.corpses`, `rig.cpp:spawnCorpse/updateCorpses/drawCorpses`): **the creature's rig as a Verlet ragdoll** (`Corpse` in game.h: fixed arrays, nothing allocated while it falls). Points = joints, sticks = limbs plus braces holding shoulders and hips to the torso, `G_MIN` braces stop limbs folding flat (the brief's min-distance rule; angle hinges were tried and fought the floor in 2- and 5-frame cycles). Every tunable is in `rig.cpp` `namespace Tune`. Collision push-outs and corrections move `pp` with `p` (no injected velocity); the final pass applies landing friction; `GROUND_DAMP` while anything touches (otherwise small pieces roll for ever). Sleep: still for `SETTLE_FRAMES`, or no drift over a 10-frame window (`DRIFT_WINDOW`), or `TIMEOUT` frames since last woken; `MAX_ACTIVE` simulate at once (oldest frozen first), `MAX_CORPSES` kept. `Mob::lastHit` (HitKind, set via `nextHit` before `damageMob`) decides the death: slash 1/3 severs the waist or the head, chop 1/2 a head, arm or leg, blunt flings it, a blast severs 2-4 groups; weapons drop from the hand 2/3 of the time. Severing turns off a stick group (`G_*`) and leaves `CorpseWound` jets at both stumps. Stab and bolt wounds live on a limb segment (`Mob::woundS/T/A` via `rigLocate`), drip while alive (`rigWoundPos`) and bolts stay stuck in the limb. Ghosts and the slime burst instead. Verified with scratch `riglab.cpp` (line-up alive/walk/windup/strike/cast, then deaths with PNG frames and a joint-motion dump).

**Stage generation** (`levelgen.cpp`)
- `buildStage(s, entryFloor)` lays out a biome at 1 cell per unit (noise terrain, chambers `carveChamber`, ramps `carveRamp`, tunnels `carveTunnel`, structures, ores, hazards, mobs).
- `connectPockets` flood-fills air pockets and tunnels/ramps each to the main region. This is the never-stuck mechanism.
- Material grain is baked into `shade` at gen time (`levelgen.cpp:~2400`: bricks/masonry courses, planks, jitter).
- `buildPiece` → `upscaleWorld(2)` → `takePiece`. `compose` stitches the pieces into a new `worldInit` world, then `shiftEntities`.
- `startRun` builds stages 0 and 1; each haven seal calls `advanceWorld` to append stage+2.
- **Deep biomes (stages 3–7)** use `deepLevels`/`deepFinish` instead of the old noise + left-to-right road:
  - 5 levels (`DEEP_LEVELS`) at `deepFloor[i]`, alternating direction; the last ends at the haven, bottom right. Since the next piece attaches at the haven floor, **the whole world descends diagonally**.
  - Crypt/Citadel: flat masonry corridors + halls, stairs between levels, tiled back wall with burial niches. Mines: timbered galleries, plank-capped shafts with `IT_ROPE`, boss arena on the last level. Caves: noise-wobbled worm galleries with dome chambers, curved chutes down.
  - Each level's end has a side room behind a squeeze (10 tall = prone crawlspace in crypts, 15 elsewhere).
  - Caverns: domain-warped ridge noise (`1-|2·fbm(warped)-1|`, tube tunnels) + low-freq blobs. A 5-row floor is laid back under every level afterwards, so the noise never cuts the route.
  - `offRoad()` replaces the `pathFloor` distance checks there. The per-column road (`worldRoad`) is -1 in deep biomes, so the "Lost? Press R" reminder doesn't fire there (R itself still works via torches placed along `path`).
- **Starter caves under the castle** (`castleCaves`): Greenmarch rock and caves fill the castle piece below its halls, ≥30 units from any castle open cell (prefix-sum test), so they never open into the castle. They join the plains caves through stubs at `CAVE_JOINT`=330 below the shared ground line (`plainsCaveJoint`/`openPlainsJoint` on the plains side; the stitcher aligns castle `ends.sy` with the gatehouse floor). The per-stage `--dump` check shows the castle with about 300k "unreachable" cells; that's expected (they're reached from the plains side, about 78% of them, measured).
- **Damp caves (WIP)** (`growDampCaves`, called each frame from `updateGame`): the corner below and west of the descent, sealed behind an obsidian barrier band (40–64 cells) and the pieces' bedrock rims. Zone = a cell with a piece above it and a piece east of it (`pieceRects`, kept up to date in `compose`). It's generated **lazily, 3 chunks per frame near the camera**; pre-generating it all would cost ~600 MB. It has moss 3 cells deep, `Glowmoss` patches (the only light), puddles and drips. `--dump` world images preview it via `dampPreview` without allocating. Known ceiling: unseen damp chunks read as solid to collision/light (see the `ponytail:` note); allocate around the player when an entrance is added.

**Audio** (`audio.cpp`): most SFX and the ambient music are synthesised into `Wave`s at startup (`LoadSoundFromWave`). Swings, crossbow, hammer, sword draw and pan come from `sounds.h` recordings (see §2.3).

**Save** (`meta.cpp`): `sands_save.txt` lines `bank N`, `runs N`, `deepest N`, repeated `owned i` / `equip i` / `stock i` (an index into `UNLOCKS[]`). Unknown keys are ignored. **Reordering `UNLOCKS[]` breaks old saves.**
- Since 2026-10-02, weapons, staves and spells (`isKitKind`) are bought for **the next run only** (`stocked`, sell-back for a full refund, max 1 weapon / 1 staff / 2 spells), and `spendKit()` in `travelOnward` clears them when you sail. Outfitter gear (hook, armour, flask, wisp) stays permanent (`owned`/`equip`).
- Old-save migration in `loadMeta`: every old weapon/staff/spell purchase is **refunded** (readying them kept handing out the old start). It runs once (old kit `owned` lines are never written back).

**Coatings** (`Mob::wet/oily/bloody`, frames left; set in `entities.cpp:updateStatus` from the cells you stand in, plus blood from physical hits and nearby kills):
- Wet: can't catch fire, puts burning out, and is hit by `electrify` like standing in water.
- Oily: burns longer (360 vs 180) and hotter.
- Heat dries water and burns oil off.
- Drawn by `rig.cpp:coat()` per sprite pixel (via `coatMob` around `drawBig`/`canvasEnd`), with drips as particles.
- Burning: `drawBurning` (pixel flames plus glare), a point light in `buildLight`, and a pulsing HUD chip with a fiery screen edge.

**Village** (`generateVillage`):
- Halls get chimney smoke (`Lamp::smoke`), window flower boxes, a well (west end), a runestone and a fish rack (by the pier).
- The gaps between halls belong to the stalls, so don't put back-wall props there.
- Villagers and hens (`entities.cpp:updateVillagers/drawVillagers`, sprites `SPR_MAN/WOMAN/CHILD/HEN_A/B` from `tools/art.py`, tinted per villager) wander, turn to face you and say a line.
- Stalls (layer 2, live) draw the real wares from `UNLOCKS` with `drawWeaponSprite`; an item vanishes once it's stocked or owned.
- `bgPut` now clears the sky flag: before, painted structures above the far-hill line counted as sky, which drew a hard light/shadow split across the halls.

**RNG** (`util.h`): one global xorshift `xr()` shared by gen, sim, gameplay and FX, seeded from `time()` in `main`. For a deterministic run, set `rngState()=k` before generating. It is not separately seedable per system. Sim and render use `hash2` for stable per-position noise.

**Frame-time profile** (measured 2026-10-02, 1366×768 → view 456×256 units / 912×512 cells, hidden window, -O2, seed 12345, 120 frames each):

| Where | update | renderWorld (CPU) | buildLight | full renderScene* | mobs in world |
|---|---|---|---|---|---|
| Stage 1 dunes | 0.56 | 2.72 | 0.63 | 7.7 | 88 |
| Stage 1 caves | 1.26 | 2.78 | 2.18 | 8.6 | 88 |
| Castle | 0.64 | 2.77 | 1.09 | 9.4 | 133 |
| Mines | 0.84–1.17 | 1.9–2.3 | 0.8–1.1 | 12.0–12.4 | 229 |
| Frostdeep | 1.2–1.5 | 2.1–2.6 | 0.4–1.0 | 13.6–14.1 | 279 |
| **Infernal Forge** | 1.2–1.8 | 2.1–2.7 | 0.7–1.1 | **16.2–16.9** | 335 |
| Citadel + 6 blasts | 2.9 | 2.3 | 0.7 | 16.6 | 335 |

\* `renderScene` includes `renderWorld` + `buildLight` + `drawEntities` + GPU submit.

**After change set 1 (off-screen culling, 2026-10-02):** full renderScene is 3.5–6.0 ms on every stage (the Forge went from 16.5 to about 4 ms). Update stays 0.2–2.4 ms; one Citadel sample hit 5–6 ms, which needs a look. Original finding below, kept for history.

- **The hot spot was `drawEntities` (fixed), and it grew with the whole run.** It draws *every* mob, interactable, lamp and pickup in the stitched world with no off-screen culling (update is culled to the camera; draw is not). `drawBig` is one `DrawRectangle` per sprite pixel. By the Forge, update + render is about 18 ms, **over the 16.7 ms budget at 60 fps**.
- The cell sim is cheap: 1–4.6 ms/frame even for a *whole* biome map (`--dump`), and ≤1.8 ms for the camera rect in play.

---

## 5. Noita technique reference (Petri Purho, GDC 2019, "Exploring the Tech and Design of Noita")

1. **Update order.** Scan bottom→top, alternate horizontal direction, never update a cell twice in a tick. Getting it wrong shows as liquids drifting one way and sand teleporting.
2. **Kinds.** Powder (down, then diagonals in random order); liquid (the same plus multi-cell sideways dispersion); gas (the mirror image, with a lifetime); solid; fire (spreads to flammables). Denser materials sink through lighter fluids.
3. **Data-driven materials and reactions.** One property table. Pair reactions `(A,B) → (A',B')` with a probability. A new material = a table row + at most one function.
4. **Heat and elements.** Temperature or burn state for melting, freezing and ignition. Tie in Firestone, Frostite, Stormite (conducts through water), Venomite, Ignite and Frostbind.
5. **Chunks + dirty rects.** 64×64 chunks, each with a dirty rect expanded 1–2 cells. Clean chunks cost nothing. Debug overlay toggle.
6. **Checkerboard passes.** 4 passes; a cell writes at most 32 px outside its chunk; same-pass chunks never overlap, so they're thread-safe. Do the ordering single-threaded first, then `std::thread` with before/after numbers.
7. **Pixel rigid bodies.** Marching squares → Douglas–Peucker → triangulate → body. Each frame: erase, step, rasterise back. When pixels are lost, recompute and flood-fill split. Candidates: ceilings, crates, barrels, kegs, the minecart, castle debris, ice. **Needs a dependency decision (§7).**
8. **Particles.** Blasts, splashes and impacts eject cells as free particles; they re-enter the grid on contact; hardness is respected.
9. **Look.** Colour sampled from a procedurally generated tiling texture at world (x,y), so moved material keeps a hand-made look. An emissive/glow pass for lava, fire, firestone, legendaries and spells.
10. **Generation (optional).** Herringbone Wang tiles as char arrays. Propose only; it must keep never-stuck; verify with `--dump`.

---

## 6. Technique status (audit 2026-10-02)

| # | Technique | Status | Where | Notes |
|---|---|---|---|---|
| 1 | Update order | **present** | `world.cpp:simulate`, `swapTo` | Bottom-up; direction alternates by row and frame; `CF_CLOCK` bit. `swapTo` marks the displaced cell too, so liquid pushed aside by rising gas skips a tick (harmless). Only the camera rect simulates; off-screen is frozen (by design) |
| 2 | Kinds + density | **present** | `updatePowder/Liquid/Gas/Fire`, `canSink`, `gasEnter` | Density swaps (oil 8 floats on water 10; sand/gunpowder sink); `dispersion` per material (water 5, acid/oil 3, lava 1 and sluggish); gas `life`; steam condenses; miasma persists |
| 3 | Data-driven materials/reactions | **mostly present** (2026-10-02) | `material.h:props` (+ optional trailing `tags`/`crumble`/`glow` columns), `reactions.h`, `world.cpp:pairRx` | Extinguish, acid-proof, gem, crumble and emissive glow are now table columns. Reactions use an O(1) pair lookup; a new reaction is one row. Still code: fire's own water/ice handling (`updateFire`), lava ignition, keg/gunpowder/miasma burn special-cases, `cellColor` animation. Acid resistance is a tag, not hardness |
| 4 | Heat and elements | **partial** | `CF_BURNING`+`life`, `burn`, `freezeArea` | There is burn state but no temperature. Fire and lava melt ice and snow; chill and ice spells freeze water; Ignite = `trailFire`. Firestone/Frostite/Stormite/Venomite are only ores and weapon elements: no heating, no shock conduction in water (`chainShock` is mob-to-mob), no poison on the grid |
| 5 | Chunks + dirty rects | **partial** | `Chunk::awake`, `hash`, `simulate` tail, `world.cpp:stayAwake` | 64² chunks with sleep via a per-tick content hash and a neighbour wake. Fixed 2026-10-02: chunks used to sleep under slow random reactions (acid stopped eating, sand never wetted); now acid or a reactive cell with a partner neighbour keeps its chunk awake. **No dirty rect:** an awake chunk scans all 4096 cells. Hashing costs 4096 reads per awake chunk per tick. `awake` never decays outside the camera rect (cosmetic). No debug overlay |
| 6 | Checkerboard / threads | **missing** | — | Single pass, single thread. Not worth it yet: the sim is ≤2 ms in play |
| 7 | Pixel rigid bodies | **partial** | `entities.cpp:checkSupport/stepBody/landBody`, `chestPhysics/updateChests` | Collapses are translate-only falling blocks. **Chests are real rigid boxes** (since 2026-10-02): a custom impulse solver (mass, inertia, restitution 0.32, friction 0.55, two substeps), contacts from outline points vs `isSolid`. They sleep when settled (on a face, or propped and not turning), wake when unsupported or blasted (`explode`), snap level within 0.12 rad, and drag in liquid. Their lid is a one-way platform (`platformRow`). No marching squares or triangulation; no library |
| 8 | Particles | **present** | `blastCells` → `world.debris` → `updateParticles` (`toCell`) | Ejected cells fly and land as cells; hardness gates breakage; ore flies as loose nuggets. Cap of 5000 particles. Splashes/impacts mostly spawn cosmetic particles |
| 9 | World-space patterns + glow | **partial** | `levelgen.cpp` shade baking, `cellColor`, `buildLight` | Colour is `lut[shade]`: a per-cell random byte (`put` uses `xr()`) or a gen-time pattern baked into `shade`, which **travels with the cell**. Sim-spawned material gets noise. Glow is a CPU light-map emissive term (fire/lava/acid only) plus `drawGlow` for legendaries. No shader |
| 10 | Wang-tile generation | **missing** | — | Generation is noise, chambers and connectors. Propose only |

---

## 7. Open questions for the user

- **Session priorities?** The template placeholder was never filled in.
- **`sounds.h` recorded audio:** is it acceptable under the "synthesised only" rule? Where are the recordings from, and what is their licence? If kept, the README line "every sound effect … synthesised" needs fixing.
- **Rigid bodies (#7):** use a vendored single-file 2D solver, a minimal custom solver (OBB/polygon + impulses, enough for crates, kegs and slabs), or keep the current translate-only falls? Undecided.
- **Draw culling** (see the perf section): it's the top perf win but isn't on the Noita checklist. OK to do first?
- **Locked boss haven bypass?** `--dump` sometimes reports the locked Mines haven as *reachable* (seed 4 now; seed 2 in the old baseline). With both gates shut, that implies a way in around the boss. Investigate before trusting the boss lock.
- Should the damp caves get an entrance, and from where? Currently sealed (WIP by request).
- The locked Mines waystone still shows as reachable in `--dump` (a way round the plug). The bypass wasn't fixed when havens became waystones.
- The art reference images in `reference/` are mostly missing (they were shared only in chat). Ask the user to save them; `reference/README.md` describes each.
- Citadel update sometimes 4–10 ms in the bench (others ≤2.5 ms). It's layout-dependent: the same bench went 10 ms → 0.7 ms when only the spawn spacing changed. It's **not** acid or structure checks (probe: 0 queued disturbances, 36 acid cells). Suspect the Lich (updated from 400 units out) or something near the bench's camera spot. Repro: the bench at seed 12345 with spawn spacing 60.
- `--dump` unreachable-cell counts vary a lot by seed (40 to 37k per stage). Seeded runs (2026-10-02) show the **locked Mines boss haven reported UNREACHABLE** in 2 of 3 seeds, in the baseline too. `stage3.png` suggests this is the closed boss gate (the check runs with the gate shut), not a stuck level. A big count elsewhere is likely sealed liquid pockets (`isSolid` treats liquid as open). Unverified either way; worth making the check open locked gates.

---

## 8. Decisions log

Newest first. Format: date — decision — reason — approved by.

- 2026-10-03 — **Enemy art and rigs redone** (see §4 limb rigs): parts generated in `tools/art_hd.py` (the old unused warrior/knight/dark-king HD sprites were removed; the chest stays). Restyled mid-way at the user's request from thin side-profile limbs to a Noita-style full figure turned side-on. The user's rigging brief (PNG atlas + JSON + Verlet) was followed except for the atlas/JSON: they would be asset files, so the parts and rig data are generated straight into `sprites_hd.h` — user requested.
- 2026-10-03 — **Battlefield before Dunmoor**: the plains are +600 units, with buildings capped at W-1020. `decorateBattlefield(W-960, W-350)` adds material `BloodEarth` (table row, end of enum), blood pools, `paintFallen` back-wall bodies with spears/arrows/pennons/shields, and Risen Levy (`E_RISEN`, sprite `SPR_RISEN_A/B` in tools/art.py) with skeletons. The storm: `world.storm/flash` (world.h), set by `entities.cpp:updateStorm` from the player's x between `G.stormX0..X1` (Piece/compose carry them like duneEnd). Clouds in `world.cpp` sky, moonlight ×(1-0.6·storm) in `buildLight`, rain particles out of open sky, lightning + delayed thunder — user requested.
- 2026-10-03 — **Havens → waystones, perks/shrines/boons → amulets** (see §1). Deleted: PERKS, Boon/rollShrine/grantBoon, RUNES, `updateDrawShrine`, GS_SHRINE. IT_SHRINE stays in the enum and draw code but is never placed. The seam blend in `compose` dissolves both bedrock rims within 8 units of the seam, then dithers rock (`plainRock` pairs) and back wall across ±140 units with spread fbm. Amulet art = char rows + `items.cpp:artColor` (necklace style, from the user's sheet) — user requested.
- 2026-10-03 — **Corpses** (see §4 Ragdolls) — user requested, with reference art.
- 2026-10-03 — **Grand, traversable buildings** (levelgen.cpp): farmhouses 84–156 wide, 1–3 storeys (S=26) with solid plank floors and an 18-wide Platform stairwell that swaps sides (`placeHouse`); watchtowers 48–64 wide, 2–4 storeys (S=24), stone foot, iron straps, arrow slits, hatches, overhanging deck, roofed lookout on posts (2/3) or open with dragon heads, torches, optional lean-to (`placeWatchtower(x, TW)`); Hearthwick halls are now **solid and walkable**: galleries at both ends over furnished rooms, double-height middle with `hearthCrane`, stalls in the gaps (`stalls[]`, no longer fixed at 160+140i). Interiors via `furnishRoom` (`paintRoom` shaded boards + `paintCobble` footing, dresser, feast table, picture, hung tools, shelf, rack, arrows, bunks, loose props; ground floors get `paintFireplace`) — user requested, with reference images (Norse watchtower with roofed archer deck; timber hall interior).
- 2026-10-03 — **Lanterns are live** (`IT_LANTERN`, `hangLantern` now pushes one everywhere incl. mines/havens): pendulum on a chain, mobs brushing it swing it; a blow or bolt snaps the chain, it flies spinning and smashes on contact (or a second hit / any blast) into oil cell-particles + ignited oil (`entities.cpp:smashLantern/hitLantern/updateLanterns`); light from `buildLight` follows it — user requested.
- 2026-10-03 — **Loose props** (`IT_PROP`, style 0 crate / 1 barrel / 2 box; `bodyHalf`/`isBody` in game.h): the chest rigid-body solver is now size-generic (`chestPhysics` reads `bodyHalf`). Melee, bolts (consumed via `projImpact`), blasts and walking mobs push them; they're one-way platforms like chests. They fall through Platform hatches — user requested.
- 2026-10-03 — **Props are solid, breakable; lanterns are physical** (user: "don't block me", "destroyable in a few hits"): props go into `propBoxes` (entities.cpp, rebuilt each frame in `updateChests` for props near the camera, tagged with `world.gen`) which `boxSolid` checks, so everything that walks collides, stands on, ledge-grabs them. Walking into one (`Mob::wall`) slides it kinematically (no tipping) unless a wall or another prop is in the way. `Interact::data` = hits left (crate/barrel 3, box 2); `hitProp`/`breakProp` → splinters, barrels may spill water/oil, coins off-village; blasts within r+6 break them. Lanterns: bodies shove the pendulum aside (angle set so it sits outside the body), it bounces off walls and smashes if it hits one at >1.3 units/frame; melee tests the lantern's body (+4), not its ring. Tested with scratch `swingtest.cpp` (drives `startAttack` + `SetMousePosition`).
- 2026-10-02 — **No contact damage**: enemy melee is a telegraphed attack (`entities.cpp:beginAttack/runAttack`, `Mob::atkPhase/atkT/atkDone`): wind-up (plant, lean back, glint; 10–24 frames by type) → live strike → recovery (open to counters). Kinds: AK_SWING (hitbox in front, most walkers + Black Knight melee), AK_LUNGE (wolf; kelpie aims through water), AK_POUNCE (slime), AK_DIVE (bat). Wraith/Banshee/Lich passive touch removed (they cast). Only the Black Knight's charge still uses `touchDamage`. A stagger cancels a wind-up. Test: scratch `atktest.cpp` — user requested.
- 2026-10-02 — **Cape**: `CAPE_N`=12 verlet points (game.h), segments 1.35 → calf length, flares 2.5→7 units deep (2026-10-02 retune after "spassing") with folds, lining, gold-trim hem, fur mantle + brooch; streams back with speed; nodes collide with terrain and cloth pixels never draw inside solid cells (`rig.cpp:simulateCape`/`drawPlayerRig`) — user requested.
- 2026-10-02 — **Melee is live for the whole swing**: `meleeStrike(w, impact)` runs every swing frame; each mob is struck once per swing (`Player::swingId` vs `Mob::hitSwing`); ground slam/crates/rock-cutting/lunge only on the impact frame (`atkHitAt`). **Enemy hit reactions**: `Mob::stagger` (10+dmg/3 frames, not bosses) cancels `attackT` and skips the AI switch; `Mob::hitT`/`hitDir` drive a recoil in `drawMobAnimated` (lean away, knock offset, hop, squash) — user requested.
- 2026-10-02 — **Ocean perf**: seabed sand only on gentle slopes over solid rock, and WetSand below sea level (dry sand soaks slowly → reaction keeps chunks awake; powder on cliffs avalanched for ever: sim 7.6 → 0.4 ms). `renderWorld` now draws in row bands on `std::thread`s (≤8; builds with the fixed command), one chunk lookup per 64-cell span, a sine LUT for liquid shimmer (12.8 → 2.7 ms on a full-water 1080p view). The moonlight scan stops 260 units into water. Ocean worldgen: cliff ledges, a shallow wreck by the landing and a deep one, a whale skeleton, terraces zig-zag across the abyss. `G.seaEnd` keeps dune wind/tumbleweeds off the sea; `G.underwater` silences the wind; back walls blend over 300 units at the beach seam (`startRun`). Hearthwick is 1200×560 (was 760×300), walking limited by `G.roamX0/roamX1` — user requested.
- 2026-10-02 — **The Drowned Deep** (`levelgen.cpp:buildOcean`): a 1000×1500-unit sea piece stitched onto the run's west edge in `startRun` (a 2nd `compose` with negative attachX; `aheadRect` saved/restored). Shelf → cliff → abyss floor at y≈1300; sunken-city terraces on columns; ≤3 temples whose domes trap air (water can't climb, so a door at the foot keeps the dome dry; a dais under the waterline lets you stand head-up); 5 worm caves with glowmoss grottoes and chests; kelpies (now chase vertically in water) + draugr/skeletons, built at `G.stage=2`. Moonlight now fades through 260 units of water (`main.cpp:buildLight`, `lwet`) — user requested.
- 2026-10-02 — **Breath**: `Player::breath` (0–100, ~14 s under, refills fast), head cell liquid = under; then 6+2·stage damage every 30 frames; HUD bar under stamina when <100. Perks: added `PK_LUNGS` (Ran's Lungs, 2× breath, stacks) and `PK_BREATHLESS` (never offered twice); "Hugin's Breath" renamed "Hugin's Vigour" to avoid confusion — user requested.
- 2026-10-02 — **Noita-style liquid splashes** (`entities.cpp:splashLiquid`): surface liquid cells leave the grid as `world.debris` (→ particles with `toCell`) and re-enter where they land. Hooked into `moveMob` (dive in / burst out / wading, bodies h≥10), `stepProj` (projectiles & spells entering liquid), melee swings (thrown along the swing). Blasts already ejected liquid — user requested.
- 2026-10-02 — **Fixed view scale 3** (`main.cpp:setupView`), F11 = borderless fullscreen: a bigger screen shows more world instead of zooming — user requested.
- 2026-10-02 — Chests are one-way platforms (`platformRow` checks each chest's rotated AABB top); the legacy "settle" loop in `updateInteract` that fought the rigid-body solver was removed. Coins bounce, ricochet and roll downhill (`updatePickups`). Trees and archery ranges must sit within 3–4 units of `surf` (they were landing on rooftops) — user requested.
- 2026-10-02 — Chests: new Norse art (`tools/art_hd.py:chest`, HD_CHEST/HD_CHEST_OPEN; dark planks, blue knotwork panels, brass lock) drawn as rotating textures (`rig.cpp:drawChest`), with rigid-body physics; sandbox key C drops a chest. The minimal custom solver was chosen (no dependency), resolving open question #7 for boxes — user requested.
- 2026-10-02 — Shop weapons/staves/spells per-run (save migration as above); coatings + obvious burning; village detail, NPCs, real wares on the stalls; enemy quota 0.6→0.4 with groups ≤2 spaced ≥48 apart; coin drops roughly halved (kills: 50% chance, 1-2 + stage/3; chests 4-8 + stage; boss 15×4) — user requested.
- 2026-10-02 — Deep biomes are now a downward descent in 5 levels (crypt corridors/stairs/crawlspaces, mine galleries/roped shafts, cavern worms/chutes) with Noita-style warped ridge-noise caverns; the starter caves were extended under the castle; a sealed, lazily grown WIP "damp caves" region fills the corner below/west of the descent; new material `Glowmoss` — user requested.
- 2026-10-02 — Rigid bodies (#7): keep the translate-only falling blocks for now; no dependency. Revisit with a minimal custom solver if wanted — user deferred ("do what you think is best").
- 2026-10-02 — Change set 2: material rules moved into table columns; O(1) reaction lookup; chunk stay-awake fix; `materialSelfTest` added to `--selftest` — data-driven #3 — user deferred.
- 2026-10-02 — Change set 1: off-screen culling in `entities.cpp:drawEntities` (mobs, interactables, lamps, pickups, projectiles, particles) — top perf win, not a Noita item — user deferred.
- 2026-10-02 — Created brain.md; audit only, no code changes — per the session brief "Step 1, then stop" — user.

---

## 9. Gotchas and lessons learned

- **Cape (verlet chain) stability**: damp only motion *relative to the body* (damping world velocity acts as a gale at a run); clamp that relative speed (1.0/frame); forbid links rising above level (else stops/turns whip the hem over the head); pin the anchor to the body with an eased shoulder height, not the posed shoulder (landing squash moves it 3.5 units in a frame); draw the cloth's thickness on a fixed side of each link (`(-dy, dx)·f`), never a per-link best side (it flips). Measure with scratch `capejit.cpp` (hem jerk, frames above shoulder): 0.92 → 0.39 mean jerk, 116 → 5 frames.
- Combat harnesses: `P.aim` follows the mouse every frame, so `SetMousePosition` (screen = (unit - G.rcx) * G.scale) + `PollInputEvents()` before `updateGame`, or swings point at the window's corner.
- **Powder next to liquid in a big body of water is a perf trap**: sand on any slope steeper than ~45° slides forever, and Water+Sand→WetSand keeps chunks awake via `stayAwake`. Generate seabeds as WetSand, only on gentle ground.
- A scratch harness that reads `PT[]` timers: `prof.py` wraps `updateGame` steps in a copy of entities.cpp (TT macro). Diffing an awake chunk's cells across one `simulate` call shows exactly what keeps it awake.
- When profiling in a hidden window, `UpdateTexture` timings include GPU sync noise (1–11 ms run to run); judge CPU work by `renderWorld`/`buildLight`.
- `compose` handles a piece attached to the **west** (negative attachX) fine, but it overwrites `aheadRect` with the new piece: save and restore it. The plains' x<4 bedrock column is overwritten by the ocean's last 4 columns.
- A 3-command `S=... && a & b & wait` line backgrounds the assignment too: set shell vars with `;` before backgrounded jobs.
- Changing `Player`/`Mob` layout in game.h needs **every** object rebuilt (incl. the 1-min audio.o), or the scratch build links mismatched structs.
- Ocean harness pattern: scratch `oceantest.cpp` (`#define main game_main` + include main.cpp, hidden 1920×1080 window, `startRun`, place player, loop `updateGame`).
- **g++ from Git Bash fails silently** (exit 127, no output) unless `/c/msys64/ucrt64/bin` is on `PATH` (cc1plus can't find its DLLs). Prefix with `export PATH=/c/msys64/ucrt64/bin:$PATH`.
- A full build takes about 1 minute (`sounds.h` is 1.9 MB of array literals). Build into the scratchpad (`-o <scratch>/sand.exe`) to avoid clobbering the user's `sand.exe`.
- `lighting`/`renderScene`/`setupView` are `static` in `main.cpp`. To benchmark them, `#include` main.cpp in a harness with `#define main game_main`.
- `--dump`'s "frame cost" line measures `renderWorld` only, **not** `drawEntities` or lighting, so it under-reports the real frame.
- `world.awake` counts include chunks far off-screen that never decay; don't read "chunks awake" as "chunks simulated".
- `README.md` and the original brief still say "sanctuaries/portals" in places; the code uses havens and has no portals.
- Git warns LF→CRLF on every file; harmless. Files are LF in the working tree; keep them LF when editing from scripts.
- **The RNG is shared**, so any sim change shifts every later random draw: after one changed tick, later stages generate differently. To A/B a sim change, compare *seeded* dumps (a scratch `dumpseed.cpp`: `rngState()=seed; dumpStages(dir);` linked without main.cpp) from a baseline copy against the new code; only the first stages are comparable.
- The off-screen torches and display stones stopped spawning ember particles (they spawned during draw). Particle counts dropped from about 300 to about 20; this is intended.
- **The user may have the game running:** linking `sand.exe` then fails with "cannot open output file: Permission denied". Don't kill it; ask them to close it and rebuild. Also: they only see changes after a rebuild in the repo (earlier builds went to the scratchpad, and the user kept playing a stale exe).
- Rigid-body sleep needs a geometric balance test (are the two lowest corners level?), or bodies freeze mid-tip: their spin is slow at first. Test tipping on a flat floor (a scratch `bench6.cpp`).
- **Bash tool pitfalls:** `cat > file` with no heredoc hangs waiting on stdin (it cost a 2-minute timeout); `python - <<EOF < /dev/null` throws the heredoc away. Use the Write tool for scripts, then `python file.py`.
- Python heredocs inside the Bash tool mangle `
` in C++ string literals (it became a real newline). Write patch scripts with the Write tool, then run them.
- The damp caves only exist once a deep biome sits east of a spot. At run start (plains + castle only), the area under the plains is still plain rock; to look at it in a harness, walk into the first haven first.
- Underground ambient light is 0.11, so dark materials render pure black without an emitter nearby; anything meant to be "dim" needs a `glow` source.
- `--dump`'s village.png only shows the top-left quarter since the village is upscaled (renders W×H cells). Use a scratch `bldshot.cpp` harness (include main.cpp, `snap()` via `renderScene` + `LoadImageFromTexture`) for lit building shots.
- The python-heredoc `
` gotcha also bites `printf` format strings in harness patches: write the patch with the Write tool.
- Rigid props are pushed kinematically on purpose: a small velocity push through `chestPhysics` makes tall barrels tip and drift back (friction torque). Rest snap is 0.2 rad.
- **Testing kills: trolls regenerate** (`hp += 0.04` before the death check), so `hp = 0` in a harness doesn't kill a troll.
- `spawnCorpse` needs a GL context (`LoadTextureFromImage`): it falls back to `burstSprite` when `!IsWindowReady()`.
- A corpse sprite wider than its hitbox can start inside a wall or roof: `spawnCorpse` nudges it clear ±12 units, or lays it down flat.
- Seams look hard mostly where the two biomes' rock is the same materials (only the blob layout changes), and where one piece is shorter (the fill between pieces isn't blended).
- **The heredoc `\n` gotcha has bitten 4+ times.** Never put `\n` inside C++ strings in a Bash heredoc or a `sed` replacement. Use the Write/Edit tools, or a .py written with Write.
- Harness pattern for building/battlefield shots: scratch `bldshot.cpp`/`seams.cpp`/`field.cpp` (include main.cpp, `snap()`); an art sheet via `amsheet.cpp` (links the objects, draws `drawAmulet` big).
- **Verlet corpses never sleeping** came from: rolling (no whole-body damping on contact), push-outs and joint corrections injecting velocity (fix: move `pp` with `p`), and joint-limit vs floor limit cycles. The drift-window test catches what's left. Debug by dumping each joint's per-frame motion in a harness (see `riglab.cpp`).
- `sheet.py` (scratch): crops and stacks harness PNGs into a zoomed contact sheet. Run Python without the msys PATH (its python has no Pillow).
- **Chunk sleep vs random behaviour:** anything that acts on a dice roll without changing cells this tick must call `stayAwake`, or its chunk sleeps within 4 ticks.

---

## 10. Working rules

- **Understand first.** On an unfamiliar area, read and report back before changing anything.
- **One technique per change set.** The game stays buildable and playable after each one.
- **Surgical changes.** Match the style (dense, comment-light, units vs cells explicit). Don't refactor or tidy unrelated code. Mention dead code rather than deleting it.
- **Simplicity first.** No speculative abstractions.
- **Hot loop:** no allocations per cell; sim RNG stays seedable (`rngState()`); profile before optimising.
- **Ask before:** large refactors, save-format changes, new dependencies, build changes, anything touching staff casting.
- **Report after each change set:** what changed; how it was verified (selftest, dump, sandbox steps); frame-time impact; anything unverified or risky.
- **Update this file** before ending the session.
