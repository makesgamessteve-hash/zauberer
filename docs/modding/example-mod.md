<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# The example mod: lantern moths

`examples/lantern-moths/` is a complete small mod. At night a few lantern moths gather
round the heir; every twenty seconds one leaves moth-wing dust, which goes into the pack.
It shows each part of a package:

| File | Shows |
| --- | --- |
| `package.lua` | the manifest: id, version, scripting API, licenses, `requires = { base = ">=0.1" }` |
| `credits.lua` | the credits list (empty: the mod has no asset files of its own) |
| `scripts/main.lua` | the entry script: an item (`zauberer.add_item`), work every tick (`zauberer.on_tick`), a HUD line (`zauberer.add_hud`), a message, drawing with `engine.on("frame")` and the base game's sprite, state in `engine.on("hash")` |
| `scripts/lantern_moths/moths.lua` | a module (`require("lantern_moths.moths")`) kept free of engine calls so it is easy to test |
| `locale/de/lantern-moths.lua` | German text, merged with the base game's |
| `tests/test_moths.lua` | Lua unit tests |

## Trying it

Copy the folder into the `mods/` folder next to the game (or `mods/` in your user data)
and start the game; the moths appear at night. Escape, Packages turns it off again.

```sh
mkdir -p /tmp/zb/mods && cp -r examples/lantern-moths /tmp/zb/mods/
zauberer --user-dir /tmp/zb                                                   # play
zauberer --user-dir /tmp/zb --headless --test examples/lantern-moths/tests     # its tests
python3 tools/check_credits.py --package examples/lantern-moths
```

The engine tests run it too (`tests/scenarios/example_mod.lua`): the mod loads after the
base game, adds its item, gives dust at night, shows its line in English and German, and a
failing function of a mod is turned off while the game goes on.
