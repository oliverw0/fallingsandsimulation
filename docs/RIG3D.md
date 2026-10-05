# The 3D-primitive entity model (Dead Cells style)

Read this first if you are going to touch a character, a foe or a weapon sprite. It is self-contained: you do not need the
conversation that produced it. Last updated 2026-10-05 (foes, weapons and armour looks added).

## What it is

Characters are **not drawn pixel by pixel**. Each is described as a list of 3D-ish primitives (tapered capsules, spheres, flat
polygons) hung on a 2D skeleton, rasterised with a z-buffer, then shaded into banded, crisp pixel art. Every frame of every
animation is baked offline by Python into a generated C++ header; the game only picks a baked frame and tints it. Nothing is
rotated at draw time, and there is no anti-aliasing: every pixel is a ramp colour.

Why: one renderer gives consistent lighting, rim, outline and depth lines across every entity and weapon, and a new pose is
data, not hand-painted art. The look is Dead Cells: chunky silhouette, 5-tone ramp per material, light from the upper left,
darker parts on the far side, a softened outline, bright filled smears on fast blows.

Reference material (read these for the *why*): `.claude/skills/dead-cells-rig/SKILL.md` (the original brief) and
`.claude/skills/sand-world-art-direction/SKILL.md` (pixel-size, lighting and palette rules for mixing this with the sand world).

## Where things live

| File | Role |
| --- | --- |
| `tools/viking.py` | The renderer. `Canvas` (z-buffer, normals, materials), `Draw` (local character space -> canvas), `RAMPS` (5-tone ramps), shading (`Canvas.render`), outline, depth lines. Knows nothing about any character. |
| `tools/viking_char.py` | The player: `build(d, pose)` draws the Viking; `draw_held(d, H)` draws the 8 weapons; `AXE_HEAD` outline. `pose()` is the pose dict. |
| `tools/viking_clips.py` | The player's clips: `idle`, `run`, `attack` (keyframe tables per weapon), `aim`, and body moves (roll, crawl, climb, swim...). |
| `tools/viking_emit.py` | Bakes every Viking frame -> `sprites_viking.h` (run-length coded, one shared palette). Also draws the strike smears. |
| `viking.cpp` | In game: `drawPlayerViking` picks sheet/clip/frame from the player's state and draws the baked texture, tinted. |
| `tools/wolf3d.py` | The dire wolf (quadruped, fur tufts). Emits `ANIM_WOLF` + `RIG_A_WOLF` through `tools/userart.py emit`. |
| `tools/foes3d.py` | Starter foes, part 1: the **parametrised biped** (`TYPES`: goblin, bomber, redcap, raider, risen levy; `build_biped`, `clip_biped`) and the shared ramps. |
| `tools/foes3d_beasts.py` | Starter foes, part 2: giant bat, acid slime, giant scorpion, sea serpent (`build_*`, `*_poses`). |
| `tools/foes3d_run.py` | Render/preview/emit for every foe: canvases, joints, corpse-part slots, `reduce_palette` (the sprite format holds 91 colours a sheet), `emit_any`. `FOES` lists them. |
| `tools/weapons3d.py` | Weapon icons baked from the same renderer (ground pickups, hotbar, shop, racks, a dead foe's hand) at 16 angles, chunkier than the held version (`draw_held(..., k)`) -> `sprites_weapons3d.h`. In game: `viking.cpp:weapon3dDraw`, called from `rig.cpp:drawWeaponSprite` (the old painted art is the fallback). |
| `sprites_viking.h`, `sprites_anim.h`, `sprites_weapons3d.h` | **Generated.** Never edit by hand. |

## Commands (run from the repo root)

Use the **bash** Python (`python` in Git Bash): it has numpy and Pillow. The PowerShell `python` does not.

```bash
python tools/viking_emit.py > sprites_viking.h          # the player (all weapons, all clips)
python tools/userart.py emit > sprites_anim.h           # every foe sheet + corpse parts (wolf3d / foes3d plug in here)
python tools/weapons3d.py > sprites_weapons3d.h         # weapon icons
python tools/wolf3d.py preview previews                 # quick preview PNGs of the wolf
g++ -std=c++17 -O2 *.cpp -o sand.exe -lraylib -lopengl32 -lgdi32 -lwinmm
./sand.exe --vik previews/fx        # the player in every weapon and state, in the real renderer
./sand.exe --shot previews/fx       # enemy line-ups (lineup0/1.png), combat sheet, icons
./sand.exe --selftest
```

If `sand.exe` is running the linker fails (permission denied): build to `sand_test.exe` and copy it over when the game is closed.

## Conventions

**Local space** (all characters): x forward (+ = the way he faces, always right; the game flips the picture for left), y down,
ground at y = 0. Units are "reference pixels"; `Draw(canvas, ox, facing, S)` multiplies by a scale `S` (the Viking uses 0.62 of
the 65 px reference art so he is ~20 world units tall, the 7x21 hitbox). One sprite pixel = half a world unit = one sand cell.
Do not introduce a second scale.

**Pose** = a dict: hip, lean, head angle, ankle targets (`fn`, `ff`), wrist targets (`hn`, `hf`), `held` (weapon), cosmetic
(`skirt`, `beard`, ...), `rot`/`pivot` (turn the whole figure, e.g. a roll). Limbs are solved by `two_bone` IK; knees bend forward
(sign -1), elbows the other way. Angles are degrees and *unwrapped* (a swing's angle only ever climbs or falls; never wrap it
before interpolating).

**Depth**: z grows toward the viewer. Near-side parts z > 0, far-side z < 0 and drawn with `dark=1` (one band darker). The
z-buffer does the sorting; draw order only breaks ties.

**Materials** are 5-tone ramps in `viking.RAMPS` (shadow -> highlight; shadows drift violet, lights warm). `viking.reg`-style
registration (see `wolf3d.reg`) adds more at runtime. Tags: `steel` is tag 1 (takes the weapon's metal colour in game), `gem`
is tag 2 (the gem colour), smears are tag 4 (own alpha). Armour/helmet steel is the separate untagged ramp `helm`.

**Shading** (`Canvas.render`): I = 0.18 + 0.82*max(0, n.L), L = (-0.55,-0.65,0.55); band = floor(I*4.6) + per-pixel `aux` offset
- `dark`, clamped 0..4. A specular glint on steel. Inner depth lines where a much nearer part of another object borders a pixel.
The outline is a **tinted** darkening of the edge colour, not black (`viking.OUTLINE_MODE`, `OUTLINE_KEEP`, `DEPTH_LINE`); outline and
depth-line colours are snapped to a coarse grid so a sheet needs few extra palette entries. The cyan rim is off (`viking.RIM_ON`).

**Emitted formats**
- Player: `sprites_viking.h`: `VkSheet` per weapon set (+ a body set); frames stacked, run-length (palette index + 1 or 0, run) byte
  pairs; `VK_PAL` colour (alpha is its own), `VK_TAG` per palette entry. Clip table by `VkClip` enum.
- Foes: the existing `AnimSheet` in `sprites_anim.h` (char-per-pixel rows, palette, per-frame joints in px from the anchor) and a
  `RigSpec` of corpse parts cut from the rest pose (so a dead foe falls apart into the same art). Clip names are the game's
  (`idle walk windup strike recover hurt land cast`).
- Joints (foes) are in `anim.JOINTS` order: neck, pel, headb, headt, shF, elF, haF, shN, elN, haN, hipF, knF, ftF, hipN, knN, ftN, wb, wt.

## The Viking's looks

- **Weathered**: faded tunic ramp (`tunic_w`), mud (`dirt`) worked in toward the hem, quilting worn pale, a stitched patch, a torn skirt hem,
  dented/rust-flecked helmet, grey in the beard, mud-spattered boots and wraps, a scar. All in `viking_char.build`.
- **Armour looks** (`P['look']`, `viking_char.LOOKS`): 0 gambeson, 1 mail, 2 lamellar (+ pauldrons, gorget), 3 plate. Material `armour`, palette
  tag 3, takes the worn armour metal's colour in game. `viking.cpp:armourLook` maps the metal's tier: copper/iron -> mail, steel and the
  elemental metals -> lamellar, adamantium -> plate. `viking_emit.py` bakes all four looks (`VK_LOOKS[look][set]`, 16-bit palette indices).
- **Starting weapon**: `items.cpp:starterSword()` - the weathered Norse sword (`draw_held` type `sword`: broad parallel-edged blade, nicks, pits).

## Foes

Each foe is a builder (primitives tagged with a part: head, torso, uarm, farm, thigh, shin, ...), a dict of pose parameters per clip, and
a row in `foes3d_run.py` (canvas size/anchor, corpse slots, joints). A baked foe holds its weapon *in the frame* (`RW_NONE`), so what it
swings is what you see. Clips use the game's names: idle walk windup strike recover hurt land. `./sand.exe --corpse <dir>` kills each
starter foe and screenshots the ragdoll built from the new parts; `--shot` shows the line-ups.

## Adding or changing a character

1. Add/adjust a builder (`build`-style function) and its pose functions; keep everything character-specific (sizes, ramps,
   keyframes) in data at the top of the file.
2. Preview: render a sheet with the module's `preview`, and look at it. Then rebuild the game and look at the **real** renderer
   (`--shot`, `--vik`) at game scale, standing in terrain: pasted-on looks are almost always scale or lighting.
3. Emit the header, rebuild, run `--selftest`.

## Gotchas

- Generated headers are large (sprites_anim.h ~79k lines). Regenerate, do not patch.
- Writing long scripts with shell heredocs mangles backslash escapes here; write scripts to a file with the editor tool instead.
- The player has no ragdoll limb atlas yet (his corpse is the generic ragdoll, recoloured); foes do (corpse parts are cut per slot).
- Normal maps are not baked yet (the art-direction skill asks for them; lighting is the global light-map multiply).
- Latest tweaks: head raised on a longer neck, spangenhelm (bands, nasal, cheek-plate), thigh armour on looks 1-3, run torso sway, bat wings with fingered membranes.
