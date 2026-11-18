# Design: Chunk Streaming

## Context

Concept §4.7 defines rings (active, near, far, horizon), a priority queue by distance,
view direction and predicted position, budgets for main-thread time, workers and memory,
an LRU cache of distant unmodified chunks, and cancellation. M1 has chunk stages 0–2, so
it streams two rings: near (stage 2) and far (stage 0). Stages 3–5 and the active ring
join in M2/M3 with the same machinery.

## Goals / Non-Goals

**Goals:** no ungenerated ground under a carpet flying at up to 150 m/s; no frame spikes
from streaming bookkeeping; memory within each platform's cap; deterministic content
regardless of streaming order.

**Non-Goals:** meshes and draw lists (add-retro-renderer), saving modified chunks (later
milestones: modified chunks will be pinned instead of evicted).

## Decisions

### 1. Rings by spherical caps, scanned per face

For each focus, the chunks within a radius form a spherical cap. Instead of a flood fill
over neighbours, each cube face that the cap can touch is scanned over the bounding box
of the cap in face coordinates (from 32 boundary samples, padded by a chunk), testing
each chunk centre against precomputed cosines of the ring radii (plus half the largest
chunk diagonal). Tangents of the scanned columns are cached. This visits every chunk
exactly once (each chunk belongs to one face) and costs about 0.8 ms for the 7,600
chunks of the desktop profile, against several milliseconds for a flood fill with hash
lookups per neighbour. Rings are recomputed only when a focus moved more than 32 m, its
far radius changed, or focus points were added or removed.

### 2. Want, keep and target

A refresh computes per chunk the stage it *wants* (2 inside the near ring, 0 inside the
far ring) and the stage it may *keep* (the same rings enlarged by the hysteresis factor,
1.25). The target is `max(want, min(current, keep))`: chunks are generated up to `want`
and only demoted once outside `keep`. Demotion frees the stage 1–2 arrays and keeps the
coarse grid. Chunks outside every keep ring are *cached*: they stay loaded until memory
needs them.

### 3. Priority and jobs

Candidates (target above current stage, no job) are sorted by the chord distance to the
nearest focus's predicted position (`lookahead` = 2 s of velocity), multiplied by 1.5
when the chunk lies behind the view direction, and the best ones fill the free job slots
(`max_jobs`). A job generates straight to the target stage. Jobs write into a copy of the
chunk record (`work`) that shares the existing arrays; the main thread swaps it in when
the job is done, so readers never see half-written data and a chunk being upgraded stays
visible at its old stage. Queued jobs of chunks that left the rings are cancelled;
running ones finish and their chunks become cached.

With an inline job queue (single-threaded web, tests) the update runs jobs one by one
until the time budget is used (checked after each job, so at most one job overruns).

### 4. Memory

Chunk bytes (record plus arrays) are counted. After demotions, while the total exceeds
the cap or more than `2 × wanted + 1024` chunks are loaded, cached chunks are evicted
least recently wanted first. Chunks inside a ring are never evicted; the profiles size
the cap well above their ring contents (desktop rings ≈ 7,600 stage-0 and ≈ 150 stage-2
chunks ≈ 63 MB).

### 5. Profiles

| | Desktop | Web | Mobile web |
| --- | --- | --- | --- |
| Near ring (stage 2) | 1,536 m | 1,024 m | 640 m |
| Far ring at ground level (stage 0) | 12 km | 8 km | 5 km |
| Far ring growth | +4 m per metre of altitude | same | same |
| Far ring maximum | 24 km | 16 km | 10 km |
| Memory cap | 512 MB | 256 MB | 128 MB |
| Jobs in flight | 8 | 4 | 2 |
| Inline budget per frame | 4 ms | 4 ms | 3 ms |

Beyond the far ring the renderer shows fog and sky (the horizon ring with planet-data
meshes needs the planet layer of M2).

### 6. The world object

`zb_world` owns the planet, the terrain and the stream plus up to four focus points.
The engine creates it after the job system (radius 2,000 km, seed from `--seed`, profile
by platform and input mode) and calls `zb_world_update` once per frame, also in headless
and scenario runs, so streaming is testable without a window. Scripts set focus points
in planet positions with a local velocity and view direction.

## Risks / Trade-offs

- [Streaming in the frame loop ties chunk availability to frame rate] → Jobs run on
  workers; only bookkeeping happens per frame. Headless runs step frames too.
- [Eviction iterates all chunks] → Only when over a limit; a sort of up to ~10,000 entries
  takes well under a millisecond.
