---
name: sand-world-art-direction
description: Art-direction rules for mixing 3D-rendered pixel-art characters (Dead Cells style) with a falling-sand / cellular-automaton world (Noita style) so they look like one game. Use this skill whenever working on Sands of Sorcery visuals, or any falling-sand game where characters, enemies, sprites, ragdolls, corpses, gibs, lighting, normal maps, palettes, terrain materials or camera zoom/scale are being added or changed. Also use it when a character "looks pasted on", when choosing sprite size or pixel scale, when baking sprites from a renderer, or before shipping any new visual asset into the sand world, even if the user doesn't mention art direction.
---

# Sand-World Art Direction

Combining 3D-rendered pixel art with a falling-sand world is a valid choice, not a weird one. Noita already pairs pixel characters with a physics-heavy sand world. Dead Cells shows 3D-to-pixel rendering reads as genuine pixel art when it's done cleanly.

The risk is never the concept; it is **inconsistency**. When the hybrid fails, it fails in one of four places, and they are almost always the first two. Check them in this order.

## The one test that matters

Before calling any visual work done, produce a screenshot of the character **standing in real terrain at real game scale and zoom**. Not on a flat background, not in an isolated sprite preview.

- If it looks pasted on, the cause is almost always **pixel scale** or **lighting**. Fix those before touching anything else.
- Show the screenshot to the user. "Looks right" is a human judgement; don't self-certify.

## 1. Pixel scale: one pixel size for everything

**Rule:** a sprite pixel and a sand cell must be the same size on screen.

If a sand cell draws as 3×3 screen pixels, character sprites draw at 3× too. Never mix a 1× sprite into a 3× world, or a world rendered at a different zoom from the sprites. Mixed pixel sizes ("mixels") are the single biggest tell that assets come from different games.

This forces a design decision about character size, so make it explicitly:

- Noita's player is about 12×19 because its world is made of 1-pixel cells. A highly detailed 80–100 px character in that world is a giant.
- **Either** scale the world's cells up to match detailed characters, **or** bring characters down to roughly 30–45 px tall. The 3D-rendered approach still works at that size, with less detail per limb.
- Record the chosen value in one constant (for example `PX_PER_CELL`) and derive sprite draw scale, camera zoom and particle size from it. Never hard-code a second scale.

Implementation requirements:

- Point filtering only (`TEXTURE_FILTER_POINT` in raylib). No mipmaps, no bilinear.
- Integer-snap every sprite draw position to the pixel grid.
- If the camera zooms by a non-integer amount, render the whole scene to a low-res target at 1 cell = 1 pixel, then upscale that target by an integer.

## 2. Lighting: bake normals, not just colours

**Problem:** sprites baked with a fixed top-left light look wrong when the real light is a burning oil pool below them or a glowing spell to the side. Falling-sand worlds are full of moving light sources (fire, lava, spells, explosions).

**Rule:** if a renderer produces the sprite, it also produces a **normal map** for every frame, and the game lights characters with those normals.

- The 2.5D primitive renderer already computes a per-pixel normal. Write it out alongside the colour frame, encoded as RGB = (n·0.5 + 0.5) and in the same atlas layout, so frame N's normals sit at the same coordinates as frame N's colours.
- Bake the colour frames with **soft, neutral** lighting (ramp shading, outlines, a small ambient term). Leave strong directional light to the runtime shader. Otherwise the baked key light and the dynamic lights fight each other.
- In-game, a simple shader: base colour × (ambient + Σ lights · max(0, n·L)). Then quantise the result back onto the material's 5-step ramp so the output stays pixel-art banded, not smooth.
- Keep the rim light baked; it is what separates characters from busy terrain.
- Ragdoll limb atlases (pre-rotated at 32 angles) need normal maps too, rotated with the geometry. This is easy when the renderer rotates the geometry before shading, and impossible with hand-drawn sprites. It is a core advantage of this pipeline; don't throw it away.

## 3. Palette harmony: same sun for world and characters

**Rule:** terrain materials use the same kind of colour ramps as characters.

- Every material (sand, stone, water, blood, wood, metal, flesh) gets a ramp that goes from dark **violet-tinted** shadows to **warm** highlights, matching the character ramps.
- Shadow and highlight hue shifts are shared values in one place (for example `SHADOW_TINT`, `HIGHLIGHT_TINT`). Characters and terrain both read them.
- Saturation: keep terrain slightly *less* saturated than characters, so characters stay readable against busy simulated ground.
- Quick check: desaturate a full screenshot. Characters should still separate from the terrain by value and outline. If they vanish, the problem is contrast, not hue.

## 4. Turn the hybrid into a feature: bodies become world

This is the reason to combine these two genres, rather than just tolerating the mix:

- Ragdolls and gibs should **become sand-sim material when they come to rest**: blood becomes liquid cells, meat chunks become a "flesh" material, and a corpse can be buried, burned, dissolved or washed away.
- Implement it as a hook, `onRagdollSleep(ragdoll)`. Rasterise the ragdoll's current limb sprites into the grid, mapping each opaque pixel to a material by its source material (the tunic becomes cloth and burns; the steel helmet becomes metal; flesh becomes flesh). Then delete the ragdoll.
- Hit effects (sparks, dust, blood) are emitted as real cells or particles that obey the simulation, not baked into sprite frames.
- Keep the sprite side and the sim side sharing the same pixel scale (rule 1), or the conversion visibly changes resolution.

## Decision checklist (run before merging visual changes)

- [ ] Screenshot in real terrain at game scale reviewed by the user.
- [ ] One pixel size for sprites, cells and particles; derived from a single constant.
- [ ] Point filtering, integer-snapped draws, integer upscale of a low-res target.
- [ ] Normal maps baked for every animation frame and every ragdoll limb angle.
- [ ] Baked colour lighting is soft; strong light comes from the runtime shader, quantised to ramps.
- [ ] Terrain ramps share the characters' shadow and highlight tints.
- [ ] Characters still read against terrain in a desaturated screenshot.
- [ ] Dead bodies and effects end up as simulation material, not decals.

## Anti-patterns

- Shipping a sprite after checking it only on a flat background.
- "Fixing" a pasted-on look by adding outlines or glow before checking scale and lighting.
- Rotating sprite textures at draw time (smears pixels and flips baked lighting). Use pre-rotated atlases.
- Smooth, unquantised dynamic lighting on pixel art; it reads as a filter, not as light.
- A second hard-coded scale factor anywhere in rendering code.
