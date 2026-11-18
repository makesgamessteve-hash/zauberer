# Design: Water Bodies

## Context

The bundle holds, for each macro cell (about 6 km):

- the fill level from priority flooding, which never rises along receivers;
- lake depths;
- discharge and receivers;
- the sea mask.

Rivers are far narrower than a cell, and the terrain is generated per 256 m chunk at
1 m. So channels must be made at chunk resolution, the same on every peer, without
storing anything new.

## Goals / Non-Goals

**Goals:**

- Rivers that connect into a network and reach the sea or a lake.
- Lakes with flat surfaces.
- Terrain that holds the water.
- Water you can see, fly over and float on.
- Within the stage and frame budgets.

**Non-Goals:**

- The region layer's refinements: oxbows, waterfalls, rapids, deltas as landforms.
- Shallow-water simulation (`add-shallow-water`, M3), floods and tides.
- Reflections, refraction and caustics.
- Water sounds.

## Decisions

### Channels from the macro graph, generated on demand

A channel runs from each channel cell's centre to its receiver's centre:

- 24 straight pieces on a Catmull-Rom curve through the main upstream cell (the
  tributary with the most discharge), the cell, the receiver and the receiver's
  receiver;
- meanders as a sideways offset A·sin²(πt)·sin(2πkt + φ), zero at both centres, so
  confluences always meet;
- k waves about every 14 channel widths;
- amplitude up to 8 % of the segment or 6 widths.

A point gathers the pieces of its macro cell and two rings of neighbours. Nothing farther
can reach it. A chunk gathers once, then prunes to the pieces whose banks reach it.

### Water levels

- Each node sits at its cell's fill level (bed plus lake depth). Priority flooding makes
  these non-increasing downstream; pieces interpolate between nodes and clamp, so water
  never climbs.
- A segment into the sea falls to sea level by half-way, roughly the coast.
- Below sea level, sea-level water is the sea, not a river, so river ribbons never lie
  on the sea.
- Lakes use the highest fill level of the lake cells around a point. The lake reaches a
  neighbouring cell only if that cell is shore (as high as the level), so a lake never
  floods its outlet.

### Shaping the terrain

The carve is a function of the nearest centre line (distance r, half width h, surface s,
mean depth d):

- in the channel (r < h), the bed is s − 1.5·d·(1 − (r/h)²): parabolic, with the mean
  depth d;
- across the bank (out to 2h + 4 m), the ground eases from s + 0.3 m to its own height.
  This lifts low land into levees, but never lifts the sea floor.

Hills and crags fade from zero at the bank to full over 150 m + 6h, giving valleys. The
fade is part of the 16 m macro samples. The carve is applied at 1 m in stage 1 and in the
analytic height, so both agree.

### One water query, two uses

`classify_water` decides lake, then river, then sea, for a ground height:

- The per-point query uses the carved analytic height. It drives Lua, the carpet and the
  tests.
- The chunk grid uses the chunk's own heights. With only coarse heights it lifts rivers
  just above the uncarved ground, so they stay visible far away. It also widens rivers
  by half a grid step, so a 4 m grid never misses a narrow creek.

The current is a parabolic profile along the channel plus a pull of up to half the speed
towards the middle.

### Rendering

Water meshes are separate from terrain meshes:

- They are built in the same budgeted pass and cached with the chunk.
- A coarse 16 m look rejects dry chunks cheaply.
- Dry vertices beside water take the highest neighbouring surface, so the water plane
  runs under the shore.

The pass draws after models and sprites, blended, depth-tested without depth writes and
without culling, so water is seen from above and below. Ripples use game time, so
captures stay reproducible.

## Risks / Trade-offs

- **Narrow creeks on the 4 m grid** are widened visually by up to 2 m on each side; the
  banks hide most of the excess.
- **Rivers start at full flow** where discharge first reaches 8 m³/s. The spring taper
  hides the abrupt start; smaller streams wait for the region layer.
- **Costs in river chunks** rise from about 3 ms to about 5 ms for stages 1–2. That is
  within the 8 ms stage 1–3 budget and is measured in `perf_water`.
