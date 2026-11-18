# Proposal: Water Bodies

## Why

Milestone **M2 (A Planet)** ends with the player following a river to the sea. World
creation already computes discharge, lakes and the sea on the macro grid
(`add-climate-and-biomes`), but the terrain still shows only an opaque plane at sea level.

Concept §4.5 and §5.7 ask for:

- rivers with widths and depths from their discharge;
- lakes at the level their basins fill to;
- currents that push the carpet and floating things.

## What Changes

- **River network** (`world/water`), computed on demand from the world bundle; nothing is
  stored:
  - Every macro cell carrying at least 8 m³/s gets a channel to its receiver.
  - The channel is a Catmull-Rom curve through the cell centres, so tributaries meet it,
    with a few meanders between them.
  - Width, depth and speed follow Leopold and Maddock's hydraulic geometry
    (w = 3·Q^0.5, d = 0.3·Q^0.4, v = Q / (w·d)).
  - The water surface falls from cell to cell and never climbs. A river entering the sea
    reaches sea level at the coast.
  - A river without a tributary springs up and widens over its first half.
- **Terrain v2 shaped by water**:
  - Channels are carved with a parabolic bed at 1 m resolution.
  - Banks rise just above the water: levees over low land, but never out of the sea.
  - Hills and crags fade into river valleys.
  - Beds are gravel where the current is fast and mud where it is slow, with sandy
    banks. Lake beds are mud with sandy beaches.
  - The noise world is unchanged.
- **Lakes** at their fill level. A lake reaches into neighbouring shore cells but never
  spills into its lower outlet.
- **Water queries**:
  - At any place: kind (sea, lake, river), surface, depth and current.
  - Per chunk, on a grid.
  - The current has a parabolic profile across the channel and a pull towards the
    middle, so floating things are carried round bends.
- **Rendering**:
  - Each chunk gets a translucent water mesh, 4 m near and 16 m far, replacing M1's
    opaque sea plane.
  - Water is drawn after the opaque scene with a new water shader: drifting ripples
    animated by game time, a sun glint, and less transparency at glancing angles.
  - Rivers stay visible over the coarse, uncarved terrain of far chunks.
- **Lua and flight**:
  - `world.water(pos)` gives kind, surface, depth and current.
  - `world.find_water(pos, kind, radius)` finds the nearest river, lake or sea.
  - `render.stats().water_items` counts water draws.
  - The carpet rides on water surfaces and drifts with river currents when it skims
    them.

## Capabilities

### New Capabilities

- `water-bodies`: the river network, lakes, terrain shaped by water, water queries,
  rendering of water, and the carpet on water.

### Modified Capabilities

- `retro-rendering`: the Water requirement now covers the sea, lakes and rivers, drawn
  translucent after the opaque scene, instead of an opaque plane at sea level.

## Impact

- New:
  - `src/world/water.{h,c}`;
  - `assets/base/shaders/water.{vert,frag}`;
  - `tests/unit/world/test_water.c` and `tests/perf/perf_water.c`;
  - scenarios `water` (headless) and `water_view` (rendered).
- Changed:
  - the terrain generator (river shaping, water queries);
  - the terrain view (water meshes);
  - the renderer (water draw kind, time, the GL water pass);
  - the engine (render time from game ticks);
  - Lua `world` and the carpet.
- Platforms:
  - The water shader is GLSL 3.30 / ES 3.00 like the others.
  - River chunks take about 5 ms for stages 1–2 and 1 ms for a 4 m water mesh in
    release builds.
  - Deterministic: the water golden hash is the same natively and in WebAssembly.
- Co-op: water is a pure function of the world bundle, so every peer computes the same
  water and currents. Carpets drifting on rivers stay in step through the existing
  prediction.
