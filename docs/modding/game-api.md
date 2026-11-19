<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# The game's API for mods

The engine's Lua API ([reference](../api/README.md)) is what every package can call. The
base game adds an API of its own for mods, the global table `zauberer` (version
`zauberer.version`, "1.0"). It is defined while the base game's `scripts/main.lua` runs,
so it is there when a mod's `main.lua` runs (mods load after the packages they require).

| Function | What it does |
| --- | --- |
| `zauberer.add_item(def)` | Adds an item with the fields of `data/items.lua` (`id`, `name`, `kind`, `weight`, and optionally `kcal`, `water`, `price`, …). Returns true, or nil and what is wrong (an id may be taken once). |
| `zauberer.add_hud(fn)` | `fn(player)` returns lines for the HUD in English; they are translated with the packages' string tables. |
| `zauberer.on_tick(fn)` | `fn(dt, player)` runs every tick of play, after the game's own work. |
| `zauberer.say(text)` | A message for the player (shown for a few seconds). |
| `zauberer.give(id, count)` | Puts items in the player's pack; returns how many fit. |
| `zauberer.player()` | What a mod may know of the player: `{ pos, on_foot, night, time }`. |

A function a mod passes in that raises an error is turned off and reported in the log;
the game and the mod's other functions go on.

For everything else use the engine's API: `engine.on("frame", …)` to draw,
`engine.on("hash", …)` to make your state part of replays and determinism checks,
`world` and `sim` to read and change the world, `save` to keep the mod's data.
