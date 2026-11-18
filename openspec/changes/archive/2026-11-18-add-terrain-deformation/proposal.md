# Proposal: Terrain deformation

## Why

Concept 5.10 wants the ground itself to change: an Earth spell raises a wall or digs a
pit, an explosion leaves a crater, and a breached natural dam lets a lake run into the
valley below. Until now the heights came only from the generator. The M3 exit demo
needs this: breach a natural dam and flood a valley.

## What Changes

- **Heights as edits**: a new edit kind (`ZB_EDIT_HEIGHT`) keeps a 1 m height sample's
  new height with the chunk's other edits, so that saves, regenerated chunks and peers
  see the changed ground. Invalid samples and heights are refused. Applying it reshapes
  the chunk and its height range, and the chunk's data version advances so that views
  rebuild their meshes.
- **Craters and mounds** (`zb_world_deform`): lowers or raises the ground within a
  radius of a place in a smooth bowl or mound, deepest at the centre and untouched
  beyond the rim. It works across chunk borders, keeping the seams closed. Plants
  inside the bowl are dug up or buried.
- **The active tier follows**: surface water and fire re-read the ground of changed
  chunks (`zb_tier_ground_changed`), so water runs into a new crater or through a new
  breach.
- **Lua**: `world.deform(pos, radius, amount)` with radius 0.5 to 64 m and amount -30
  to 30 m; it returns the chunks changed.

## Capabilities

### New Capabilities

- `terrain-deformation`: changing the ground's heights, and what follows from it.

### Modified Capabilities

- `world-persistence`: the edits per chunk also keep heights.

## Impact

- `world/delta`, `world/world`, `sim/water`, `sim/fire`, `sim/tier`,
  `script/script_world`.
- Region files keep their format: a height edit is one more kind of edit record.
- Not covered: rivers and lakes of the generated world do not move when the ground
  under them changes. The active tier's surface water does.
