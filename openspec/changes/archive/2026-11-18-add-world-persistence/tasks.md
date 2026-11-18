# Tasks

## 1. Deltas

- [x] 1.1 `world/delta`: edits per chunk, dirty regions, applying to a chunk's stages; verify "world-persistence: Edits are kept per chunk"
- [x] 1.2 Region files with header, checksum and deflate; refusals; verify "Region files"
- [x] 1.3 Stream hook when a chunk arrives at a stage and editable chunk data; the world owns the deltas and applies them; foliage and Lua skip removed plants

## 2. Saves

- [x] 2.1 `world.save_game`, `world.load_game`, `world.saves`; atomic writes; created and noise worlds; verify "Save and continue"
- [x] 2.2 `world.remove_plants`, `world.set_surface`, `world.ground_surface`
- [x] 2.3 Created worlds stored in `saves/<name>/`; verify "world-creation: Saved worlds come back"
- [x] 2.4 Migration of `worlds/<name>.zbw`; verify "Worlds from before saves move into a save"
- [x] 2.5 Fixture `tests/fixtures/saves/v1/` and its test; verify "Saves of an earlier version still load"

## 3. Game

- [x] 3.1 `main.lua`: save on entering a world, autosave every five minutes of flight, save on quit, continue on start
- [x] 3.2 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
