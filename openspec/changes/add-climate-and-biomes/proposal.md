# Proposal: Climate and Biomes

## Why

Milestone **M2 (A Planet)** promises a desert because of a rain shadow, a river that
always reaches the sea or a salt lake, and forests where the climate allows them
(concept §4.4 and §10.3). The deep-time geology gives the shape of the land. This change
gives it its present: climate, water, biomes, magical land and the biogeographic
provinces that species placement will use.

## What Changes

- **Climate** on a climate grid at a quarter of the macro resolution (about 24 km):
  - Monthly temperature comes from top-of-atmosphere insolation with the axial tilt
    (23° by default), a radiative balance plus poleward heat transport, seasonal swings
    that grow with continentality (distance from the sea) and are damped over the ocean,
    the altitude lapse rate (6.5 °C/km), and ocean currents: cold along subtropical west
    coasts and warm along their east coasts, reversed in subpolar gyres.
  - Monthly precipitation comes from moisture that three-cell prevailing winds (trades,
    westerlies, polar easterlies, shifted with the thermal equator) carry from
    evaporating oceans. It rains out faster in the intertropical convergence and under
    orographic lift, slower under the subtropical highs and in sinking lee air, which
    gives rain shadows. Land returns part of its rain.
- **Hydrology** on the macro grid:
  - River discharge from runoff (Budyko-style evaporation), accumulated downstream along
    the geology's receivers.
  - Rivers above 8 m³/s with a Strahler order, mouths, and great rivers over
    2,000 m³/s.
  - Lakes that lose their water to evaporation become salt lakes.
  - Glaciers and ice sheets, sea ice, groundwater depth, and oases where desert
    groundwater is shallow.
- **Biomes** per macro cell, as in the concept's table:
  - aquatic: deep ocean, shelf, coral reef, kelp, sea ice, lakes and great rivers;
  - coastal: beach, dunes, cliffs, mangroves, salt marsh, estuary;
  - Whittaker biomes from annual temperature, precipitation, the warmest month and the
    dry season;
  - mountain biomes above 1,200 m: cloud forest, montane forest, alpine meadow, scree,
    volcanic slopes, glacier;
  - wetlands on flat lowland: swamp forest, floodplain, peat bog, marsh;
  - the eight **magical biomes** around the strongest ley nexuses (strength ≥ 0.75),
    chosen by their setting.
- **Biogeographic provinces** over landmasses, seeded on each crust lineage and grown
  over land and narrow straits. They are slowed by high mountains, deserts and ice, and
  stopped by lineage borders unless the lineages met within the last 50 Myr. Each
  province has an isolation (endemism) value, and pairs of provinces have an affinity.
- Shared code: `world/cubegrid`, the cube-sphere grid now used by geology's macro grid
  too (no change in results). Geology records **crust lineages** and when they last
  collided (an addition to `add-deep-time-geology`, which is still open), and gives
  climate its input (`zb_climate_input_from_geology`).

## Capabilities

### New Capabilities

- `climate-and-biomes`: present climate, hydrology, biomes, magical biomes and
  biogeographic provinces.

### Modified Capabilities

None (the lineage requirement is part of the still-open `deep-time-geology` change).

## Impact

- New `src/world/climate.{h,c}` and `src/world/cubegrid.{h,c}`. Geology moves its macro
  grid onto `cubegrid` and gains lineages.
- Tests: `tests/unit/world/test_climate.c` and a climate budget in
  `tests/perf/perf_geology.c`.
- Platforms: everything is single-threaded and deterministic. A default world takes
  about 1.6 s on the desktop (budget 15 s for phase 2). WebAssembly gives the same golden
  hash.
- Co-op: climate is computed once at world creation on the host and travels in the world
  bundle (`add-world-creation`). Clients never recompute it, and seasons and weather
  will read it on every peer.
