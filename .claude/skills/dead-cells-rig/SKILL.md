---
name: dead-cells-rig
description: How Sands of Sorcery builds Dead Cells-style characters - a real 3D model built and posed in Blender, rendered frame by frame to a no-anti-aliasing G-buffer, then palette-shaded into crisp pixel art (banded ramps, cyan rim, outline, smears), baked into generated headers; plus limb atlases and Verlet ragdoll deaths. Use whenever building, extending or restyling the player or any character (Viking axeman flagship), its Blender model, animation clips (idle, run, attack, roll, crawl, swim, climb, grapple, crossbow), limb baking or ragdoll handoff.
---

# Dead Cells-style characters: Blender model → pixel frames

Dead Cells renders a skinned 3D model, animates it, snapshots every frame at sprite size with no anti-aliasing and toon-shades it. This project does the same: **Blender is the modeller and renderer; Python does the pixel-art shading and writes a C++ header; the game only plays baked frames.**

Keep `sand-world-art-direction` in mind alongside this skill: one pixel size for sprites and cells, normals kept per frame, ramps shared with the terrain.

> History: until 2026-10-07 characters were built from tapered capsules and flat polygons on a skeleton (`tools/viking.py` `Canvas.capsule/poly`). That read as "linked limbs": seams at every joint, each tube shaded on its own, cloth impossible. **Don't build new characters that way.** The capsule body survives only as a fallback when Blender is missing.

## Pipeline (the Viking, as built)

| Stage | File | What it does |
|---|---|---|
| Clips | `tools/viking_clips.py` | Every clip is a list of poses (hip, lean, head, hand/foot targets, weapon grip/angle, `rot`/`pivot`, `skirt`...). Pure data. |
| Joints | `tools/viking_char.py:joints(P)` | Pose → joint positions (two-bone IK for limbs). Shared by the 3D and the fallback path. |
| Export | `tools/viking3d.py:export(P)` | Joints → local space (x forward, y down, depth + toward viewer), plus derived bones (fists, toes, robe panels `skirt`/`skirtF`). `rest()` is the bind pose. |
| Model + render | `tools/viking3d_blender.py` (runs inside `blender -b --factory-startup`) | Builds the model **in code**, poses the armature per job, renders a multilayer EXR G-buffer per frame. |
| Cache | `.cache/viking3d/<scale>_<FWxFH>_<GX>_<GND>/<pose-hash>.exr` (gitignored) | Only new/changed poses re-render. **Bump `viking3d.VERSION` after any model edit.** |
| Shade | `tools/viking3d.py:body` → `tools/viking.py:Canvas.render` | G-buffer poured into the Canvas; palette banding, rim, outline, depth lines, weapons and smears happen here. |
| Emit | `python tools/viking_emit.py > sprites_viking.h` | Uses the 3D body when `blender` is on PATH. RLE frames, one shared palette, tags (1 weapon metal, 2 staff gem, 3 armour metal, 4 smear). ~1 min for 362 poses. |
| Play | `viking.cpp:drawPlayerViking` | Picks sheet/clip/frame from player state. No rendering at runtime. |

Environment: Blender 5.2 (`dnf install blender`). The tools need numpy, Pillow and OpenEXR, which are **not** in the system Python: use a venv (`python3 -m venv <dir> && <dir>/bin/pip install numpy pillow OpenEXR`) and run the tools with its python, `-I`, and `PYTHONDONTWRITEBYTECODE=1` (keeps `tools/__pycache__` out of git).

## The Blender model

Build the model from the script, not a hand-saved `.blend`, so it diffs, rebuilds headless and stays in sync with the clip skeleton. (If the user ever supplies a `.blend`, load it in the same script and keep the bone names and G-buffer contract below.)

- **Armature:** parentless bones named after the export joints (`BONES` in the script: spine, neck, head, claN/F, uarmN/F, farmN/F, handN/F, thighN/F, shinN/F, footN/F, skirt, skirtF). Each job poses every bone directly from its two joints, so no IK or constraints live in Blender. Python is the single source of animation.
- **Body:** one continuous mesh: a skin modifier over skeleton nodes, subdivided and smoothed, bound with automatic (bone-heat) weights. Shoulders flow into sleeves and knees bend inside trousers. One surface is the whole point; never go back to separate limb objects.
- **Gear:** explicit meshes on the same armature (tunic skirt with torn hem and front split, belt, buckle, belt tail, pouch, spectacle helm, hair, beard), with hand-set weights where auto-weights misbehave.
- **Garments over legs:** the skirt lives in its own collection `robe`, rendered on a second view layer, and `viking3d.read` composites it over every leg pixel whatever the depth. Only nearer arms, hands, torso and weapons cover it. Its front panel is pushed out by the leading knee and its back by the trailing one. Use the same trick for any cloak, robe or tabard that must "hug" the body and always sit on top of the legs.
- **Coordinates:** Blender X = x, Z = −y, Y = −depth; an orthographic camera looks along +Y, `ortho_scale = FW / S`, positioned so the sprite's ground row and anchor column match `GND`/`GX`.

## G-buffer contract (Blender → Python)

Render settings: Cycles, CPU, **1 sample, box filter width 0.01, no denoise, 0 bounces, transparent film**, exact sprite resolution. That is what makes the output alias-free. Passes per view layer:

| Pass | Use |
|---|---|
| Depth (Z) | z-buffer; inner depth lines |
| Normal | per-pixel normal for banding, rim and (later) runtime lighting |
| Material Index | `material * 16 + region`, via each Blender material's `pass_index` (one material per material/region pair, `mat_slot`). Material names index `MATS3`; regions say near/far limb (far regions shade a band darker, `FAR`) and give separate objects for inner lines |
| AOV `rest` | rest-pose (Generated) coordinates, so textures and armour patterns stay pinned to the body as it moves |
| Combined alpha | coverage mask |

Never let Blender do the colour. All colour comes from the palette ramps in Python, so 3D characters and 2D terrain share one look and one palette.

## Shading (Python, `viking.Canvas.render`)

- Light from the upper left and in front; intensity → one of 5 bands of the material's ramp (`viking.py:RAMPS`, shadow → highlight; shadows drift violet, highlights warm). Far-side regions get one band darker.
- Steel glints where the normal faces a fixed highlight direction. Cyan rim (110, 236, 255) on the back edge.
- Outer outline (14, 8, 22) around the silhouette. Inner depth lines where a nearer object overlaps a farther one by more than ~5 z-units.
- Explicit bounds checks for neighbour lookups (an array-roll version once leaked the bottom row's outline to the top).
- Floor clip below `GROUND + 2`, so a blade that hits the floor looks buried.
- No anti-aliasing anywhere. Every pixel is a palette colour or transparent; smear alpha is the only exception.

## Clips and attacks

- Clip data lives in `viking_clips.py`. A clip change re-renders only the poses it touched.
- Attack weapon angles must change **monotonically** through a swing (unwrapped degrees). Never wrap into [0, 360) before interpolating, or the swing spins the wrong way.
- Smears: sweep the weapon's cutting polygon (`viking_emit.held_polys`) from the previous frame's pose to this one in sub-steps, coloured by age (`SMEAR` in viking_emit.py) and filling only pixels the body doesn't cover.
- Hold frames sell weight: a peak hold before the strike, 2–3 very fast strike frames, an impact hold. Attack frames are timed to `atkLen`/`atkHitAt` in-game.
- Sparks, dust and blood are spawned in-game as particles or sand cells at frame events, never baked into frames.

## Limb atlases and ragdolls (not done for the Viking yet)

The player's corpse still uses the old generic ragdoll, recoloured. When building it:

- **Bake limbs in Blender too:** for each ragdoll part (head, torso, skirt, near/far upper arm, forearm+hand, thigh, shin+boot, weapon), hide the rest and render it at 32 angles about its parent pivot, with the light and camera fixed (rotate the geometry, not the image). Store size, parent pivot and child pivot per cell. Far parts get their own darker variant.
- Never rotate sprite textures at draw time. Pick the nearest of the 32 baked angles and snap to whole pixels.
- **Verlet ragdoll**, not Box2D. Collision is just "is this cell solid?", so it works with terrain that changes every tick. 11 particles (head, neck, pelvis, elbows, hands, knees, ankles), weapon as a separate 2-particle body. Each fixed step: integrate (damping 0.99; liquid drag ×0.85 plus buoyancy), then 8 iterations of sticks → angle limits (knee and elbow 0..150°, neck −50..60°, hip −40..130°) → grid collision (x and y resolved separately, bounce −0.2, friction 0.6; sample bones at 1/3 and 2/3 so shins don't slice 1-cell ledges). Sleep after 60 still steps; wake on terrain change or impulse.
- **Death handoff, no pop:** seed particles from the displayed frame's joints, `prev = pos − (entity vel + joint vel + hit impulse)·dt`, weapon pinned to the hand for 0.12 s then freed, and swap sprite → ragdoll in the same frame.
- Hooks: `onRagdollSleep` (turn the corpse into sand-sim material, see the art-direction skill) and `applyImpulse(point, vec)`.

## Other creatures

Foes, folk, the wolf and Indi still come from the older painters (`tools/foes3d.py`, `figures.py`, `wolf3d.py`, `dog.py`, emitted via `userart.py` into `sprites_anim.h`). New characters and restyles should follow this Blender pipeline: generalise `viking3d_blender.py` (model builder per character, same G-buffer contract) rather than writing another primitive painter.

## Rules

**Do**
- Look before you claim. After any visual change, render a clip sheet at 3–4× (and an in-game shot at real scale), inspect it, and show the user. "Looks like Dead Cells" is their call.
- Keep character-specific things as data: model build, ramps, clips. The G-buffer contract and shading code stay generic.
- Bump `VERSION` after model edits. Stale cache frames are the classic "my change did nothing" bug.
- Keep the `blender`-missing fallback working, or say clearly that you removed it.
- Run with Blender's version quirks in mind (5.x: multilayer EXR is `media_type = 'MULTI_LAYER_IMAGE'` + `file_format = 'OPEN_EXR_MULTILAYER'`; materials no longer need `use_nodes`). Pass `--python-exit-code 1` so script errors fail the bake loudly.

**Don't**
- Don't let Blender shade colour, anti-alias, denoise or filter.
- Don't model limbs as separate objects that meet at joints.
- Don't let garments lose to legs on depth. Composite them over the legs.
- Don't add libraries to the game or change its build. Blender, numpy, Pillow and OpenEXR are offline tool dependencies only; the game just compiles the generated header.
