# Proposal: Retro Renderer

## Why

Milestone **M1 (First Flight)** shows the streamed planet with the game's look: "1996,
seen through modern eyes" (concept §8.1): a low internal resolution, a curated palette
with ordered dithering, Gouraud lighting, distance fog, a gradient sky and view
distances that hardware of that era could not reach, on a visibly round planet.

## What Changes

- **Palette pass**: the scene is quantised to the 64-colour palette (32³ lookup table
  from add-art-generators) with an 8 × 8 ordered dither; text and UI are drawn after it.
  It can be switched off.
- **Sky and fog**: a gradient sky with a sun disc and glow whose horizon dips with
  altitude, and exponential-squared distance fog in the horizon colour on every scene
  shader.
- **Terrain drawing**: one mesh per loaded chunk at a level of detail chosen by distance
  (4, 8, 16, 32 or 64 m grids), textured from the material atlas at the nearest level,
  in the chunk's own tangent frame (exact curvature), with skirts against cracks
  between levels and a water surface at sea level. Meshes are (re)built within a
  per-frame budget, nearest first.
- **Floating origin in the world**: the world moves its render origin to the first
  player's chunk when that player is more than 1,024 m away from it; terrain is placed
  relative to it.
- **Lua**: `render.atmosphere`, `render.palette`, `world.to_local`, `world.from_local`,
  `world.up_at`, `world.view_distance`; render statistics report terrain draws.

## Capabilities

### New Capabilities

- `retro-rendering`: palette quantisation and dithering, sky and fog, terrain levels of
  detail, curvature, water, mesh budgets, the floating origin for rendering and the
  scripting access to all of it.

### Modified Capabilities

None (the M0 rendering requirements are unchanged: with the palette and atmosphere off,
the M0 goldens still match).

## Impact

- `src/render/` gains the retro reference functions (`retro.c`), terrain draw items and
  the sky, terrain and post passes; new shaders in the base package.
- `src/app/terrain_view.c` builds and draws terrain meshes; `src/world/world.c` owns the
  render origin.
- Platforms: the same GLSL runs as GLSL 3.30 on desktop and GLSL ES 3.00 on WebGL 2
  (`texelFetch`, flat varyings and `gl_VertexID` are available on both). The draw-call
  and triangle budgets are stated for the desktop profile; web and mobile profiles stream
  smaller rings, so they draw less. XR views reuse the per-view passes.
- Co-op: rendering is local; nothing here is replicated.
