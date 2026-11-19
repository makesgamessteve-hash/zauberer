# Proposal: The modding SDK

## Why

M11's exit includes a documented example mod that works. Mods are packages like the base game
(concept 9); players need to turn them off, and modders need a small, stable API into the game.

## What Changes

- The game's API for mods, the global `zauberer` (`mods/api.lua`): add items, HUD lines and work every
  tick, give items, tell the player, know where the player is; a failing function is turned off.
- The example mod `examples/lantern-moths/` with its tests, German text and guide
  (docs/modding/example-mod.md, game-api.md); it ships in the release archive under examples/.
- Packages turned off by the player (menu and `engine.set_package_enabled`), kept in config.lua.

## Impact

- New capability `modding-sdk`. VFS: packages turned off before mounting.
