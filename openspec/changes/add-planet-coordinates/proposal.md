# Proposal: Planet Coordinates

## Why

Milestone **M1 (First Flight)** flies over a round planet of up to Earth size. Positions
on a 2,000–6,371 km sphere cannot be stored in single-precision floats, and every later
system (terrain chunks, streaming, flight, rendering, netcode, saves) needs one shared way
to address a place on the planet, find neighbouring chunks across cube-face seams and turn
planet positions into precise camera-relative coordinates. World generation must also
produce identical results on every platform, which requires transcendental functions that
do not depend on the platform's math library.

## What Changes

- A **cube-sphere planet model**: six faces with an equi-angular (area-balancing)
  mapping, each subdivided into chunks of nominally 256 m; a configurable radius from
  50 km to 6,371 km (default 2,000 km).
- **Planet positions** as *(face, chunk x, chunk y, local x/y in metres, altitude)*, with
  exact conversions to and from planet-centred Cartesian coordinates, stable 64-bit chunk
  keys, chunk neighbours across face seams, the local "up" and tangent frame, and
  great-circle distances.
- A **floating origin** that maps planet positions to single-precision coordinates
  relative to an anchor near the camera, and re-anchors without visible jumps.
- **Deterministic math** (`sin`, `cos`, `tan`, `atan`, `atan2`, `asin`, `acos`, `exp`,
  `log`, `pow`) implemented in the engine core, bit-identical on all platforms, used by
  the planet model and all later world generation.
- A small **Lua API** (`world.planet()`, position conversions, distances) for scripts and
  scenario tests.

## Capabilities

### New Capabilities

- `planet-coordinates`: cube-sphere addressing, conversions, neighbours, frames, distances
  and the floating origin.
- `deterministic-math`: platform-independent transcendental functions with golden values.

### Modified Capabilities

None.

## Impact

- New engine module `src/world/` (depends on `core` only) and `src/core/dmath.*`.
- `script` gains a `world` table; the API reference is regenerated.
- Platforms: no differences. Desktop, web, mobile web and WebXR use the same code; the
  golden values are checked on the Linux, Windows and WebAssembly (web-node) CI jobs.
- Co-op: positions are the common language of replication; a position serialises to its
  chunk key plus three floats, identical on every platform.
