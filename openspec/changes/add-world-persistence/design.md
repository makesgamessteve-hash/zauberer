# Design: World Persistence

## Context

Terrain, water and plants are pure functions of the world (bundle or noise seed) and
a chunk's address, generated in stages by the stream. The user folder already holds
`save.lua` (the scripts' key-value store) and, since world creation, a world bundle
per created world. Writes go through the platform file system, which writes atomically
and syncs IDBFS on the web.

## Goals / Non-Goals

**Goals:**

- Close the game and continue where you were, in the same world.
- Edits survive unloading, regeneration and restarts, and cost nothing where nobody
  edited.
- Every file versioned and checked; older saves migrate; a fixture guards the format.

**Non-Goals:**

- Terrain height edits (digging, `add-terrain-editing`), structures, items, entities
  and chunk simulation timestamps: each later change adds an edit kind.
- `global.bin` and `script.lua` from the plan's layout: there is no macro simulation
  or quest state yet. `player.lua` holds what the scripts keep.
- Several save slots in the UI, cloud saves, exporting a world.

## Decisions

### Edits, not snapshots

A save stores only edits per chunk. A chunk is regenerated from the world and then its
edits are applied. Each edit is 12 bytes: kind, value, cell or plant index. The store
is a hash map from chunk to a small sorted array. An edit of the same target replaces
the earlier one, so painting a cell many times keeps one edit.

Edits address what generation already produced: a plant by its index in the chunk's
plant list (stable, since generation is deterministic and the generator version is in
`world.lua`), a cell by its 1 m surface coordinates. A removed plant keeps its slot
with layer `ZB_PLANT_REMOVED`, so indices do not shift.

### Applying when a stage arrives

The stream calls the world when a chunk reaches a new stage. The world applies the
chunk's edits to whatever stages it has; edits for a stage not yet generated wait.
Edits from scripts are applied to the loaded chunk at once (through
`zb_stream_edit`, which bumps the chunk's version so the renderer rebuilds it) and
recorded in the store.

### Region files

Chunks are grouped into regions of 16 × 16 per face, one file each, written only when
they have edits. A file is:

- the magic `ZBRD`, a format version, face and region coordinates;
- the raw length, a 64-bit hash of the raw data and the compressed length;
- the edits, compressed with the deflate codec the world bundle already uses.

The plan left the codec open between LZ4 and others for this change. Deflate is
already in the engine, is fast enough for files this small, and avoids a new
dependency, so it is the choice. Reading checks the magic, the version, the lengths
and the hash, and refuses with a message the player can read.

### A save is a folder

`saves/<name>/` holds `world.lua`, the bundle, `player.lua` and `regions/`. `world.lua`
is plain Lua data so it can be read without the engine. Saving writes `world.lua`,
the bundle when missing, `player.lua` and every region with edits (all of them, so a
save under a new name is complete), then marks the store clean.

Loading reads `world.lua` and refuses a newer format or generator, or a noise save
whose seed is not the running noise world's. It then adopts the world (bundle or
noise), sets the time, and reads every region file. A damaged region is skipped with
a warning rather than losing the whole game.

### Migration

Format 0 is the M2 layout, `worlds/<name>.zbw`. When no `saves/<name>/` exists but
that file does, the world is loaded from it and immediately saved in the new layout;
the old file is left in place. A fixture save of format 1 lives in
`tests/fixtures/saves/v1/` and a headless test loads it, so a later format change has
to keep it loading.

### The game continues

`main.lua` keeps the current save name in `save.lua`, loads that game on start and
restores the carpet from the player table. It saves on entering a new world, every
five minutes of flight and on quit.

## Risks / Trade-offs

- Plant indices depend on the generator. A newer generator version refuses older
  saves for now; a migration that remaps edits can come when the generator changes.
- Saving writes every region with edits, not just dirty ones, to keep "save as" simple.
  Fine while edits are few; switch to dirty regions plus copying when they grow.
