# Design: Simulation Tiers

## Context

Fire (20 Hz), water (20 Hz) and air (4 Hz) each keep their own accumulators and active
grids. The engine created them, stepped them in order, emitted smoke, fetched the local
wind, computed the weather once a second, and sent lightning. Chunks leave the active
tier when the player moves on, and their fire state is freed.

## Goals / Non-Goals

**Goals:**

- One place that runs and couples the active tier, measurable against the budget.
- A plausible world on return: fires that went on, scars dated when they burned.

**Non-Goals:**

- Near and far tiers with coarser models.
- Catch-up for other systems (they have no state outside the tier yet).
- Multi-threading the tier (jobs come when a profile shows the need).

## Decisions

### `sim/tier` owns, the engine draws

The tier creates the three simulations with one radius (384 / 256 / 160 m by profile) and
steps them in a fixed order:

1. fire;
2. water;
3. smoke into the air, then the air;
4. once a second: the weather, lightning, and water soaking fire.

The engine keeps borrowed pointers only for drawing. Scripts reach the weather control and
the tier through the script config.

### Timing with the platform clock

The tier reads a clock (the engine passes the platform's) around each simulation and
averages per tick over each second. Without a clock (tests, tools), nothing is timed.
Over-budget seconds are counted, with at most one warning a minute, so a slow machine
does not flood the log.

### Water puts fire out

Once a second, every cell with more than 2 cm of standing water tells fire to stop
burning there and soak its fuel to a moisture of 1 kg/kg. Rivers and lakes already never
burn, because fire marks them as water when a chunk is taken in.

### Catch-up by flood fill

When fire lets go of a chunk, its pending scars go to the world. Its burning cells are
kept with the game time, up to one record per possible chunk. When the chunk is taken in
again:

- under 10 game minutes later (about 17 real seconds at the default day length): the
  recorded cells burn again;
- later: a breadth-first flood fill from them over cells with fuel and fuel moisture
  under extinction, eight neighbours, up to `days × 1,500 m × (1 − equilibrium moisture
  / extinction)` cells away, within the chunk. Every reached cell keeps a tenth of its
  fuel and is scarred, dated when the chunk left.

The world applies the scars at its current time, so after 30 days the grass is back and
the ash gone, but shrubs and trees are still missing.

## Risks / Trade-offs

- The fill ignores wind and slope, and stops at the chunk's edge. Good enough to make a
  return plausible; a near-tier front model will do better.
- Records are lost when the world is replaced, by design: they belong to that world.
