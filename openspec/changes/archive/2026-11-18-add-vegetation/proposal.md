# Proposal: Vegetation

## Why

M2's exit demo flies from a coastal desert over a rain-shadowed range into a temperate
rainforest. That only reads if plants follow the climate. Concept §4.6 places plant
instances at chunk stage 3 by species suitability. §8.2 draws forests at four levels
of detail so they stay cheap from the ground to the horizon.

## What Changes

- **Species registry**: `data/flora.lua` describes 19 base species.
  - Layers: trees, shrubs and ground cover.
  - Each species has an atlas frame and, for trees, a far-forest clump impostor.
  - It has a height range, a width, and a climate niche: annual temperature and rain as
    lowest, best and highest values, plus a frost limit.
  - It lists the ground it grows on and the steepest slope it takes. Some species are
    wetland-only (reeds, date palms at oases).
  - Each species has one foliage colour per season.
  - Broken registries are refused with a message naming the species and field.
- **Plant art**: a generated pixel-art atlas, `sprites/flora.png` (`tools/gen/gen_flora.py`),
  built from small procedural plant models in palette colours only.
  - Plants: oak, beech, spruce, fir, pine, palm, date palm, birch, acacia, cypress,
    kapok, dead tree, shrubs, a column cactus, grass, fern, flowers, reeds.
  - Four clump impostors: broadleaf, conifer, dry and tropical.
- **Placement** (chunk stage 3):
  - Jittered grids per layer: trees about every 6 m, shrubs every 4 m, ground cover
    every 2.5 m.
  - A grid point keeps a plant with a probability from the biome's cover of that layer
    and how well the species suit the place's climate and ground.
  - The species is drawn by suitability, and the size and mirroring are random per
    plant.
  - Nothing grows under water. Wet ground (wetlands, river banks, lake shores, oases)
    admits wetland species.
  - Climate: the created world's monthly climate, or for the noise world a climate by
    latitude with rainy and dry regions, classified into biomes the same way.
  - Results are counter-hashed, so they are deterministic everywhere.
- **Stage 0 canopy**: tree cover every 32 m and the chunk's dominant tree species,
  within the 2 KB stage-0 footprint.
- **Streaming**: when the world has plants, chunks within an ecology ring get stage 3.
  The ring is 768 m on the desktop, 512 m on the web and 320 m on mobile.
- **Rendering**, with four levels as in concept §8.2:
  - Near (under 300 m): every plant as an alpha-tested billboard.
  - Mid (under 750 m): trees and shrubs.
  - Far (under 4 km): one clump impostor per 32 m of canopy.
  - Horizon: the canopy surface. Coarse terrain meshes are raised by up to 14 m and
    tinted canopy green.

  Billboards are static per-chunk instance buffers drawn with a foliage shader. They turn
  to the camera about the chunk's up axis and sway in the wind with game time. They are
  coloured per species by the season, blended between seasons
  (`zb_terrain_view_set_season`).
- **Lua**: `world.flora()`, `world.plants(pos, radius)` and
  `render.stats().foliage_items` / `foliage_instances`.

## Capabilities

### New Capabilities

- `vegetation`: the species registry, plant placement, the canopy, the ecology ring,
  the four levels of forest, seasonal colours, and plants from Lua.

### Modified Capabilities

None. Stage 3 and the canopy add to the terrain stages without changing the
requirements of stages 0–2. The ecology ring is off unless the world has plants, so
chunk streaming's rings are unchanged.

## Impact

- New:
  - `src/world/flora.{h,c}`;
  - `assets/base/data/flora.lua`, `assets/base/sprites/flora.{png,lua}`;
  - `assets/base/shaders/foliage.{vert,frag}`;
  - `tools/gen/gen_flora.py`;
  - tests `test_flora`, `perf_vegetation`, and scenarios `plants` and `forest_view`.
- Changed:
  - the terrain generator (stage 3, canopy, chunk climate);
  - streaming (the ecology ring);
  - world and engine (flora loaded from the packages);
  - the renderer (a foliage draw kind, instance buffers, per-species tints);
  - the GL backend (a foliage pass) and the terrain view (plant buffers by band, the
    canopy surface).
- Platforms:
  - Stage 3 takes about 0.7 ms per chunk (2,700 plants), and its billboards 0.2 ms, in
    release builds.
  - The foliage shader is GLSL 3.30 / ES 3.00, with dynamic indexing of a uniform array
    in the vertex shader, which ES 3.00 allows.
  - Mobile uses a smaller ecology ring.
- Co-op: plants are a pure function of the world (and seed), so every peer grows the
  same plants. Felling and growth will need world persistence and simulation (M3).
