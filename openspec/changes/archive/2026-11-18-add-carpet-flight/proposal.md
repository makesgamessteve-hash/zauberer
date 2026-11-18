# Proposal: Carpet Flight

## Why

Milestone **M1 (First Flight)**: "fly endlessly over streamed terrain". The planet,
terrain, streaming and the retro renderer exist; the player needs a carpet to fly, a
camera to follow it and the minimal heads-up information to know where they are.

## What Changes

- A **flight model** in Lua (`assets/base/scripts/flight/carpet.lua`): throttle,
  boost and brake, turning that slows at speed, banking, climbing and diving, ground
  and sea clearance against the terrain height, a ceiling. Positions move over the
  sphere with `world.move`, which carries the heading along great circles (also across
  the poles).
- **Deterministic script math**: the sandbox's `math.sin/cos/tan/asin/acos/atan/exp/log`
  and the `^` operator use the engine's deterministic math, so flight (and every later
  script simulation) is bit-identical on every platform, in replays and between co-op
  peers.
- A **chase camera** with smoothing and orbit look, a **spawn** on land, and a
  **heads-up display** (speed, heading, altitude, distance flown, controls).
- The base package runs the flight by default; the M0 demo remains as a **showcase**
  mode for its scenarios and goldens.

## Capabilities

### New Capabilities

- `carpet-flight`: the flight model, deterministic script math, camera, spawn, HUD and
  the flight's co-op and replay behaviour.

### Modified Capabilities

None.

## Impact

- `assets/base/scripts/` gains `flight/`; `main.lua` switches between modes.
- `src/script/` replaces the transcendental math functions; `cmake/ZbDependencies.cmake`
  force-includes `src/core/lua_numpow.h` into the Lua sources; `world.move` is added.
- Platforms: no differences in the flight. The replay fixture (`tests/replays/demo.zbrec`)
  now records flight and its hashes are compared across Linux, Windows and WebAssembly.
- Co-op: each heir flies their own carpet. The flight model is pure and deterministic, so
  the host can simulate it authoritatively and clients can predict their own carpet with
  the same code (add-netcode-foundation replicates the state that `carpet.snapshot`
  returns).
