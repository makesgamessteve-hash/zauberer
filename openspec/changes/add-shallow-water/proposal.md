# Proposal: Surface Water

## Why

Concept 5.7 gives the active tier its own water: puddles, runoff and floods, water a
spell summons, and ponds that drain. Mass is conserved. The rivers, lakes and seas of
`add-water-bodies` are fixed: they never spill or fill. The M3 exit demo breaches a
natural dam and floods a valley, and asks that water mass invariants hold in tests.
Rain from the coming weather needs somewhere to go: into the ground or downhill.

## What Changes

- **The active grid** (`sim/active`). The set of active chunks and their 2 m cells,
  taken out of `sim/fire` so that every active-tier simulation shares it. It provides:
  - the survey around the focus points;
  - neighbours across chunk edges and cube-face seams;
  - lookups by position and walks over an area.

  Fire keeps its behaviour and is now a user of the grid.
- **Surface water** (`sim/water`), a shallow-water pipe model on the grid:
  - each cell holds the water on it and sends water to its four neighbours, driven by
    the difference of their water surfaces;
  - outflow is limited to what a cell holds, so depths never go negative;
  - volume is conserved, and every cubic metre is accounted for: on the ground,
    soaked in, drained into a fixed water body, rained in or added.
- **The ground**: water soaks in at a rate and up to a capacity by surface material.
  Sand takes it quickly, soil steadily, mud barely, and rock, snow and ice not at all.
- **Fixed water**: the terrain's sea, lakes and rivers take water that flows into them
  (drained) and keep their level.
- **Rain**: water falls on every cell; what the ground cannot take runs off.
- **Sleep**: still water sleeps with its depths kept, and inflow, rain or water poured
  in wakes it.
- **Engine and Lua**:
  - the simulation runs at 20 Hz around the focus points;
  - `sim.add_water`, `sim.water_at` and `sim.water_stats`;
  - `sim.set_weather` sets its rain too;
  - standing water is drawn with the water shader.

## Capabilities

### New Capabilities

- `shallow-water`: surface water in the active tier: flow, pooling, soaking, draining,
  rain and the volume balance.

### Modified Capabilities

None. Fire's requirements are unchanged by moving onto the shared grid.

## Impact

- New: `src/sim/active.{h,c}`, `src/sim/water.{h,c}`, tests `test_surface_water`, a
  scenario, and `perf_surface_water`.
- Changed:
  - `sim/fire` (uses the grid);
  - the engine (create, update, draw standing water);
  - the terrain view (a mesh of standing water per chunk);
  - Lua `sim`.
- Performance: a sleeping chunk costs nothing. A chunk of moving water costs about
  0.5 ms per step (budget in `perf_surface_water`).
- Not yet: lava, snow melt, ground moisture feeding plants and fire, rivers rising and
  overflowing, and digging (`add-terrain-deformation` breaks dams).
