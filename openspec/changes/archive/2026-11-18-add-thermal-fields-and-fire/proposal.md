# Proposal: Heat and Fire

## Why

Fire is the first living force of M3 (concept 5.5 and 5.6). The M3 exit demo starts with
it: lightning ignites a dry forest. The fire runs downwind and uphill and stops at a
river, rain douses it, and the burn scar regrows. Fire is also the first thing a mage
does with the Fire colour in M4, so heat must flow from any source, not only flames.

## What Changes

- **Heat and fire in the active tier** (`sim/fire`). Every loaded chunk with its plants
  within the active radius (384 m on desktop) gets a grid of 128 × 128 cells of about
  2 m. Each cell holds:
  - its temperature above the ambient air (from the climate, season, time of day and
    height);
  - its fuel (grassy ground, ground cover, shrubs, tree crowns and litter) with its
    material from the registry;
  - the fuel's moisture, which approaches the climate's balance and rises with rain.
- **The model.**
  - Heat diffuses between cells and is lost to the air.
  - Water in the fuel boils off at 100 °C before the fuel gets hotter.
  - A dry cell ignites at its fuel's ignition temperature (each cell a little
    different) and burns at the material's rate.
  - A burning cell gives a share of its heat to its eight neighbours, weighted
    downwind (wind) and uphill (slope).
  - Crown fires throw embers downwind that start spot fires.
  - Water cells (sea, lakes, rivers), bare ground and wet fuel do not burn, and rain
    puts fires out.
  - Cross-chunk neighbours are found through the planet, across cube-face seams too.
  - Chunks with nothing above ambient sleep and cost nothing.
  - Everything is deterministic.
- **Burn scars** (`world/delta`, `ZB_EDIT_BURNT`):
  - a burnt-out cell is dated in the world's deltas and saved like any edit;
  - the ground is ash (a new surface material) for 20 game days;
  - plants grow back by layer: ground cover after 15 days, shrubs after a year
    (48 days) and trees after five years;
  - regenerated chunks show this, so a scar visited 30 days later has early regrowth.
- **Engine**: the simulation runs at 20 Hz in real time (fire is physical, like flight),
  follows the world's focus points, and is redrawn as animated flame sprites near the
  camera.
- **Lua**:
  - `sim.ignite`, `sim.add_heat` and `sim.fire_at`;
  - `sim.fire_stats`;
  - `sim.set_weather` and `sim.weather` (wind and rain, until `add-weather` drives
    them).
- **Art**: an animated flame sprite (`tools/gen/gen_sprites.py`).

## Capabilities

### New Capabilities

- `thermal-and-fire`: heat, fuel, ignition, spread, embers, extinguishing and burn
  scars in the active tier.

### Modified Capabilities

- `world-persistence`: a new edit kind, burnt cells, that heals with game time.
- `materials`: the terrain gains an ash surface, which must be in the registry.

## Impact

- New: `src/sim/fire.{h,c}`, the flame sprite, tests `test_fire` and a scenario.
- Changed:
  - `world/delta`: burnt edits; edits are indexed per chunk so that replacing one is
    no longer a linear search;
  - the world: `zb_world_burn`, `zb_world_focus`, and deltas applied at the world's
    time;
  - the terrain: the ash material;
  - the terrain view: ash drawn in its flat colour;
  - the engine (create, update, draw) and Lua `sim`.
- Performance: a sleeping chunk costs nothing. A fully burning chunk costs about 1 ms
  per step on desktop (measured in `perf_fire`). The budget is the 6 ms simulation
  share per tick.
- Determinism: the fire is deterministic for the chunks it has. Which chunks it has
  depends on when the stream loads them, so the fire stays out of the replay state hash
  until `add-simulation-tiers` makes chunk arrival deterministic.
