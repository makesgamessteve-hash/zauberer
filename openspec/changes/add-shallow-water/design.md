# Design: Surface Water

## Context

`sim/fire` holds the active chunks: their 2 m cells, the neighbours across edges and
seams, and the survey around focus points. The terrain knows its fixed water bodies
(sea, lakes, rivers) per point (`zb_terrain_water_grid`). Chunks carry 1 m heights and
surface materials.

## Goals / Non-Goals

**Goals:**

- Water that behaves: runs downhill, pools flat, soaks in, drains away.
- Exact bookkeeping of volume, tested as an invariant.
- No cost where water is still; deterministic.

**Non-Goals:**

- Lava and other liquids.
- Snow and its melt.
- Ground moisture feeding plants and fire.
- Rivers that rise and overflow.
- Water pushing the carpet or floating things (rigid bodies, M3).
- Erosion.
- Digging and dams; `add-terrain-deformation` changes the ground and tells the grid.

## Decisions

### One active grid for every active-tier field

The chunk set, cell numbering, neighbour links and the survey move out of `sim/fire`
into `sim/active`. Each simulation owns a `zb_active` with hooks that build and free its
per-chunk data. A grid names the stage it needs: fire waits for plants (stage 3) and
water for the surface (stage 2). Two grids over the same chunks cost two surveys a
second, which is negligible, and keep the simulations independent.

### The pipe model

Each cell keeps four outflows (m³/s), one per side. A step does three things:

1. Each flow gains `dt × g × (h_i − h_j) × width`, where `h` is the water surface. It
   is damped by 0.99 per step, which acts as bed friction and lets sloshing settle in
   about a minute, and it is clamped at zero.
2. If the total would take more than the cell holds in one step, all four are scaled
   down so that it is emptied exactly.
3. Each cell's outflow volume is moved into its neighbours, with depth changed by
   volume over each cell's own area, so cell sizes that differ across chunks still
   conserve volume.

All outflows are computed before any water moves, which keeps the result independent
of cell order. Depths are doubles. Moving 10⁻⁷ m onto a 1 m column is below a float's
resolution, and in floats that rounding kept still water moving forever. The model is the standard "virtual pipes" scheme. It is stable for 2 m
cells at 20 Hz up to tens of metres of depth.

### Fixed water and the tier's edge

Cells under the terrain's water bodies keep their level. Water reaching them leaves the
simulation as drained, and they never send water out. The edge of the active tier is a
wall. Water there waits until the tier moves.

### Rain without waking everything

Rain falls on awake chunks every step. Sleeping chunks get theirs in one-second
batches. Only cells where the ground cannot take it all get standing water, and those
wake their chunk. So drizzle on a meadow costs a pass over each chunk once a second,
not 20 times.

### Sleep

A chunk whose largest flow stays under 10⁻⁵ m³/s for 40 steps (2 s) sleeps. Its depths
stay, and its flows reset to zero. Water flowing in, rain left standing, or water poured
in wakes it.

### Drawing

The terrain view builds, per chunk with standing water deeper than 2 cm, a mesh of
quads at the water surface. It uses the river water's vertex format and shader. Meshes
are rebuilt at most every half second for chunks whose water moved.

## Risks / Trade-offs

- Water at the tier's edge piles up until the tier moves. A near-tier model can come
  with simulation tiers.
- Depths drift by rounding over long runs, so the invariant is tested to 0.1 %.
