<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Creating packages

Everything the game loads comes from **packages**: folders with a `package.lua`
manifest. The base game is the package `assets/base`; mods are packages too. The engine
looks for packages in three places and loads them in dependency order:

| Location | Used for |
| --- | --- |
| `assets/` next to the executable (or `--assets <dir>`) | the base game |
| `mods/` next to the executable | mods shipped with an installation |
| `<user data>/mods/` | mods a player installs (`--user-dir` moves the user data) |

Later packages override files of earlier ones: a mod that contains `sprites/mote.png`
replaces the base game's mote for everyone who loads files through the virtual file
system. Paths are relative to the package root, use `/`, and may not leave it (`..`).

## Layout

```text
mods/runes/
  package.lua          manifest (required)
  credits.lua          author, source and license of every asset file
  scripts/main.lua     entry script, run once at start
  scripts/runes/*.lua  modules for require("runes.something")
  data/input.lua       default bindings for new or changed actions
  textures/ models/ sprites/ fonts/ sounds/ music/ shaders/
```

## `package.lua`

The manifest is a Lua table of literals only (no code runs while it is read):

```lua
return {
  id = "runes",                    -- lower-case letters, digits, '_' and '-'
  name = "Rune stones",            -- shown in menus and logs
  version = "1.2.0",
  api = "1.0",                     -- scripting API the package was written for
  license = { code = "MIT", assets = "CC-BY-4.0" },
  requires = { base = ">=0.1" },   -- package id -> version constraint
  optional = { weather = ">=2.0" },-- loaded first when present
}
```

A package is **disabled**, with the reason in the log and in `engine.packages()`, when its
manifest is invalid, its id is taken, it needs a newer scripting API (major version
different, or minor version higher than the engine's), a required package is missing or
too old, or it is part of a dependency cycle.

## Scripts

`scripts/main.lua` runs once in load order. Each package has its own environment: globals
you define stay in your package (use `_G` deliberately to share). Register hooks with
`engine.on`:

```lua
local state = { glow = 0 }

engine.on("init", function()
  state.rune = render.model("models/rune.glb")
end)

engine.on("tick", function(dt)          -- 60 times per simulated second
  state.glow = (state.glow + dt) % 1
end)

engine.on("frame", function(alpha)      -- every rendered frame
  render.draw_model(state.rune, { position = { 2, 0.5, 0 }, rotation = { 0, state.glow * 360, 0 } })
end)

engine.on("hash", function()            -- simulation state for replays and determinism checks
  return state.glow
end)
```

Other events: `event(name, args)` (`resize`, `input_mode`, `audio_unlocked`), `shutdown`
(reverse load order) and, in development builds, `reload(preserved)` after you edit a
script (keep state across reloads with `engine.preserve(tbl)`).

Rules that keep the game deterministic and safe:

- There is no `io`, `os`, `dofile`, `loadfile`, `string.dump` or binary chunk loading.
  Read package files with `vfs.read`/`vfs.load`, store player data with `save`.
- Use `engine.time()` and `engine.tick()`, never wall-clock time; `math.random` is seeded
  per run, so the same seed and input give the same game.
- A hook that raises an error is disabled and reported; the rest of the game keeps running.
  Development builds stop runaway hooks after an instruction budget.

The full API is in the [Lua API reference](../api/README.md); `docs/api/zauberer.d.lua`
gives editors with the Lua language server completion and type checks.

## Input

Actions have a name and a kind (`button`, `axis1`, `axis2`). Packages declare default
bindings in `data/input.lua`; a later package replaces an action's bindings, and players'
own bindings (saved in `config.lua` in the user data) win over both:

```lua
return {
  cast = { type = "button", bind = { "key:r", "gamepad:y", "touch:button:cast" } },
}
```

## Credits and licenses

Every asset file (images, models, sounds, music, fonts) needs an entry in `credits.lua`;
the credits screen shows them under the package's name:

```lua
return {
  { file = "models/rune.glb", author = "Ada", source = "made in Blender", license = "CC-BY-4.0" },
  { file = "sounds/hum.ogg", author = "Freesound user x", source = "https://freesound.org/…", license = "CC0-1.0" },
}
```

`python3 tools/check_credits.py --package mods/runes` reports files without credits,
entries for missing files and licenses that are not allowed.

## Testing a package

```sh
zauberer --user-dir /tmp/zb --headless --test mods/runes/tests          # Lua tests (test_*.lua)
zauberer --user-dir /tmp/zb --headless --scenario mods/runes/tests/demo_scenario.lua
```

Lua tests use `test.case`, `test.expect`, `test.expect_eq`, `test.expect_near` and
`test.expect_error`; `require` loads your package's modules. Scenarios step the
simulation (`step(n)`), inject input (`input.inject`) and check results with `expect`; the
process exits with a non-zero code when an expectation fails.
