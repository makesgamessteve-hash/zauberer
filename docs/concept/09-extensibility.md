# 9. Extensibility and Modding

## 9.1 Principles

- **Nothing is baked into the executable.** The binary contains the engine only. All
  game content (scripts, data, textures, models, sprites, sounds, music, shaders, fonts
  and translations) lives in folders next to the binary.
- **The base game is a package like any other.** Mods use the same format and the same
  APIs as the base game, with no privileged paths.
- **Data in Lua, behaviour in Lua, performance in C.** Lua defines *what exists* (data)
  and *how it behaves at a high level* (rules, AI policies, quests, UI). Bulk numeric work
  stays in the C core: fields, fluids, noise, meshing, rendering, physics, pathfinding and
  population updates.

## 9.2 On-disk layout (shipped game)

```
zauberer[.exe]                 engine binary (or index.html + zauberer.wasm/js on web)
assets/
  base/                        the base game package
    package.lua                manifest: id, version, engine API version, dependencies, license
    scripts/                   Lua game logic (entry point: scripts/main.lua)
    data/                      registries as Lua tables: materials, substances, reactions,
                               species, biomes, style families, culture and faith
                               templates, languages, weaves, recipes, rooms, loot, ...
    textures/  sprites/  models/  voxels/  sounds/  music/  shaders/  fonts/  locale/
    credits.lua                attribution for every asset: author, source, license
mods/
  <mod-id>/                    same structure as assets/base
user data (per OS: %APPDATA%, ~/.local/share, ~/Library/Application Support, IndexedDB on web)
  config.lua                   settings and key bindings
  saves/<world>/               world saves
  mods/                        optional per-user mods
```

## 9.3 Packages and load order

- **The manifest** (`package.lua`) declares:
  - `id` and `version` (semantic versioning),
  - `api` (the engine scripting API version it targets; incompatible majors are refused
    with a clear message),
  - `requires` and `optional` dependencies with version ranges,
  - `load_after` hints,
  - `license` (SPDX identifiers for code and assets),
  - `affects_worldgen` (true if it changes generation, which is recorded per world).
- **Resolution:** packages are sorted by dependencies (a stable topological order).
  Missing or incompatible dependencies disable the package with an error, and the game
  never crashes because of them.
- **The virtual file system** looks up a path such as `textures/grass.png` from the
  highest-priority package to the lowest, so mods can **override** individual assets.
- **Registries merge.** Data entries are keyed by `package-id:name`. Mods can **add**
  new entries, **override** entries completely, or **patch** fields of existing entries.
  Each patch is applied in load order and logged.
- **Per-world mod set.** A save records the packages and versions it was created with.
  Loading with a different set shows a compatibility report.
- **Co-op:** every player in a session needs the same package set. Joining compares the
  package lists and shows any mismatch. Packages are never transferred automatically,
  because scripts are code and the Lua sandbox is not a security boundary.

## 9.4 Lua scripting API (surface overview)

The API is versioned and documented, and generated reference docs are part of the build.

| Module | Purpose |
| --- | --- |
| `engine` | Version, platform, timing, logging, profiling markers |
| `vfs` | Read files from packages (no raw OS file access) |
| `registry` | Define, query, override and patch data entries |
| `world` | Query terrain, materials, biomes, fields (temperature, moisture, mana); request edits (deltas) |
| `sim` | Apply sources and sinks (heat, matter, force, growth, decay); spawn effects; query reactions |
| `entity` | Create entities, attach components, query in radius, send events |
| `ai` | Utility-AI considerations and actions for species and NPCs |
| `magic` | Weave composition and validation, casting, mana pools, the effect primitives that map verb × essence to simulation calls |
| `gen` | Hooks for structure grammars, building programs, furnishing rules, set-piece templates and name generators |
| `ui` | Immediate-mode widgets, layout, fonts, input focus, touch-aware components |
| `input` | Action map, bindings, gesture recognizer results, touch/XR state |
| `audio` | Play and stop sounds, music layers, 3D emitters |
| `save` | Persist and restore script state (serialisable tables) |
| `history` | Query the history database (cultures, languages, faiths, polities, figures, events, perspectives) for lore, dialogue and generation; add history rules in world-creation hooks |
| `story` | Beat graph, narrative-director queries, journal and Codex entries |
| `test` | Available only in test runs: deterministic stepping, assertions, input injection, state hashing |

**Execution model:**

- Scripts run on the simulation thread at defined hook points: init, fixed tick for
  scripts that request it, events and coroutines for quests and cutscenes, and frame
  callbacks for UI only.
- Hot loops must not call into Lua per cell or per particle. The API offers batch
  operations instead.
- Generation hooks such as building grammars run on worker threads in **separate Lua
  states** with read-only access to world data. This keeps generation deterministic and
  parallel.

## 9.5 Sandbox and safety

- No `os.execute`, `io.popen` or unrestricted `io`. File access goes through `vfs` (read
  only) and `save` (user data only).
- No `load` of binary chunks, and no `debug` library outside development builds.
- A CPU instruction budget per hook in development builds, so runaway scripts are
  reported with a stack trace.
- A script error never crashes the engine. The offending hook is disabled, the error is
  shown in an on-screen console (development) or logged (release), and the game
  continues where possible.

## 9.6 Asset formats

| Kind | Format | Notes |
| --- | --- | --- |
| Textures and sprites | PNG | Sprite sheets plus a Lua descriptor (frames, directions, pivots) |
| Models | glTF 2.0 (`.glb` / `.gltf`) | Exported from Blender or any other DCC tool |
| Voxel models | MagicaVoxel `.vox` | Props, statues, set-piece parts |
| Audio | OGG Vorbis, WAV | Music stems as OGG |
| Shaders | GLSL (a version-neutral source with an engine-supplied header) | One source for GL 3.3 core and GLSL ES 3.00 |
| Fonts | Bitmap fonts (PNG + descriptor) and TTF | The pixel font is project-made (CC BY 4.0) |
| Data | Lua tables | Validated against schemas at load; errors name file and line |
| Translations | Lua string tables | Keyed strings with plural rules |

## 9.7 Development workflow for content

- **Hot reload** in development builds. Changed scripts, data, textures and shaders reload
  live. Data reloads re-run validation and refresh dependent caches.
- **Developer console** with a Lua REPL, teleport, time controls, field visualisation
  overlays (temperature, moisture, mana per color, populations) and a chunk-stage
  inspector.
- **Headless runner** (`zauberer --headless --script <file>`) for automated content
  tests in mods. Mods can ship their own `tests/`.

## 9.8 Licensing of contributions

- **Code** (C engine, Lua scripts, shaders, build files) is under the **MIT License**.
- **Creative assets** (textures, sprites, models, voxels, audio, music, fonts made by the
  project, story text and lore) are under **Creative Commons Attribution 4.0 (CC BY
  4.0)**.
- Every file carries an SPDX license identifier, either in a header or in a repository
  license map. CI checks that every file is covered.
- `credits.lua` per package lists the author, source and license of every asset. CI
  checks that every asset has an entry, and the in-game credits screen is generated from
  it, which satisfies CC BY attribution.
- Third-party assets are only accepted if they are CC0 or CC BY 4.0 compatible.
  Anything else needs an explicit project decision.
- Mods choose their own license. The manifest's `license` field is shown in the mod list.
