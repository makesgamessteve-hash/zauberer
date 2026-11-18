# Proposal: Terrain Chunks

## Why

Milestone **M1 (First Flight)** needs ground to fly over. The planet model from
`add-planet-coordinates` addresses every chunk, but nothing fills a chunk yet. The full
world generator (deep-time geology, climate, biomes) arrives in M2; M1 needs a first,
fast, deterministic terrain so that streaming, the retro renderer, flight and co-op can
be built and tested against real landscapes.

## What Changes

- **Deterministic 3D gradient noise** (`world/noise`): improved Perlin noise with hashed
  lattice gradients, fractal sums and ridged multifractals, evaluated at planet
  positions so it is seamless across cube faces and identical on every platform.
- **Continents v1** (`world/terrain`): a continent field with a sea level fitted to a
  35 % land fraction, ocean basins, lowlands, hills and ridged mountain belts.
- **Chunk stages 0–2** (concept §4.6):
  - stage 0 *Coarse*: a 17 × 17 grid of heights (16 m apart) and surface materials, for
    the horizon,
  - stage 1 *Terrain*: a 257 × 257 heightfield (1 m apart, edges shared with the
    neighbours) with detail noise,
  - stage 2 *Surface*: a 256 × 256 grid of surface materials (soil, sand, rock, gravel,
    snow, ice, mud) from height, slope and latitude.
  River beds and strata columns, also listed for stage 2 in the concept, need the
  hydrology and geology of M2 and are added there.
- **Lua access**: `world.height(pos)` and `world.surface(pos)`.

## Capabilities

### New Capabilities

- `terrain-generation`: noise, the continent model v1, chunk stages 0–2, their
  determinism, seams, memory and time budgets, and scripting access.

### Modified Capabilities

None.

## Impact

- `src/world/` gains `noise` and `terrain`; the world seed is the run's `--seed`.
- Platforms: no differences in output. Desktop, web, mobile web and XR generate the same
  bytes; the golden hashes are checked on the Linux, Windows and WebAssembly CI jobs. The
  time budget is checked on the Linux release job only (the other jobs share runners of
  unpredictable speed).
- Co-op: terrain is never sent over the network. Every peer generates it locally from the
  world seed, which the host hands to clients when they join (add-netcode-foundation).
