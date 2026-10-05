---
name: dead-cells-rig
description: Brief for building Dead Cells-style characters - lit 3D-ish primitives on a skeleton rendered to crisp pixel art, banded shading with a cyan rim light, baked animation clips, 32-angle limb atlases and Verlet ragdoll deaths. Use whenever building, extending or restyling the player or any rigged character (Viking axeman flagship), animation clips (idle, run, attack, roll, crawl, swim, climb, grapple, crossbow), limb baking or ragdoll handoff in Sands of Sorcery.
---

> Project notes (added when saved): the flagship is the Viking axeman; this repo bakes offline in Python (`tools/viking.py`) into a generated header like the other tools, instead of in-engine C++ baking. Keep `sand-world-art-direction` in mind alongside this: one pixel size for sprites and cells, normals baked per frame, ramps shared with terrain.

# Agent Brief: Dead Cells-Style Rigged Sprites + Ragdoll Deaths

Oct 5, 2026 · @Oliver Wiles

## How to use this template

Paste everything below this section into your Sonnet agent as its task brief, after filling in every `{{PLACEHOLDER}}`. Then have it work one milestone at a time and stop for your review after each.

1. Fill the placeholders in **Project context**. They are the facts the agent can't guess: repo layout, how the sand grid answers "is this cell solid?", and how sprites are currently generated.
2. Pick a renderer location: **in-engine C++** (bakes textures at startup, matching how Sands of Sorcery already generates sprites in code) or **offline Python** (PNG + JSON assets). The brief defaults to in-engine; delete the other option.
3. Start the agent with: *"Read this brief end to end. Restate the milestones in your own words and list any placeholder you can't resolve from the repo. Then do Milestone 1 only."*
4. After each milestone, check its acceptance test yourself before saying "next". The visual milestones need your eyes; the agent can't judge "looks like Dead Cells" on its own.

---

## Role, goal and scope

You are implementing a character pipeline for a 2D falling-sand roguelike: characters are **built from lit 3D-ish primitives on a skeleton, rendered to crisp pixel art**, animated from pose keyframes, and on death **become physics ragdolls** made of pre-lit limb sprites that collide with the sand grid.

The target look is Dead Cells: side-on, chunky silhouettes, banded (posterised) shading, a cool rim light on back edges, dark outlines, fast attacks with bright filled smears. Dead Cells gets this by rendering 3D models to pixels with no anti-aliasing; this pipeline does the same thing with simple shapes instead of meshes.

**In scope**

- A 2.5D primitive renderer that outputs pixel frames.
- One character (the Viking axeman) defined as data: primitives, materials, skeleton.
- Three animation clips: idle, run, attack. Plus a death handoff.
- A limb baker that pre-renders each limb at 32 rotations with fixed lighting.
- A Verlet ragdoll that collides with the game grid and draws using the baked limbs.

**Out of scope unless asked**

- AI behaviour, damage numbers, hit detection logic beyond exposing an attack's active frames.
- Additional characters. Design for them (everything character-specific is data), but build only the axeman.
- Turning corpses into terrain material. Leave a clear hook for it.

## Project context

The game is Sands of Sorcery: C++17 and raylib, a Noita-like falling-sand roguelike. Sprites and audio are generated in code rather than loaded from asset files, so this pipeline should follow that convention unless told otherwise.

| Item | Value |
| --- | --- |
| Repo root | `{{REPO_PATH}}` |
| Build system / command | `{{BUILD_CMD}}` |
| Where existing sprite generation lives | `{{SPRITEGEN_PATH}}` |
| World units | `{{1 world unit = 1 sand cell? Yes/No}}` |
| Pixels per sand cell when drawn | `{{PX_PER_CELL}}` |
| Grid query for collision | `{{e.g. bool World::isSolid(int x, int y)}}` |
| Grid query for liquids | `{{e.g. Material World::at(int x, int y)}}` |
| Gravity (cells/s²) | `{{GRAVITY}}` |
| Fixed timestep | `{{e.g. 1/60 s}}` |
| Entity / enemy base class | `{{ENTITY_TYPE}}` |
| Particle system to use for sparks and dust | `{{PARTICLE_API or "none, use sand cells"}}` |

**Renderer location (pick one, delete the other):**

- **In-engine (default):** port the renderer to C++. At startup, bake every animation frame and every limb rotation into a raylib `Texture2D` atlas. No asset files.
- **Offline:** a Python script (numpy + Pillow) writes PNG atlases plus JSON metadata into `{{ASSET_DIR}}`; the game loads them.

**Hard constraints**

- No anti-aliasing anywhere in sprite output. Every pixel is a palette colour or transparent (smear alpha is the only exception).
- Draw sprites at integer scale with point filtering (`TEXTURE_FILTER_POINT`). If the world camera rotates or zooms non-integer, render to a low-res target first.
- Character-specific numbers live in data structs, not in renderer code.

## Architecture

All rendering happens once, at startup. At runtime the game only plays baked frames and, after death, draws baked limbs at the ragdoll's angles.

&#91;embedded content: pipeline · bake once at startup, then animate and ragdoll at runtime\]

Modules to create, one file pair each: `PrimitiveRenderer`, `CharacterDef` (plus `VikingAxeman`), `Pose`/`IK`, `ClipBaker`, `LimbBaker`, `Animator`, `Ragdoll`, `DeathHandoff`. Only `Ragdoll` and `Animator` touch the game world, and only through the grid queries and frame-event hook in **Project context**.

## Renderer spec

The renderer rasterises a list of primitives into per-pixel buffers, then shades every pixel from a 5-step colour ramp. It never blends colours, so output stays pixel-art clean.

**Buffers (one per frame, frame-sized):** depth `z` (init −∞), normal `n` (xyz), material index, shade offset `aux` (int), object id. A primitive writes a pixel only where it is inside the shape *and* its z is greater than the stored z.

**Coordinates:** screen x right, y down. z points toward the viewer. The character faces +x, so his **right** side is near the camera (z > 0) and his **left** arm and leg are far (z < 0).

**Primitives**

*Tapered capsule* `(A, B, rA, rB, zA, zB, material, texture?, caps=true, clip?, dark=0)`. Spheres are capsules with A = B. For each pixel centre p:

```latex
t = \mathrm{clamp}\!\left(\frac{(p-A)\cdot(B-A)}{|B-A|^2},0,1\right),\quad r = r_A + (r_B-r_A)\,t,\quad d = |p - (A + t(B-A))|
```

Inside when d ≤ r. With `caps=false`, also require the unclamped t to be in \[0,1\]; this gives skirts and sleeves flat ends. Normal = (dx/r, dy/r, √(1−(d/r)²)). Depth = lerp(zA, zB, t) + nz·r, so every capsule bulges toward the camera.

*Flat polygon* `(points, z, material, normal, auxFn?)` for axe blades and other slabs. Constant normal; `auxFn` adds edge highlights.

*Texture callbacks* take (t, normal, px, py) and return an `aux` offset and optionally a material override per pixel. Used for belts, hems, wood grain, fur noise and leg wraps.

**Shading (per pixel)**

- Light direction L = normalise(−0.55, −0.65, 0.55): from upper left, in front.
- I = 0.18 + 0.82·max(0, n·L). Band = clamp(floor(I·4.6) + aux, 0, 4). Colour = ramp\[material\]\[band\].
- Steel only: if n·normalise(−0.3, −0.45, 0.84) > 0.975, colour = near-white (a specular glint).
- Rim light: where nx > 0.62 and nz < 0.55, colour = 35% base + 65% rim colour (110, 236, 255).
- Far-side parts pass `dark=1`, pushing them one band darker. This is what makes near and far limbs read separately.

**Lines**

- Outer outline: any transparent pixel 4-adjacent to an opaque one becomes (14, 8, 22).
- Inner depth lines: an opaque pixel whose 4-neighbour belongs to a different object and is more than 5 z-units in front gets darkened 55% toward the outline colour. This separates an arm from the torso it overlaps.
- Use explicit bounds checks for neighbours. Do not wrap around frame edges (an earlier prototype used array roll and leaked an outline from the bottom row to the top).

**Floor clip:** after shading, clear every row below `GROUND + 2`. A blade that hits the floor looks buried, which sells impacts.

## Character and skeleton

A character is a `CharacterDef` (materials, bone lengths, a build function from pose to primitives) plus a `Pose` per frame. The renderer knows nothing about Vikings.

**Pose struct**

- `hip` (x, y), `lean` (torso angle, degrees, y-down; −90 is upright), `head` angle.
- `feet.near`, `feet.far`: ankle targets. `hands.near`, `hands.far`: wrist targets.
- `axe`: grip point, angle (direction grip→head), z at grip, z at head.
- Cosmetic: `skirt` swing (degrees), `beard` sway (px), `eyeFlare` (bool).

**Derived joints:** chest C = hip + 23·dir(lean); neck N = C + 3·dir(lean); head centre = N + 8·dir(head) + (1, 0). Shoulders sit at C + (0.5, 2.5) near and C + (−2.5, 2.5) far.

**Two-bone IK** for every arm and leg: given root R, target T, lengths l1 and l2, clamp |T − R| into (|l1 − l2|, l1 + l2), then the middle joint = R + l1·dir(angle(T − R) + sign·acos((l1² + d² − l2²) / (2·l1·d))). Knees use sign −1 (bend forward), elbows +1. Expose the sign per limb per pose; overhead poses sometimes need it flipped.

**Build order and depth (far to near):** far leg (z −6, dark) → far arm (z −9, dark) → axe → skirt (z −4) → torso (z 0) → fur mantle (z 2.5) → neck, face, beard, moustache → helmet (clipped below the brim) → nasal guard → near leg (z 6) → near arm (z 10). Z-buffering does the real sorting; this order just keeps ties stable.

**Two-handed grip:** far hand on the grip point, near hand 9 px up the haft. Haft z sits between the two hands' z so the near fist wraps over it and the far fist sits behind.

**The Viking axeman (design brief):** yellow tunic with belt and steel buckle, skirt to the knee with a dark hem, fur mantle across the shoulders, steel nasal helmet with a darker brim band and a rivet, big ginger beard with one braid, dark purple gloves with bracer cuffs, dark trousers, pale linen leg wraps, chunky boots, glowing ember eye under the brim. A 48-unit two-handed bearded axe with a back spike, leather grip binding and steel pommel. Exact sizes are in **Reference values**.

## Animation clips

Each clip is a list of `Pose` values plus per-frame durations. Idle and run poses come from functions of phase; the attack is a hand-authored keyframe table. Store the source poses with the baked frames: the ragdoll needs them at death.

| Clip | Frames | Loop | Timing (ms) | Notes |
| --- | --- | --- | --- | --- |
| idle | 8 | yes | 150 each | Breathing: hip +0.5 px, lean ±1.2°, head lags, axe on shoulder rises and falls, beard sways |
| run | 8 | yes | 70 each | Lean −74°, stride ±15 px, foot lift 11 px on the forward swing, hip bob 2.5 px at 2× frequency, far arm pumps, skirt kicks back, dust on the two contact frames |
| attack | 13 | no | 80, 70, 60, **150**, 35, 30, 30, **120**, 70, 70, 70, 80, 90 | Overhead two-handed chop; the bold frames are the peak hold and the impact hold |

**Attack beats:** ready → gather → wind-up → **peak hold** (eyes flare) → strike ×3 (very fast, smeared) → **impact** (blade buried, sparks, dust, 1 px shake) → impact hold → settle → pull out → recover → ready. Report frames 4–7 as the hitbox-active window.

**Axe angle must increase monotonically through the swing** (202° → 266° → 322° → 372° → 404°, unwrapped). The blade's cutting edge is on the +90° side of the haft, so it leads a clockwise swing. Never wrap angles to \[0, 360) before interpolating.

**Smears:** for each strike frame, sweep the blade polygon from the previous frame's axe pose to the current one in 14 sub-steps, recording for each pixel how recent its sub-step was (age 0 = now, 1 = previous frame). Fill only pixels not covered by the character:

- age < 0.18 → (255, 252, 236); < 0.38 → (255, 232, 160); < 0.62 → (255, 170, 60); else (226, 92, 40).
- Alpha = 255·(1 − 0.5·age). Above age 0.6, checkerboard-dither (skip pixels where x + y is odd).

**Sparks and dust:** in the prototype these were baked into frames. In-engine, **emit them as real particles or sand cells** at the impact point and foot contacts instead, so they interact with the world. Expose `onFrameEvent(clip, frame)` for this.

## Limb baking for ragdolls

Rotating finished sprites at runtime smears pixels and flips the baked lighting (an upside-down forearm would be lit from below). Instead, **re-render each limb at 32 angles with the light held fixed**, then pick the nearest angle at runtime.

**Ragdoll parts (12):** head (helmet, face and beard as one part), torso (with belt and mantle), skirt, near and far upper arm, near and far forearm-with-hand, near and far thigh, near and far shin-with-boot, axe.

**For each part:**

1. Define it in its own local frame: a parent pivot at the origin and a child pivot along +x at the bone length. The axe's pivot is its grip.
2. For k = 0…31, rotate the part's primitive *geometry* by k·11.25° about the parent pivot, then render it with the normal renderer. Light direction, rim and outline all stay world-fixed.
3. Pack the results into one atlas. For each cell store the size, the pixel position of the parent pivot, and the child pivot.
4. Far-side parts are baked with `dark=1`. Bake near and far as separate variants; do not reuse near sprites for the far side.

**Runtime draw:** angle index = round(bodyAngle / 11.25°) mod 32, then draw the cell so its stored pivot lands on the body's pivot position (integer-snapped). Draw order: far parts, then torso, skirt and head, then near parts. The axe goes wherever its last z put it.

**Inner lines are lost** between separately drawn parts, which is acceptable for corpses. Each baked limb keeps its own outer outline, and that is enough separation.

## Ragdoll physics

Use a hand-rolled Verlet ragdoll, not Box2D. Its collision is just "is this cell solid?", so it works with a terrain that changes every tick, and corpses can sink in liquid or be buried by sand.

**Particles (11):** head, neck, pelvis, near and far elbow, near and far hand, near and far knee, near and far ankle. Shoulders are offsets from the neck along the torso's perpendicular, recomputed each step, so they aren't particles. The axe is its own two-particle body (grip, head).

**Per particle:** position, previous position, inverse mass, collision radius in cells. Pelvis and neck are heaviest (inverse mass 0.5); hands and ankles lightest (1.5).

**Each fixed step:**

1. Integrate: `v = (pos − prev) · damping (0.99)`; `prev = pos`; `pos += v + gravity · dt²`. Apply liquid drag (×0.85) and buoyancy for particles in liquid cells.
2. Repeat 8 times: satisfy stick lengths (mass-weighted), then angle limits, then grid collision.
3. Run 2 substeps per frame if fast-moving bodies tunnel through 1-cell walls.

**Sticks:** neck–head, neck–pelvis, shoulder–elbow ×2, elbow–hand ×2, pelvis–knee ×2, knee–ankle ×2, axe grip–head. Use the bone lengths from the skeleton so the ragdoll matches the sprite.

**Angle limits** (child bone relative to parent bone, degrees, positive = forward bend):

| Joint | Min | Max |
| --- | --- | --- |
| Knee | 0 | 150 |
| Elbow | 0 | 150 |
| Neck (head vs torso) | −50 | 60 |
| Hip (thigh vs torso) | −40 | 130 |
| Shoulder | free | free |

When a joint goes past a limit, rotate the child end toward the limit and move the parent end a quarter as far the other way.

**Grid collision:**

- Particles: resolve x and y separately. Move by vx, and if the particle overlaps a solid cell, revert x and scale vx by −0.2. Then the same for y. On contact, apply friction 0.6 to the tangential velocity.
- Bones: also sample each stick at its 1/3 and 2/3 points. If a sample is inside a solid cell, push both endpoints out along the shallowest axis. This stops shins slicing through 1-cell ledges.

**Sleep:** if every particle moves less than 0.05 cells per step for 60 steps, freeze the ragdoll. Wake it if any cell under its bounding box (expanded by 2 cells) changes, or if it takes an impulse.

**Death handoff (no pop):**

1. On death, take the pose of the currently displayed animation frame and run the same joint maths as `build()`. That gives every particle's position.
2. Set each `prev = pos − (entityVelocity + jointVelocity + hitImpulse) · dt`. `jointVelocity` = this frame's joint position minus the previous frame's, divided by that frame's duration. `hitImpulse` is strongest on the particle nearest the hit point.
3. The axe detaches. Pin its grip to the near hand for 0.12 s, then release it as a free body.
4. In the same frame, hide the animated sprite and show the ragdoll.

**Facing left:** mirror particle x positions on spawn, and draw the limb atlas flipped horizontally with angles mirrored (θ → 180° − θ). Flipping the lighting too is fine; Dead Cells does it.

**Hooks to leave:** `onRagdollSleep(ragdoll)` (for converting corpses into terrain later) and `applyImpulse(point, vec)` (for explosions).

## Milestones

Do these in order and stop after each one for review. Each milestone ends with a debug screen or a written-out image the reviewer can look at.

1. **Renderer core.** Capsule, sphere and polygon primitives, z-buffer, ramp shading, rim, outline, inner depth lines. *Check:* a test scene of one sphere, one tapered capsule crossing in front of it, and one flat polygon, drawn at 4× scale. Shading shows 5 distinct bands, a cyan rim on the right edges, a dark outline, and a dark line where the capsule crosses the sphere.
2. **Skeleton, IK and the axeman's static pose.** *Check:* the idle pose matches the description in **Character and skeleton**: near limbs brighter than far limbs, axe resting on the near shoulder, blade clear of the helmet.
3. **Clips and frame baking.** Idle, run and attack baked to an atlas, played by an `Animator` with per-frame durations and `onFrameEvent`. *Check:* a debug screen loops all three clips side by side at 3×. The swing smear joins up with the blade, and nothing clips at the frame edges.
4. **Limb atlas.** 12 parts × 32 angles, with pivots. *Check:* a debug grid of every part at every angle. Lighting comes from the top-left in every cell, including upside-down ones, and pivot markers sit on the joints.
5. **Ragdoll on flat ground.** Verlet, sticks, angle limits, simple floor. *Check:* dropped from the idle pose, the body falls, knees never hyperextend, and it comes to rest and sleeps within 3 s.
6. **Grid integration.** Collision against `{{GRID_QUERY}}`, liquids, wake-on-change. *Check:* a corpse tumbles down a staircase without passing through 1-cell ledges, floats or sinks in liquid, and wakes and falls when the sand under it is dug out.
7. **Death handoff.** *Check:* killing the enemy on each attack frame and mid-run produces no visible pop on the death frame. A hit from the right throws the body left, and the axe flies off separately.
8. **Cleanup.** Constants moved into the character data, a short README section, no leftover debug drawing in release builds.

## Rules for the agent

**Do**

- Look before you claim. After any visual change, write a PNG at 3–4× scale (or take a screenshot of the debug screen) and inspect it before reporting the milestone done.
- Keep everything character-specific (ramps, bone lengths, primitive list, keyframes) in data. A second character should need no renderer changes.
- Bake at startup and cache the results. Rendering a frame is far too slow to do per tick.
- Keep the physics fixed-timestep and deterministic. Same seed and inputs should give the same corpse.
- When a number in this brief looks wrong in practice, change it and say what you changed and why.

**Don't**

- Don't use texture filtering, mipmaps or sub-pixel sprite positions. Snap sprite draws to integer pixels.
- Don't rotate sprite textures at draw time. Use the baked angle atlas.
- Don't wrap the attack's axe angle into \[0, 360) before interpolating; the swing will spin the wrong way.
- Don't let neighbour lookups wrap around buffer edges.
- Don't pull in Box2D or another physics engine without asking.
- Don't start the next milestone until the reviewer says so.

**When you're stuck:** report what you tried, show the image, and ask one specific question. Don't silently simplify the spec.

## Reference values

These come from the working Python prototype. Treat them as a starting point that is known to look right, not as sacred. Units are pixels in a frame where the ground is at y = 138 and the hip rests 28 px above it.

**Bone lengths:** torso 23, thigh 15, shin 14, upper arm 12, forearm 11, axe haft 48 (plus 4 below the grip).

**Colour ramps (shadow → highlight, RGB)**

| Material | 0 | 1 | 2 | 3 | 4 |
| --- | --- | --- | --- | --- | --- |
| tunic | 46,24,56 | 128,66,36 | 206,132,38 | 246,190,70 | 255,236,160 |
| tunic\_trim | 30,16,40 | 84,40,36 | 140,74,34 | 180,104,40 | 214,140,60 |
| steel | 22,22,40 | 56,60,84 | 102,108,132 | 152,160,180 | 210,218,232 |
| belt | 26,14,24 | 66,34,30 | 108,58,38 | 148,88,54 | 188,128,78 |
| pants | 12,8,22 | 30,22,46 | 50,38,70 | 74,58,94 | 104,88,122 |
| wrap | 34,24,36 | 86,70,70 | 140,122,108 | 186,170,148 | 222,210,186 |
| boot | 10,6,14 | 28,18,24 | 50,32,32 | 76,50,44 | 108,76,60 |
| skin | 60,28,40 | 142,78,62 | 212,140,106 | 240,180,140 | 255,222,190 |
| beard | 44,20,22 | 120,54,24 | 186,98,36 | 226,148,60 | 252,200,110 |
| fur | 18,12,18 | 44,30,30 | 80,56,46 | 118,88,68 | 158,126,98 |
| wood | 28,14,16 | 70,36,28 | 112,62,40 | 150,92,56 | 186,126,78 |
| glove | 14,8,20 | 38,24,46 | 62,42,72 | 92,66,102 | 124,96,132 |

Rim (110, 236, 255). Outline (14, 8, 22). Eye glow (255, 150, 40) with a (255, 236, 170) core.

**Primitive sizes (radius start → end):** torso 8.4 → 10.2; skirt 9 → 12, 15.5 long, no caps; thigh 5.2 → 4.2; shin 4.2 → 3.4; boot 3.8 → 2.6, toe 6.5 ahead of the ankle; upper arm 4.6 → 3.8; forearm 3.8 → 3.2; hand sphere 3.7; helmet sphere 8.8, clipped 3.2 below head centre; face sphere 6.4; beard 5.6 → 2.2, 12 long; mantle sphere 8.6.

**Attack keyframes:** axe angle in degrees, grip offset from the chest, lean in degrees, crouch in px, feet x relative to hip.

| # | Beat | Axe ° | Grip (x, y) | Lean | Crouch | Feet (near, far) |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | ready (one hand) | −137 | 8, 12 | −84 | 0 | 7, −8 |
| 1 | gather | 192 | −6, −6 | −98 | 2 | 8, −9 |
| 2 | wind-up | 208 | −5, −17 | −106 | 3 | 10, −10 |
| 3 | peak hold | 202 | −4, −19 | −109 | 3 | 10, −10 |
| 4 | strike | 266 | 2, −21 | −96 | 1 | 12, −10 |
| 5 | strike | 322 | 10, −12 | −80 | 1 | 14, −10 |
| 6 | strike | 372 | 14, −1 | −68 | 3 | 15, −11 |
| 7 | impact | 404 | 14, 4 | −60 | 5 | 15, −12 |
| 8 | impact hold | 404 | 14, 4 | −61 | 5 | 15, −12 |
| 9 | settle | 402 | 13, 4 | −63 | 4 | 15, −12 |
| 10 | pull out | 380 | 11, 3 | −72 | 3 | 13, −11 |
| 11 | recover | 300 | 6, −6 | −80 | 2 | 10, −9 |
| 12 | ready (one hand) | −137 | 8, 12 | −84 | 1 | 7, −8 |

Frames 0 and 12 hold the axe one-handed on the shoulder (haft z 13 at the grip → −6 at the head); all others are two-handed with the haft at z 12.
