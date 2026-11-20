# Proposal: Starting the game over

## Why

Players testing the game want to go back to its first start without deleting files by hand
(in the browser they live in IndexedDB, out of reach).

## What Changes

- A reset key (F9, remappable): pressed once, the HUD asks to press it again within five seconds;
  pressed again, the game starts over as at its first start.
- `engine.reset(keep)`: after the tick, the scripts end without their shutdown hooks (nothing is
  saved), sounds stop, every saved game (`saves/`, `worlds/`) is deleted, the save data keeps only
  the named keys (the player's settings), and the world, the simulation and the scripts are made
  anew in the same window. Bindings and packages (config.lua) are kept. Refused in a co-op
  session and in tests and scenarios, with the reason shown on the HUD.
- `zb_fs_remove_tree`, `zb_audio_stop_all`; the engine's start split into the world (`start_sim`)
  and the scripts (`start_scripts`).

## Impact

- Capability `world-persistence`: a new requirement. Engine, script API (`engine.reset`),
  base game (`ui/reset.lua`, the `reset` action, German text).
