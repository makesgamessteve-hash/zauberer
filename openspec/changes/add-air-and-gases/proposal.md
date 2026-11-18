# Proposal: Air and Gases

## Why

Concept 5.8 has the active tier's air carry what happens in it:

- smoke from fires, steam from boiling water, fumes from alchemy and volcanoes;
- the scent of every creature, so predators track prey and the player approaches
  from downwind.

The wind itself is local: faster over crests, sheltered in hollows and forests, turned
along slopes. Fire already leans with the wind, but with one wind for the whole planet.
M4's spells and M5's creatures need the air as a field to read and write.

## What Changes

- **The active grid takes a resolution.** Fire and water keep 128 cells per chunk.
  Air uses 32 (8 m cells), so a smoky sky costs a sixteenth.
- **Local wind** (`sim/air`): the weather's wind per 8 m cell, shaped by the ground:
  - faster over crests and slower in hollows, by height above the ground around;
  - slower under forest canopy;
  - turned along a slope rather than up it.
- **Gases**: smoke, steam, scent and fumes, as concentrations in the lowest air layer:
  - taken along by the local wind (semi-Lagrangian, so any wind is stable);
  - spread a little;
  - lost at each gas's rate: steam condenses in seconds, smoke rises and thins in
    minutes, and scent lingers.

  Sources emit kilograms. A chunk with no gas sleeps; gas reaching an edge wakes the
  neighbour.
- **Coupling**:
  - burning cells release smoke into the air;
  - fire leans with the local wind of its chunk instead of the weather's.
- **Engine and Lua**:
  - the air runs at 4 Hz around the focus points, and `sim.set_weather`'s wind drives
    it;
  - `sim.wind_at`, `sim.gas_at`, `sim.emit` and `sim.air_stats`;
  - smoke is drawn as drifting puffs (generated sprite art).

## Capabilities

### New Capabilities

- `air-and-gases`: local wind and the gases it carries in the active tier.

### Modified Capabilities

- `thermal-and-fire`: fire releases smoke and leans with the local wind.

## Impact

- New: `src/sim/air.{h,c}`, the smoke sprite, tests `test_air`, `perf_air` and a
  scenario.
- Changed: `sim/active` (a resolution per grid), `sim/fire` (smoke and local wind), the
  engine, and Lua `sim`.
- Not yet:
  - air layers above the lowest, thermals and fire-driven inflow;
  - oxygen and closed rooms (with interiors);
  - creature scents by species (M5);
  - toxic effects on bodies (survival, M6).
