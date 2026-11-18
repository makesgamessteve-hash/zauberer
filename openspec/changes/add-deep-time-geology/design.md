# Design: Deep-Time Geology

## Context

Concept §10.2 asks for a simplified, fast tectonic history in the spirit of
*Procedural Tectonic Planets* (Cortial et al., 2019): a sphere sampled with
40,000–160,000 points, 100–250 steps of 2–5 Myr, processes at plate boundaries,
surface processes, a rock record, minerals by event, a ley network along old
structures, Earth-likeness steering and amplification onto the 6 km macro grid. Budget:
≤ 60 s on the desktop, ≤ 2 min in a desktop browser.

## Goals / Non-Goals

**Goals:**

- A plausible history with fixed costs: no global remeshing, bounded work per step.
- Exact determinism on every platform (one golden hash for native and WebAssembly).
- Data the later layers need: heights, drainage, lakes, strata, minerals, ley lines and
  eras (for the time-lapse and the chronicle).

**Non-Goals:**

- Present climate, rivers as features, biomes: `add-climate-and-biomes`.
- The creation pipeline, its user interface, jobs and the world bundle file:
  `add-world-creation`.
- Feeding terrain chunks from the macro grid: also `add-world-creation`.

## Decisions

### Fixed lattice, crust advected semi-Lagrangian

Points never move: each step, every point looks back along each plate's rotation and
takes the crust found there if it belongs to that plate. A point that no plate reaches
is a spreading gap (new ocean floor). A point that several plates reach is a
convergence: the most buoyant crust stays (continental, then thick oceanic, then young
oceanic) and the rest subducts, or collides when it is buoyant too. Each plate has a
bounding cap, so a point usually tests only 2–3 plates. Lookups descend the neighbour
graph from the point itself, then check neighbours of neighbours, which matches a brute
force search exactly (unit-tested). A default world runs 125 steps × 40,000 points in
about 4 s.

Alternative: moving points with periodic re-triangulation (Cortial et al.). It
preserves crust exactly but needs a robust spherical Delaunay triangulation and more
time.

### Crust conservation

Collisions transfer 90 % of the lower plate's crust to the upper one. Crust thicker than
55 km spreads onto the ocean floor beside it. Arc crust thicker than 18 km becomes
continental. When collisions have shortened continents below their starting area, the
shallowest ocean floor on continental margins accretes, a little per step (sediment
shelves and terranes). Without this, continents shrink by half over 500 Myr and raised
ocean floor ends up as "land".

### Elevation by isostasy

Continental elevation is 400 m + 180 m for each km of crust beyond 35 km (Airy).
Oceanic elevation is −(2600 + 340 √age) m, at most −6400 m, plus 120 m for each km of
thickening. Trenches beside arcs are 2.5 km deeper. Each step fits the sea level to the
land target, then shifts it by a slow eustatic cycle and −100 m in ice ages.

### Surface processes on the lattice

Land erodes at 0.87 % of its height above the sea per Myr: mountains halve in about
80 Myr, and ice scour makes it 1.6× faster. The eroded rock follows steepest descent:

- In the sea it becomes limestone in warm water and shale elsewhere.
- In arid subtropical shallows it becomes gypsum and salt.
- In closed basins it becomes salt in arid subtropics, red sandstone in deserts and
  coal in humid lowlands.
- Coastal deltas in humid eras get coal.
- Otherwise it becomes sandstone, or conglomerate below high mountains.

The deep sea receives pelagic clay, chalk or limestone. Climate eras come from slow
cycles plus noise, and continents near the poles cool the planet. Ice ages mark land
above 52° latitude, or high land above 30°, as glaciated, and leave till once the ice
retreats.

### Strata

Each point has 8 strata (6 bytes each) with the youngest on top. Similar strata deposited
within 60 Myr merge. When the column is full, the two oldest strata merge. Erosion
removes strata from the top, so old strata are exposed where erosion is strong.
Collisions metamorphose strata (shale → schist, limestone → marble, sandstone →
quartzite) and turn granite basement into gneiss.

### Steering

At 75 % of the span the state is saved. After the last step the metrics are computed:

- land fraction;
- landmasses over 1 % of the surface, and large ones over 2 %;
- mountains on every large continent;
- young (< 60 Myr) and old (> 200 Myr) ranges;
- land ice from a latitude and altitude temperature;
- climate bands;
- the share of land on continental crust.

Each target scores its own criteria. A miss restores the saved state and re-runs it
with a derived seed (up to `max_retries`). Too few continents raise the chance of
rifting, too many lower it, and the best-scoring attempt is kept.

### Macro grid and erosion

Cells are equi-angular cube-sphere cells. Their area comes from the solid angle of the
mapping, and their neighbours across cube edges from a precomputed edge table.
Amplification blends the lattice heights around each cell centre and adds noise
detail. Then the sea level is fitted to the land target (area-weighted).

Each erosion pass works like this:

- The ocean is defined as the seas larger than 1/1000 of the planet. Smaller basins
  below sea level are dry depressions.
- A priority-flood from the ocean gives receivers and a stack order.
- Drainage area accumulates downstream.
- Implicit stream power (Braun and Willett) grades rivers to their receivers, or to a
  lake's surface where they enter a lake.
- Lakes silt up by 40 % of their depth.
- Hillslopes diffuse.

The final routing records the lakes (1–2 % of land, as on Earth).

### Phases

`zb_geology_advance` does one bounded piece of work: a tectonic step, one face of
amplification, or one erosion pass. That keeps cancellation and progress simple for the
creation job. The geology object is single-threaded; world creation runs it on one
worker.

## Risks / Trade-offs

- [Semi-Lagrangian advection smears plate boundaries at coarse resolutions] → crust
  conservation rules and margin growth; tests at 3,000–5,000 points and a corpus of seeds.
- [The default macro grid needs about 60 MB during erosion] → cells are released after
  creation once the world bundle stores them; mobile uses a coarser grid.
- [Lattice-scale features (35 km) are coarse for the region layer] → amplification
  adds detail at the macro scale; the region and chunk layers add their own.
- [Tuning constants shape every world] → the golden hash pins them; changing them is a
  generator version change, and saved worlds keep their bundle.
