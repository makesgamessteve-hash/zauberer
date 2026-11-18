# Design: World Creation

## Context

Concept §4.3 runs the planet layer once, at world creation, and stores it in the world
bundle. §4.9 lists the new-world options, and §10.13 describes the time-lapse globe.
§10.14 sets budgets:

- Phase 1 (deep time): ≤ 60 s on the desktop, ≤ 2 min on the web, and ≤ 2 min for
  *quick* quality on mobile.
- Phases 2 + 3A + 4: ≤ 15 s on the desktop.

Geology and climate are already deterministic libraries with their own budgets.

## Goals / Non-Goals

**Goals:**

- Creation that never blocks a frame.
- A file format that loads fast and survives damage gracefully.
- Terrain that matches the created world.
- A playable loop: create, fly, quit, continue.

**Non-Goals:**

- History and peoples (M8).
- Magic intensity, story and difficulty options.
- Rendered water (`add-water-bodies`).
- Vegetation.
- Saving the player's changes (`add-world-persistence`).
- Sending bundles to co-op clients (M9).

## Decisions

### Pieces of work, not threads inside the pipeline

`zb_world_creation_advance` does one bounded piece and returns. The game runs pieces on a
low-priority worker job and reads progress only between jobs, so no locks are needed and
the main thread never waits. Without workers (the single-threaded web build) the script
advances a few pieces per frame. The pieces are:

- a deep-time step;
- the macro-grid phases;
- one erosion pass;
- the climate's phases (grid, months, hydrology, biomes, provinces).

The fraction weights deep time at 70 %, relief at 15 % and climate at 15 %.

### Time-lapse frames from the lattice

After each deep-time step, the pipeline records each lattice point's elevation (int16,
metres above that era's sea level) and its plate. That is 3 bytes per point per frame:
about 9 MB at normal quality. The globe maps each of its vertices to the nearest lattice
point once, then recolours per frame without a search. While a job works on frame
*n + 1*, the screen shows complete frames only.

### The bundle format

The header holds:

- the magic `ZBWB`;
- the bundle version;
- the generator version;
- the options;
- the metrics.

Sections follow: `MACR`, `CLIM`, `LATT`, `STRA`, `LEYN`, `PROV`, `ERAS` and `CONT`. Each
section is:

- a tag;
- its raw and compressed sizes;
- a 64-bit hash of the raw bytes;
- a zlib stream (stb's encoder and decoder, so there is no new dependency).

The reader:

- checks every size against the grid and point counts before trusting it;
- refuses a newer bundle version;
- skips unknown tags (forward compatibility within a version);
- refuses a file that misses a required section.

Lattice positions are not stored: they follow from the point count.

### Terrain v2

`zb_terrain` gains an optional bundle. Its macro height is the bundle's smooth height,
interpolated across cube edges (`zb_cubegrid_sample_smooth`). Hills (about 1 km) and
crags (about 100 m) are added, scaled by relief, so the terrain stays within a few
hundred metres of the macro grid.

Each chunk recomputes its biomes from the bundle when it is generated rather than
storing them, so the stage-0 footprint test still holds. A biome gives a surface material
by height, slope and wetness. The noise world keeps its own path and its golden hashes.

### Files and names

World names use letters, digits, `-` and `_`, which keeps paths inside
`<user dir>/worlds/`. The engine swaps the played world in place:

- a new world and terrain view from the bundle;
- scripts see the new world;
- the old world is freed after the new one is in place.

## Risks / Trade-offs

- **Time-lapse memory on high quality** (about 19 MB). Acceptable on desktop; mobile
  uses *quick* (about 3 MB).
- **Bundle size** at normal quality is about 30 MB compressed. That is fine on disk.
  Co-op transfer (M9) will stream it in pieces.
- **Generator version**: worlds record version 1. Changing a generator later will mean
  a new version and keeping the old one, as concept §4.8 requires.
