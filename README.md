# Zauberer

*Working title. "Zauberer" is German for "wizard".*

Zauberer is an open-source game about a child who inherits a living flying carpet and a
murdered father's spellbooks. The child grows into a mage on an endless planet that is
generated lazily and simulated physically, chemically and biologically. Magic is built
from words, gestures, ingredients and eight colors of mana, and it acts through the
world's simulation of heat, fire, water, air and life. Play alone, or in online co-op
with up to three friends as the father's children, the heirs of the carpet.

> **Status: M0, engine foundation.** The engine core runs on Linux, Windows, macOS and the
> web (threaded and single-threaded builds): packages and a virtual file system, the Lua 5.4
> host, input with touch controls, retro rendering, audio, replays and the full test
> machinery. The M0 demo is a flying carpet with mana motes, text, sound and a credits list.

## Documents

| Document | What it covers |
| --- | --- |
| [Game concept](docs/concept/README.md) | Vision, pillars, story, carpet, magic, world generation, simulation, settlements, gameplay, presentation, modding |
| [Implementation plan](docs/implementation-plan.md) | Architecture, technology stack, repository layout, quality strategy, milestone roadmap M0–M11, decisions, risks, open questions |
| [OpenSpec change: engine foundation](openspec/changes/add-engine-foundation/proposal.md) | The first change (M0): proposal, design, nine capability specs, tasks |
| [Lua API reference](docs/api/README.md) | Every function scripts can call (generated from the C bindings; type stubs in `docs/api/zauberer.d.lua`) |
| [Creating packages](docs/modding/packages.md) | Package layout, `package.lua`, scripts, hooks, data, credits and testing a mod |

## Technology

| Area | Choice |
| --- | --- |
| Engine | C11, SDL3, OpenGL 3.3 core / OpenGL ES 3.0 (WebGL 2) |
| Game layer | Lua 5.4 scripts and data, loaded from package folders next to the binary |
| Platforms | Windows, Linux, macOS, Web (Emscripten), WebXR; touch controls on mobile web; cross-play co-op for up to four players |
| Quality | Spec-driven development with [OpenSpec](https://github.com/Fission-AI/OpenSpec), ≥ 95 % line coverage (C and Lua), automated headless game tests, CI on all platforms |

## Building and running

Requirements: CMake ≥ 3.25, Ninja, a C11 compiler (GCC, Clang or MSVC) and Python 3.
SDL 3 is fetched at a pinned commit; Lua, stb, cgltf and Unity are vendored in
`third_party/`. On Linux, SDL needs the usual development headers (X11 or Wayland, EGL,
ALSA or PulseAudio).

```sh
cmake --preset dev                 # debug build with hot reload and tests
cmake --build --preset dev
build/dev/bin/zauberer             # the demo (assets/ is staged next to the binary)
```

Presets: `dev`, `release`, `coverage` (GCC/Clang + gcovr), `asan` (AddressSanitizer and
UBSan), and with the [Emscripten SDK](https://emscripten.org) (version in
`cmake/emsdk-version.txt`, `EMSDK` set): `web` (threads, needs cross-origin isolation),
`web-st` (single-threaded) and `web-node` (headless engine and unit tests under Node.js).

```sh
source "$EMSDK/emsdk_env.sh"
cmake --preset web-st && cmake --build --preset web-st
node tests/web/server.mjs build/web-st/bin    # prints the URL to open
```

With a WebXR headset (or a WebXR emulator extension), the page shows an **Enter VR** button
that starts an immersive session; the camera becomes the seat the head moves around.

Useful options: `--assets <dir>`, `--user-dir <dir>`, `--headless`, `--ticks <n>`,
`--seed <n>`, `--debug-views 2` (stereo), `--record <file>` / `--replay <file>`
(`zauberer --help` lists all). Player data lives in `%APPDATA%/Zauberer`,
`$XDG_DATA_HOME/zauberer`, `~/Library/Application Support/Zauberer` or IndexedDB on the web.

Co-op: `zauberer --host 7777` hosts a session on a desktop and `zauberer --join
ws://<host>:7777` joins it, natively or in a browser (`?args=--join%20ws://localhost:7777`).
Up to four heirs fly together; every game generates the same world from the seed.

## Testing

```sh
ctest --preset dev                                   # unit, engine, rendered and tool tests
build/dev/bin/zauberer --headless --test tests/lua --junit lua.xml
build/dev/bin/zauberer --headless --scenario tests/scenarios/demo_runs.lua
build/dev/bin/zauberer --headless --replay tests/replays/demo.zbrec   # checks recorded state hashes
cd tests/web && npm install && node --test web.test.mjs              # browser tests (web, web-st builds)
```

- **Unit tests** (Unity, `tests/unit`) check every C module and fail on leaked allocations.
- **Engine tests** (`tests/engine`, Python) drive the binary: startup errors, scenarios,
  Lua tests, replays and, on Linux, rendered golden images (SDL offscreen driver + Mesa
  llvmpipe). Regenerate goldens with `ZB_UPDATE_GOLDEN=1`.
- **Quality gates** (`tools/`): line coverage ≥ 95 % for C and Lua with a ratchet
  (`coverage_gate.py`), every spec scenario tagged by a test (`spec_coverage.py`), module
  layering (`check_includes.py`), REUSE/license split (`check_licenses.py`, `reuse lint`),
  asset credits (`check_credits.py`) and the generated Lua API reference
  (`gen_api_docs.py --check`). CI runs all of them on every push
  ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)).

## Working with OpenSpec

This repository uses OpenSpec's `spec-driven` workflow. The project context and rules are
in [`openspec/config.yaml`](openspec/config.yaml). The OpenSpec skills for Claude Code are
in `.claude/skills/`.

```sh
npm install -g @fission-ai/openspec   # or use npx
openspec list                          # changes in progress
openspec show add-engine-foundation    # read a change
openspec validate --all --strict       # validate specs and changes
```

To implement a change, use `/openspec-apply-change` in Claude Code. To start a new
milestone, use `/openspec-propose`.

## License

- **Code** (engine, scripts, shaders, build files and tools) is licensed under the
  [MIT License](LICENSE).
- **Art and other creative assets** (textures, sprites, models, voxels, audio, music,
  project fonts, story text) and the documentation are licensed under
  [Creative Commons Attribution 4.0 International (CC BY 4.0)](LICENSES/CC-BY-4.0.txt).

The license texts are in [`LICENSES/`](LICENSES/). Every file carries SPDX metadata (headers
or [`REUSE.toml`](REUSE.toml)); every package credits its assets in `credits.lua`.
Third-party code is listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
