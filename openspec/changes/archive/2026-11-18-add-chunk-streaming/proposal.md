# Proposal: Chunk Streaming

## Why

Milestone **M1 (First Flight)** asks for ten minutes of uninterrupted flight in any
direction. Terrain chunks (add-terrain-chunks) can be generated, but something must
decide which chunks exist at which stage, generate them in time ahead of a fast carpet,
keep the main thread smooth and stay within each platform's memory.

## What Changes

- A **chunk stream** (`world/stream`) that keeps chunks in rings around up to four focus
  points: stage 2 in the near ring and stage 0 in the far ring, whose radius grows with
  altitude.
- A **priority order** by distance to the focus position predicted from its velocity,
  with chunks behind the view direction later.
- **Budgets**: a limit on generation jobs in flight, a main-thread time budget when jobs
  run inline (single-threaded web build), and a memory cap enforced by evicting the least
  recently wanted chunks outside the rings.
- **Hysteresis**: a chunk keeps its stage until it is 25 % beyond its ring, so flying
  along a ring boundary does not regenerate chunks.
- **Cancellation** of queued jobs whose chunks left the rings.
- **Platform profiles** for desktop, web and mobile web.
- A **world object** (`world/world`) owning the planet, terrain and stream; the engine
  creates it from the run's seed and updates it every frame; Lua sets focus points
  (`world.set_focus`, `world.clear_focus`), reads the ground height from loaded chunks
  (`world.ground_height`) and the streaming statistics (`world.stream_stats`).

## Capabilities

### New Capabilities

- `chunk-streaming`: rings, priorities, budgets, hysteresis, cancellation, eviction,
  platform profiles, focus points and their scripting access.

### Modified Capabilities

None.

## Impact

- `src/world/` gains `stream` and `world`; `src/app` creates the world and updates it
  each frame; `script` uses the engine's world (or its own in unit tests).
- Platforms: desktop uses worker threads; the threaded web build uses web workers; the
  single-threaded web build generates inline within the time budget. The profiles differ
  in radii, caps and job counts (see design).
- Co-op: streaming is local. Each peer streams around its own players' focus points; the
  host additionally keeps chunks around every player it simulates (up to four focus
  points). Chunk content does not depend on streaming order.
