# Design: Terrain Chunks

## Context

Concept §4.3–4.7 describes a generation pyramid (planet, region, chunk, detail) with
chunks advancing through stages 0–5. M1 implements the chunk layer's first three stages
on top of a placeholder planet layer; M2 swaps the placeholder for deep-time geology
without changing the chunk interfaces.

## Goals / Non-Goals

**Goals:** varied, plausible terrain from the air; bit-identical output on every
platform; seamless chunk borders, including across cube faces; generation fast enough
for a fast carpet (budget below); a memory footprint that lets the far ring stay at
stage 0.

**Non-Goals:** geology, climate, rivers, biomes and vegetation (M2), caves and overhangs
(sparse voxels, M2+), streaming and caching (add-chunk-streaming), meshes
(add-retro-renderer).

## Decisions

### 1. Noise

Improved Perlin gradient noise in 3D, evaluated at planet-centred positions divided by
a wavelength. 3D noise on the sphere has no seams or polar distortion, unlike 2D noise
per face. Lattice gradients come from a multiplicative hash of the corner coordinates
and the seed (no permutation tables, so any seed works and nothing needs storing). Only
`+ − ×` and `floor` are used, so the output is bit-identical everywhere. Simplex noise
would need fewer corners but is harder to keep bit-exact in the skew step; the measured
cost (below) does not need it.

### 2. Continent model v1

Heights in metres above sea level:

- a continent field: 9 octaves of noise at a frequency relative to the radius (so any
  planet size gets a few continents); the sea-level threshold is the quantile of 2,048
  Fibonacci-sphere samples that leaves 35 % above it,
- oceans down to −4,000 m and lowlands rising to 600 m inland,
- hills: 6 octaves from a 6 km wavelength, ±120 m, reaching slightly below sea level so
  coasts get islands and bays,
- mountains: a 7-octave ridged multifractal (50 km wavelength) squared, up to 3,500 m, in
  belts given by a 700 km mask and kept away from coasts,
- detail: 3 octaves from a 40 m wavelength, ±2.5 m.

### 3. Stages and data layout

| Stage | Data | Size |
| --- | --- | --- |
| 0 Coarse | 19 × 19 macro heights (16 m apart, with a one-sample border for interpolation) and 17 × 17 materials | 1.7 KB inside the chunk struct |
| 1 Terrain | 257 × 257 float heights | 258 KB |
| 2 Surface | 256 × 256 material bytes | 64 KB |

Stage 1 interpolates the macro shape from the stage 0 grid with Catmull-Rom splines (the
macro octaves have wavelengths of at least 94 m, so 16 m samples resolve them) and adds
detail noise evaluated on a 2 m grid and refined bilinearly (finest detail wavelength
10 m, refinement error < 5 cm). This cuts the noise evaluations per chunk from about
1.7 million to 50,000.

Edge samples are computed from the same planet positions on both sides of a border
(the equi-angular grid lines continue across face seams), so neighbours agree on their
shared edge exactly within a face and to rounding (< 1 µm) across faces.

### 4. Surface materials

A pure rule of height, slope (from the heightfield), latitude and a per-cell jitter
that dithers material borders (which suits the retro look): deep seabed → mud;
shore and shallows → sand; steeper than 38° → rock; above the snow line (3,200 m at the
equator, falling towards the poles) → snow; polar lowland → ice; 30–38° → gravel;
otherwise soil.

### 5. Budgets

Measured on the development container (one core, release build): stage 0 0.15 ms, stage 1
2.2 ms, stage 2 0.6 ms per chunk. The concept's budget for stages 1–3 is 8 ms on a
mid-range desktop; stages 1–2 must stay at or below 5 ms to leave room for ecology
(stage 3, M2). The CI perf test asserts the 5 ms median on the Linux release job.

### 6. Placement and threading

`world/terrain` depends on `core` and `world/planet`. A `zb_terrain` is immutable after
`zb_terrain_init`, so any number of jobs can generate different chunks at once; each
chunk's data is owned by its caller. Scripts reach the terrain through the `world` table.

## Risks / Trade-offs

- [The v1 continents look generic] → They are placeholders; M2 replaces the macro height
  with geology while keeping stages, layouts and seams.
- [Float heights lose precision far above sea level] → 0.5 mm at 5,000 m, well below the
  1 m grid.
