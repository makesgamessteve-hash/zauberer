# Proposal: World Persistence

## Why

Concept §7 has a world that remembers: what the heirs cut, built and broke stays that
way. The plan (§1.9) keeps a save as the world bundle plus **deltas**, so a chunk that
is generated again comes back as the players left it.

M2 can create a world and fly in it, but closing the game loses everything except the
world's name: the time of day, where the carpet was, and any change to the land. This
change saves and loads a game and lays down the delta store that later edits
(digging, building, spells) and co-op (M9) share.

## What Changes

- **Deltas** (`world/delta`):
  - Edits per chunk: a removed plant (stage 3) and a painted surface material
    (stage 2) for now. A second edit of the same plant or cell replaces the first.
  - The world applies a chunk's edits whenever the stream finishes a stage of it, so
    unloaded and regenerated chunks keep them.
  - Region files of 16 × 16 chunks: a versioned header, a checksum and the
    compressed edits. Damaged and newer files are refused with a message.
- **Saves** (`saves/<name>/` in the user folder):
  - `world.lua`: format, kind (created or noise world), seed, radius, generator
    version and world time.
  - `bundle.zbw`: the created world, written once.
  - `player.lua`: a table the scripts keep (carpet position and heading).
  - `regions/<face>.<rx>.<ry>.bin`: the edits.
  - Every file is written atomically (temp file, then rename).
  - Migration: a world kept the M2 way (`worlds/<name>.zbw`, format 0) moves into a
    save when it is loaded. A repository fixture of a format-1 save must keep loading.
- **Lua**:
  - `world.save_game(name, player?)` and `world.load_game(name)` return the player
    table or an error; `world.saves()` lists them.
  - `world.remove_plants(pos, radius)`, `world.set_surface(pos, material)` and
    `world.ground_surface(pos)` edit and read the world.
  - The game saves on entering a new world, every five minutes of flight and on quit,
    and continues the last game on start: the same world, time, carpet and edits.

## Capabilities

### New Capabilities

- `world-persistence`: deltas per chunk, region files, saves of a game and their
  migration.

### Modified Capabilities

- `world-creation`: created worlds are stored inside their save (`saves/<name>/`)
  instead of `worlds/<name>.zbw`.

## Impact

- New:
  - `src/world/delta.{h,c}` and `src/script/script_save.c`;
  - tests `test_delta`, scenarios `persistence_save` and `persistence_load`, the
    fixture `tests/fixtures/saves/v1/`.
- Changed:
  - the stream (a hook when a chunk arrives at a stage, editable chunk data);
  - the world (owns the deltas, applies them, edits from scripts);
  - world creation (bundle path, own-world description) and the engine (the noise
    world as a playable world);
  - `main.lua` (save, autosave, continue).
- Platforms: on the web the user folder is IndexedDB (IDBFS), which the save module
  already syncs after writes. Region files are small (a few hundred bytes per region
  with edits).
- Co-op: the host owns the saves; clients receive the same edits as messages
  (`add-coop-sessions`, M9).
