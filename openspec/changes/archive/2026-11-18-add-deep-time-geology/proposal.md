# Proposal: Deep-Time Geology

## Why

Milestone **M2 (A Planet)** replaces noise continents with a planet that has a
geological past (concept §4.4 and §10.2, decision D13): old and young mountain ranges,
sutures, rift valleys, island chains, strata in canyon walls and minerals where geology
puts them. Climate, rivers, biomes, the ley network and the world bundle of later
changes all build on this history.

## What Changes

- **Lattice** (`world/lattice`): a Fibonacci lattice of 40,000 points (500 to
  1,000,000) with nearest neighbours and an exact nearest-point lookup.
- **Tectonics** (`world/geology`): about 500 Myr in 125 steps. Plates turn about Euler
  poles at Earth-like angular speeds. Each step resolves:
  - subduction under arcs (with trenches and andesite);
  - continental collision, where the crust thickens, rocks metamorphose and long
    collisions weld plates along sutures;
  - spreading ridges where plates diverge;
  - hotspot tracks;
  - rifting and the supercontinent cycle;
  - accretion of mature arcs and margin growth.
- **Surface processes**:
  - Isostatic elevation.
  - Erosion and downhill deposition, where the environment picks the rock: shallow-sea
    limestone, shale, river sandstone and conglomerate, desert red beds, evaporites,
    coal swamps after 360 Myr, chalk, pelagic clay and till.
  - A sea level fitted to the land target with eustatic cycles.
  - Global climate eras with ice ages and arid and humid periods.
- **Crust lineages**: continental crust remembers the craton it grew from (new terranes
  start lineages of their own), and the time when two lineages last collided is
  recorded. Biogeography uses this: continents that met recently share their life.
- **Rock record**: up to eight strata per point (rock, environment, thickness and age,
  youngest on top) above a basement. Fossils follow from a stratum's rock, setting and
  age, including mana fossils from the Age of Mana.
- **Minerals by event**:
  - arcs: copper, gold and silver;
  - granite collision belts: tin and lapis lazuli;
  - cratons: banded iron;
  - basins: salt, gypsum, coal, naphtha and bitumen;
  - volcanoes: sulfur;
  - basalt plateaus: carnelian;
  - also turquoise, karst limestone, floodplain clay, and mana crystals at sutures near
    nexuses.
- **Ley network**: nexuses at triple junctions, volcanoes, hotspots and old sutures,
  joined by a spanning tree plus loops whose paths follow sutures, rifts and boundaries.
- **Earth-likeness steering** for the targets Earth, Pangaea, Archipelago, Ice world and
  Hothouse. When a target is missed, the final quarter re-runs with derived seeds (up to
  3 times) with more or less rifting, and the closest result is kept.
- **Macro grid**: about 6 km cells (524 per face edge at 2,000 km). The lattice is
  amplified with feature-aware detail (ridged ranges that are sharper when young,
  coasts, volcanic cones), then eroded by 12 passes of implicit stream power, hillslope
  diffusion and lake infill over priority-flood routing. The output is heights, lakes,
  drainage areas and receivers.
- A phase driver with progress and cancellation (for the time-lapse globe of
  `add-world-creation`) and a golden hash for every platform.

## Capabilities

### New Capabilities

- `deep-time-geology`: the tectonic history, rock record, minerals, ley network,
  Earth-likeness steering and the macro grid.

### Modified Capabilities

None. Terrain chunks keep their noise continents until `add-world-creation` switches
them to the world bundle.

## Impact

- New code in `src/world/`: `lattice`, `geology`, `geology_final`, `geology_macro`,
  `heap.h` and `dvec.h`.
- Tests: `tests/unit/world/test_geology.c` and `tests/perf/perf_geology.c`.
- Platforms:
  - **Desktop**: a default world takes about 10 s in a release build, within the 60 s
    budget.
  - **Web**: the same code runs single-threaded in WebAssembly with the same golden
    hash. The 2-minute web budget is checked when `add-world-creation` runs it in the
    browser.
  - **Mobile**: uses fewer points or a coarser macro grid ("Quick" quality, set by
    world creation).
- Co-op: geology runs once, on the host, at world creation. Clients receive its result
  in the world bundle (`add-world-creation`) and never simulate it. The result is
  deterministic, so a seed and options reproduce it anywhere.
