# Proposal: World Creation

## Why

Milestone **M2 (A Planet)** ends with an exit demo that begins on the new-world screen:
the player watches the continents drift in a time-lapse, then flies in the world that
was made. Deep-time geology (`add-deep-time-geology`) and the present climate
(`add-climate-and-biomes`) exist as libraries. This change makes them a game feature:

- a creation pipeline the game can drive without stalling a frame;
- the options of concept §4.9 that generation uses so far;
- the time-lapse globe of concept §10.13;
- cancellation;
- the **world bundle**, which stores everything the long simulations computed so that
  loading never re-runs them (concept §4.3);
- terrain that follows the created world instead of the M1 noise world.

## What Changes

- **Creation pipeline** (`world/creation`):
  - three stages: deep time ("Drifting continents"), relief ("Raising mountains,
    carving valleys") and climate ("Winds, rain and rivers");
  - each stage advances in bounded pieces: a deep-time step, an erosion pass or a month
    of climate;
  - progress reports the stage, a fraction that never goes back, millions of years ago,
    the time-lapse frames recorded so far and the Earth-likeness attempt;
  - cancelling at any point frees everything.
- **Options**: seed, radius, geology target, land fraction, temperature offset, axial
  tilt and quality. The quality presets are:
  - *quick* for mobile: 12,000 points, 80 steps, about 12 km macro cells;
  - *normal*: 40,000 points, 125 steps, about 6 km cells;
  - *high*: 80,000 points, 160 steps, about 4 km cells, capped at 1,024 cells per face
    edge.
  Invalid options are refused with a message.
- **Time-lapse**: one frame per deep-time step, recording each lattice point's elevation
  above that era's sea level and its plate. The globe is a cube-sphere mesh coloured by
  height while deep time runs and by biome once the world is finished. It turns on the
  new-world screen, with the stage, percentage and era counter beside it.
- **World bundle** (`world/bundle`): a versioned file holding:
  - the macro grid (height, lakes, drainage, discharge, groundwater, receivers, biomes,
    river orders, provinces, flags);
  - monthly climate;
  - the lattice rock record (crust, strata, minerals, lineages);
  - the ley network, provinces, lineage contacts and eras.
  Each section is zlib-compressed with its own checksum, and unknown sections are
  skipped. Damaged, truncated or newer files are refused with a message.
  Queries by direction give height (smooth across cube edges), biome, cell, lattice
  point and monthly climate at any altitude.
- **Terrain v2**: a world made from a bundle takes its heights from the macro grid, adds
  hills and crags below the macro scale, and picks surface materials by biome (sand
  deserts, ice sheets, scree, wetlands, snow above the snow line). The M1 noise world is
  unchanged.
- **Game integration**:
  - The new-world screen (key N or gamepad Y) runs creation on a worker job.
  - Enter flies in the finished world: the bundle is written to
    `<user dir>/worlds/<name>.zbw` and becomes the played world.
  - The world's name is remembered, so the next start continues in it.
  - Lua reads what a place knows: biome, monthly climate, strata down to the basement,
    minerals and province.

## Capabilities

### New Capabilities

- `world-creation`: the creation pipeline, its options, the time-lapse globe,
  cancellation, the world bundle, terrain from a created world, and the game's new-world
  flow.

### Modified Capabilities

None. Terrain v2 adds to the terrain generator without changing the noise world's
requirements.

## Impact

- New: `src/world/creation.{h,c}`, `src/world/bundle.{h,c}`, `src/core/deflate.{h,c}`
  (zlib through stb), `src/script/script_create.c`, and
  `assets/base/scripts/create/screen.lua`.
- Changed: the terrain generator (bundle heights and biome materials), world (an
  optional bundle), the engine (`host.use_bundle`, jobs for scripts), and `main.lua` (the
  "create" mode).
- Tests:
  - `tests/unit/world/test_creation.c`;
  - scenarios `create_world` (headless) and `create_globe` (rendered);
  - engine tests in `test_headless.py` and `test_render.py`.
- Platforms: the pipeline is deterministic. The bundle's golden hash is the same
  natively and in WebAssembly. On the single-threaded web build, creation advances a few
  pieces per frame instead of running on a worker. *Quick* quality is the mobile preset.
- Co-op: the host creates the world. The bundle file is what a joining heir will receive
  (`add-coop-sessions`, M9), so clients never re-run deep time.
