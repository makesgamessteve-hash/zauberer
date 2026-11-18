# Proposal: Simulation Tiers

## Why

Concept 5.2 and 5.3 put the simulation in tiers with their own rates, and make the world
go on while nobody watches: a dormant chunk that comes back after days catches up.
Concept 5.15 budgets the whole simulation at 6 ms a tick on desktop.

Fire, water and air so far were created, stepped and coupled ad hoc by the engine:

- nobody measured them against the budget;
- standing water did not put fire out;
- a fire burning when its chunk left the active tier simply vanished.

The M3 exit demo needs the last of these: fly away, skip 30 days, and come back to a
burn scar.

## What Changes

- **One owner for the active tier** (`sim/tier`). It:
  - creates heat and fire (with the material registry), surface water and the air;
  - steps them at their own rates around the world's focus points;
  - follows a replaced world.

  The engine and Lua go through it.
- **Couplings in one place**:
  - the weather in use drives all three;
  - storms send lightning;
  - fire smokes into the air and leans with its local wind;
  - new: standing water deeper than 2 cm puts fire out and soaks its fuel, once a
    second.
- **The budget**: each simulation is timed per tick and averaged per second against the
  6 ms budget. Seconds over budget are counted, with a warning at most once a minute.
- **Catch-up for fire**:
  - a chunk that leaves the active tier while burning is remembered;
  - back within ten game minutes, the fire burns on;
  - back later, the dry fuel connected to it has burnt, up to about 1.5 km per game day
    (less in damp climates), within the chunk;
  - those scars are dated when the chunk left, so regrowth counts from then.
- **Lua**: `sim.tier_stats()` gives the milliseconds per simulation, the budget, the
  seconds over it, and the fires remembered.

## Capabilities

### New Capabilities

- `simulation-tiers`: the active tier as one scheduled, budgeted, coupled whole, and
  catch-up for chunks that come back.

### Modified Capabilities

None.

## Impact

- `src/sim/tier.{h,c}` grows from a weather helper into the tier's owner.
- `sim/fire`: dormant fires, catch-up, and soaking by water.
- `world`: `zb_world_burn` takes the time of the burn.
- The engine loses its glue.
- Tests: `test_tier`, the catch-up test in `test_fire`, and the `tier` scenario.
- Not yet:
  - the near tier's coarser fields and fire fronts;
  - the far tier's aggregates;
  - catch-up for water, plants and creatures;
  - spreading over several chunks during catch-up;
  - deterministic chunk arrival for replays.
