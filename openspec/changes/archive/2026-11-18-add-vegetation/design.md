# Design: Vegetation

## Context

The terrain has stages 0–2 per 256 m chunk, and created worlds carry monthly climate
and biomes. The renderer has terrain meshes, models, sprites and, since
`add-water-bodies`, water. Plants must cover tens of thousands of instances near the
player and whole forests to the horizon.

## Goals / Non-Goals

**Goals:**

- Plants that follow climate, biome and ground.
- Deterministic everywhere.
- Within the stage and frame budgets.
- A forest that reads from the ground to the horizon.
- Seasons that can be driven later.

**Non-Goals:**

- Growth, succession, burning and felling (M3 simulation).
- Fauna populations (M5).
- Trunks as meshes.
- Dithered LOD fades.
- Per-plant collision.

## Decisions

### Data-driven species, climate niches

Species live in a LUON data file, so mods can add them; the registry is per world. The
suitability is the product of two parts:

- Climate: two quadratic niches (temperature and rain, 1 at the optimum, 0 at the
  limits) and a frost cut-off on the coldest month.
- Ground: material, slope and wetness checks.

The climate part is computed once per 16 m sample, since it varies slowly. Ground
checks run per candidate.

### Layers on jittered grids

A jittered grid per layer is a cheap blue-noise approximation: no two trees closer than
the jitter allows, with no dart-throwing loop. The keep probability is
min(0.95, Σ suitability × density × fill × cover). Fill is a per-layer constant: 0.6
for trees, 0.25 for shrubs, 0.35 for ground cover. Cover comes from a per-biome table.
Forests fill most tree cells, savannas few, and deserts almost none.

### Stage 0 canopy for the far levels

Far clumps and the horizon canopy need tree cover long before stage 3. Stage 0
stores 81 bytes of cover (every 32 m) and the dominant tree species, computed from the
same niches on the coarse grid. The stage-0 struct stays under 2 KB.

### Instance buffers per chunk and band

Each cached chunk entry holds one foliage buffer, built for its current band:

- 0: every plant;
- 1: trees and shrubs;
- 2: clumps from the canopy.

It is rebuilt when the band or the chunk data changes, within a per-frame budget.
Instances (52 bytes) are in the chunk's tangent frame like the terrain mesh, so the
chunk transform places them.

The shader builds the billboard about the chunk's up axis, so trees stay upright near
the curved horizon. It indexes a per-species tint array (64 entries) for the season.
Mirroring is encoded by swapping the atlas u coordinates.

### The horizon is the terrain

At terrain LOD 3 and up (32 m and 64 m grids, beyond about 4 km), the mesh builder
raises vertices by the canopy cover × 14 m and blends their colour to canopy green.
Forests stay visible as a bumpy mass at no extra draw cost.

### The noise world gets a climate too

The default flight world has no bundle, so it gets a simple climate:

- 27 °C at the equator, falling with the square of the latitude's sine;
- seasons growing poleward;
- rain from a large-scale noise field, drier towards the poles.

The climate's own Whittaker classification gives the biome. Flights over the noise
world therefore also cross savannas, forests and taiga.

## Risks / Trade-offs

- **Billboards and fog of near plants**: alpha-tested billboards are cheap but flat.
  Crossed cards and trunks can come with the M8 visual pass.
- **Far clump overdraw**: up to 81 clumps per chunk at a distance. The frame stays
  within budget on llvmpipe in the tests; per-cluster culling can come later.
- **Seasons are static until `add-sky-and-time`** drives them; the default is summer.
