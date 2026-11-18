# Design: Planet Coordinates

## Context

The planet is a sphere of radius R (default 2,000 km). Gameplay happens at metre scale
anywhere on it. Single-precision floats keep about 7 significant digits, so a raw
Cartesian coordinate at 2,000 km has a resolution of ~0.25 m, which is too coarse for
physics and rendering. Doubles are precise enough but slow and unavailable as GPU
attributes on the target hardware. The concept (§4.2) fixes the representation: a
cube-sphere with chunks of 256 m and a floating origin.

## Goals / Non-Goals

**Goals:** exact, reversible addressing; neighbours across the twelve cube edges and
eight corners; precise camera-relative coordinates anywhere; deterministic math for
world generation.

**Non-Goals:** terrain heights (add-terrain-chunks), streaming (add-chunk-streaming),
physics on the curved surface (later; gameplay treats the active area as locally flat).

## Decisions

### 1. Cube-sphere with the equi-angular mapping

Face coordinates `(u, v) ∈ [-1, 1]²` map to the cube point `n + tan(πu/4)·U + tan(πv/4)·V`
(face normal `n`, face axes `U`, `V` with `U × V = n`), normalised onto the sphere. This is
the equi-angular ("tangent") cube map: the largest-to-smallest chunk area ratio is √2,
compared with 5.2 for the plain gnomonic cube and 2.1 for S2's quadratic mapping, at the
cost of one `tan`/`atan` per conversion. Each face has `N = round(R·π/2 / 256)` chunks
per edge (12,272 for the default radius), so chunks are 256 m wide at face centres and
shrink to about 181 m near the cube corners. Alternatives considered: the COBE
quadrilateralised cube (ratio ≈ 1) needs an iterative or high-order inverse; HEALPix
breaks the rectangular chunk grid.

Face order and axes are fixed (+X, −X, +Y, −Y, +Z, −Z) and part of the save format.
Points exactly on an edge belong to the face with the largest absolute component, ties
resolved in face order, so every direction has exactly one owner.

### 2. Positions and keys

A position is `(face, cx, cy, lx, ly, alt)`: integer chunk indices, a local offset in
nominal metres inside the chunk (0 to 256, where 256 occurs only on a face's far edge;
scaled linearly in face space) and
the altitude in true metres above the sea-level sphere. Local offsets are floats
(resolution < 0.03 mm); altitudes are floats too (resolution ≤ 1 mm below 16 km). Keys pack `face` (3 bits), `cx` and `cy` (30 bits each) into a
`uint64_t`, which supports Earth-size planets (N ≈ 39,000) with room to spare.

Neighbours are found geometrically: the centre of the requested neighbour is computed in
the face's extended `(u, v)` plane, turned into a direction and re-projected, which
handles all edge orientations without a hand-written table. A corner chunk has 7
distinct neighbours instead of 8.

### 3. Floating origin

An origin holds an anchor chunk, the anchor's planet-centred position in doubles and the
anchor's tangent frame (geographic east, north and up; +Z is the north pole, and exactly at
a pole east is +Y). Local axes are x = east, y = up, z = south (right-handed, y up). `to_local(p)` computes `p − anchor` in double
precision and rotates it into the anchor frame before rounding to float, so precision
depends only on the distance to the anchor, never on where the anchor is on the planet.
The frame bends with the planet: distant points sink below the local horizon exactly as
on the real sphere, which is the "curvature" the renderer shows. Re-anchoring moves the
anchor to the camera's chunk and returns the rigid transform between old and new frames
so systems can move their float positions. Callers re-anchor when the camera is more than
a threshold (default 1,024 m) from the anchor.

### 4. Deterministic math

`core/dmath` implements the transcendental functions with explicit range reduction and
fixed polynomial evaluation (coefficients from the well-known fdlibm minimax kernels),
using only IEEE-754 operations that are exact or correctly rounded on all targets
(`+ − × ÷`, `sqrt`, `floor`, `frexp`, `ldexp`). Determinism follows from the build flags
(`-ffp-contract=off`, `/fp:precise`, no fast-math) and the fixed evaluation order; the
functions are not required to match the platform's libm bit for bit, only themselves.
Accuracy targets: ≤ 2 ulp for `sin`, `cos`, `atan`, `exp`, `log`; `pow` is computed as
`exp(y·log x)` with a relative error ≤ 1e-13 for the ranges used by world generation.
Golden values (a hash over a fixed sample) are checked in unit tests on every CI
platform.

### 5. Module placement

`src/world/` depends only on `core` (dependency rules §1.1). `script` exposes a thin
`world` table. Nothing else depends on world yet.

## Risks / Trade-offs

- [Chunk sizes vary by √2] → Generators work in true metres from the conversions, so the
  variation is invisible; streaming budgets use chunk counts that already allow for it.
- [Custom math is slower than libm] → It is used in generation, not per-pixel or per-frame
  hot paths; benchmarks keep it within 3× of libm.
