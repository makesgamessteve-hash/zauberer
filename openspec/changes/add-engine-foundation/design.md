# Design: Engine Foundation

## Context

The repository holds only documentation, the MIT license and the OpenSpec setup. The game
concept is in `docs/concept/`. The architecture, module dependency rules and quality
strategy are in `docs/implementation-plan.md` (sections 1–4). The motivation for this
change is in `proposal.md`, and the required behaviour is in this change's `specs/`.

## Goals / Non-Goals

**Goals:**

- A module skeleton that already follows the dependency rules (§1.1 of the plan), so later
  milestones add modules rather than restructure.
- One main-loop code path for desktop, web and headless runs.
- Asset access that is asynchronous from day one, because the web demands it.
- A renderer whose view list makes stereo (WebXR, milestone M1) an additive change.
- All quality gates active and green with the first commit of code.

**Non-Goals:**

- World generation, simulation, magic, UI toolkit (beyond text), WebXR sessions, gesture
  recognition, world saves (only configuration and a small key-value save API).
- Skinned or animated glTF models. Only static meshes in M0.
- Native mobile builds, and code coverage measured on Windows and macOS (see Open
  Questions).
- Networking. Co-op arrives with `add-netcode-foundation` in M1. M0 avoids single-player
  assumptions so that the foundation fits without rework: input snapshots and recordings
  carry a player index, the state hash is not tied to one player, and no module keeps a
  global "the player".

## Decisions

### 1. Build system: CMake ≥ 3.25 with presets

| Preset | Toolchain | Purpose |
| --- | --- | --- |
| `dev` | Host compiler, Debug | Development features on: hot reload, on-screen errors, instruction budget |
| `release` | Host compiler, RelWithDebInfo | Shipping configuration |
| `coverage` | GCC, `-O0 --coverage` | Coverage gate |
| `asan` | Clang, ASan + UBSan | Sanitizer job |
| `web` | Emscripten, pthreads | Default web build |
| `web-st` | Emscripten, no threads | Fallback web build |
| `web-node` | Emscripten, Node environment | Headless tests and determinism hashes for WASM |

- Dependencies (SDL3, Lua, stb, cgltf, Unity) come from `FetchContent` pinned by URL and
  SHA-256, or are vendored in `third_party/` with their license files. The option
  `ZB_USE_SYSTEM_SDL3` allows distribution packagers to use the system SDL.
- **Alternatives considered:**
  - Meson: SDL3 and Emscripten support is weaker.
  - Premake: less IDE and CI integration.
  - vcpkg or Conan: an extra tool for a small dependency set.

### 2. Module layout as static libraries

`zb_core`, `zb_platform`, `zb_vfs`, `zb_script`, `zb_render`, `zb_audio` and `zb_input`
are static libraries. The `zauberer` executable (`src/app/`) links them. The allowed
dependencies are encoded in `target_link_libraries`. `tools/check_includes.py` fails CI if
a module includes a header from a module it doesn't depend on. Only `zb_platform` sees SDL
and Emscripten headers, only the render backend sees GL, and only `zb_script` sees Lua.

### 3. Main loop on SDL3 main callbacks; headless without SDL

- **Windowed runs** use `SDL_MAIN_USE_CALLBACKS`. Each `SDL_AppIterate` call does the
  following:
  1. drain events,
  2. sample input into the action snapshot,
  3. run up to 8 fixed ticks from a nanosecond accumulator,
  4. render with the interpolation factor.
- **Headless runs** (`--headless`) go through a separate entry function that never
  initialises SDL video or audio. It steps ticks in a plain loop and uses a stub platform
  implementation. This lets the `web-node` build run the whole headless suite under
  Node.js.
- The loop takes a **clock interface**. Unit tests inject a fake clock to verify the
  30 fps, 144 fps and stall scenarios without real waiting.
- **Alternative considered:** a classic `main()` loop with `emscripten_set_main_loop`. It
  works, but it splits code paths and complicates WebXR frame driving later.

### 4. Time and determinism

- The tick counter is a `uint64`, and the tick duration is the constant 1/60 s. The
  accumulator is in integer nanoseconds.
- **Compiler flags:**
  - `-ffp-contract=off` and no `-ffast-math` (GCC, Clang, Emscripten),
  - `/fp:precise` and no `/fp:fast` (MSVC).
- **`zb_hash`** uses 64-bit mixing in the style of SplitMix64 and xxHash. It is
  implemented in-house and unit-tested against fixed vectors.
- **`zb_rng`** is counter-based: `value = hash(seed, stream, counter)`. It has no hidden
  state, so results don't depend on the order of calls between streams.
- **State hash:** the engine hashes its own deterministic state (tick, RNG stream
  counters, input snapshot). Scripts contribute through `engine.hash_state(tbl)`, which
  hashes a canonical serialisation: sorted keys, and IEEE bit patterns for numbers.

### 5. Memory

- All allocation goes through `zb_allocator { alloc, realloc, free, ctx }`. Frame and
  load-time scratch memory uses arenas.
- Test builds wrap the general allocator in a **tracking allocator** that records
  `__FILE__:__LINE__` per allocation. Every unit test and scenario run checks it at
  teardown. Lua's allocator also routes through it.

### 6. Virtual file system and packages

- **Discovery** uses `SDL_GetBasePath` and `SDL_EnumerateDirectory` on desktop. On the web,
  `assets/index.tsv` (path, size, SHA-256) is generated by `tools/make_asset_index.py`
  when packaging.
- **Manifests** (`package.lua`) run in an empty Lua state that only allows table
  constructors and literals, with an instruction cap. A manifest can't run code.
- **Load order** is Kahn's algorithm with a priority queue on package id, which makes it
  stable. Packages that are cyclic or have unresolved dependencies are disabled.
- **Overlay lookup** keeps a path→package map, built once after loading and rebuilt on
  hot reload.
- **The asynchronous API** is `zb_vfs_request(path) → handle`, with states `pending`,
  `ready` and `failed`. On desktop the job workers read files. On the web,
  `web/vfs_fetch.js` serves requests with `fetch` and the **Cache Storage API**, keyed by
  path and hash. A changed hash replaces the entry, and there is a plain network
  fallback when Cache Storage is unavailable.
- **Web boot set:** before any script runs, the web build prefetches every package's
  `package.lua`, `scripts/**`, `data/**` and `locale/**` into memory, so that Lua
  `require` can stay synchronous. Textures, models, sounds and music stay on demand.
- **Placeholders** (checkerboard texture, unit cube, silence) are generated in code. They
  are not assets.

### 7. Lua host

- Lua 5.4 is built from source with `LUA_32BITS` off and the engine allocator. One main
  state runs the game logic; worker states are added in M2 for generation.
- **Libraries:**
  - base, minus `dofile`, `loadfile` and `print`, which is redirected to the log,
  - `string`, `table`, `utf8`, `coroutine`,
  - `math`, with `math.random` and `math.randomseed` replaced by the deterministic RNG,
  - `load` forced to text mode,
  - no `io`, `os` (except `os.date` for formatting) or `package.loadlib`,
  - `debug` only in `coverage` and `dev` builds, for LuaCov and the instruction budget.
- **Environments:** each package gets its own `_ENV` table, whose metatable falls back to
  the shared API. Packages can't overwrite each other's globals by accident.
- **Hooks:** `engine.on("tick", fn)` and similar register a hook per package and event.
  Dispatch happens in load order through `lua_pcall` with a traceback message handler.
  A failure disables that package's hook.
- **Instruction budget:** in `dev` and test builds, `lua_sethook` with `LUA_MASKCOUNT`
  every 1,000 instructions accumulates per invocation. Exceeding 10 M raises an error,
  which is then handled by error isolation.
- **Hot reload** (dev builds only): file modification times are polled every 250 ms, with
  no OS-specific watcher dependency. Changed packages reload, and `on_reload` gets the
  table that the package passed to `engine.preserve(tbl)`.
- **API surface** in M0: `engine`, `vfs`, `input`, `render` (minimal), `audio`, `save`
  (key-value), and `test` (test runs only). Bindings are handwritten C in
  `src/script/api_*.c`. Annotation comments in Lua Language Server format are extracted
  into `docs/api/` and a `zauberer.d.lua` definitions file for modders' editors. The API
  version lives in `ZB_API_VERSION_{MAJOR,MINOR}`.
- **The sandbox is not a security boundary.** It prevents accidents and protects
  determinism. Mods are treated as trusted code, as in most moddable games. This is
  documented for modders.

### 8. Input

- **Pipeline:** SDL events update the raw device state. Bindings resolve it into the
  action snapshot (a fixed array per action), which is sampled once before each tick.
  Each snapshot carries a player index (always 0 in M0), so that remote players' inputs
  from M1's netcode use the same path.
- **Gamepads** use the SDL3 gamepad API with its mapping database. Hot-plug events
  release the actions held by a removed device.
- **Touch mode on the web:**
  - At start, `web/touch_detect.js` evaluates `matchMedia('(pointer: coarse)')` and
    `navigator.maxTouchPoints`.
  - SDL's touch-to-mouse synthesis is disabled with `SDL_HINT_TOUCH_MOUSE_EVENTS=0`, and
    touch handlers call `preventDefault` so browsers don't send compatibility mouse
    events.
  - Mouse events with `SDL_TOUCH_MOUSEID` are ignored, and a 300 ms grace window after
    the last touch blocks mode flips.
- **Virtual controls** are engine-side hit testing over regions declared by scripts. Each
  touch is owned by the control where it started.
- **Recording format:** a header (magic, version, seed, package-set hash) followed by
  per-tick snapshots, run-length encoded for unchanged ticks.

### 9. Rendering

- **GL loading:** desktop uses a glad 2 loader generated for GL 3.3 core. The web links
  WebGL 2 directly (`-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2`).
- **Shaders:** one GLSL source each. The engine prepends `#version 330 core` or
  `#version 300 es` plus precision qualifiers.
- **Frontend and backend:**
  - The frontend culls against each view's frustum and emits sorted draw items (view,
    pipeline, mesh, material, transform, sort key) into a draw list.
  - The GL backend executes the draw lists.
  - The null backend, used by headless runs and tests, records them for assertions.
- **Internal resolution:** an internal framebuffer (color texture + depth renderbuffer) at
  the internal resolution. `glBlitFramebuffer` with `GL_NEAREST` copies it to the integer
  scaled, centred rectangle of the default framebuffer, and the rest is cleared.
- **Content types:**
  - **Models:** cgltf parses static meshes. Nodes are flattened into mesh instances with
    world transforms. Lighting is per-vertex with one directional light plus ambient.
  - **Sprites:** instanced camera-facing quads with alpha test. Frame timing uses
    simulation time.
  - **Text:** a bitmap font (PNG atlas + Lua descriptor), with a replacement glyph for
    missing code points.
- **Golden images:**
  - Desktop: Mesa llvmpipe via the SDL offscreen video driver with EGL, falling back to
    `xvfb-run` with the X11 driver.
  - Web: headless Chromium via Playwright.
  - Comparison tolerance: a per-pixel channel delta ≤ 8, and ≤ 0.5 % of pixels above it.
  - If the two backends diverge beyond tolerance, a per-backend golden is allowed only
    with a recorded rationale.

### 10. Audio

- The output is an SDL3 audio stream at float32 stereo 48 kHz, fed from the mixer in the
  stream callback.
- **Mixer:** fixed voice pool (64), with master, music and effects groups. A smooth peak
  limiter is followed by a hard clamp to [-1, 1].
- **Decoding:**
  - WAV via `SDL_LoadWAV_IO` from VFS memory.
  - OGG via `stb_vorbis` decoding incrementally from the compressed buffer, which keeps
    the decoder state below 1 MiB.
- **Web unlock:** `web/audio_unlock.js` resumes the AudioContext on the first
  `pointerdown` or `keydown` and notifies the engine. Deferred loops and music start
  then, and earlier one-shots are dropped.
- **Headless runs** use a null device. Voices advance by simulation ticks, so handle
  states are deterministic.

### 11. Test machinery

- **Unity** (vendored) runs C tests with one executable per module, registered with
  `zb_add_unit_test()` in CMake.
- **Lua tests** run through `--test <dir>`, using the engine's `test` module (`case`,
  `expect`, `expect_eq`, `expect_error`). Output is JUnit XML.
- **Scenarios** run through `--scenario <file>` with:
  - `scenario{ name, seed, packages }`,
  - `step(n)` and `input.inject(action, value, ticks)`,
  - `expect(...)`, `hash()` and `render.capture()` for golden images in windowed test
    runs.
- **Replays** are stored under `tests/replays/` as recordings plus expected hashes.
- **Spec coverage:** tests carry tags in the form `spec: <capability>: <scenario name>`,
  in comments for C and in scenario metadata for Lua. `tools/spec_coverage.py` collects
  the scenario names from `openspec/specs/**` and `openspec/changes/*/specs/**`
  (excluding the archive), and fails on any name without a tag.
- **Coverage:** the `coverage` preset runs CTest, the Lua tests, the scenarios and the
  replays. gcovr merges the C data and enforces `--fail-under-line 95`. LuaCov runs
  inside the engine with `--lua-coverage <out>`, and `tools/lua_coverage_gate.py`
  enforces 95 %. Both read `tools/coverage-exclusions.txt` (path + justification) and
  check the ≤ 1 % budget. The ratchet compares against `tools/coverage-baseline.json`,
  which is updated on `main` by CI.
- The tools are Python 3 standard library only, and only run in CI and on development
  machines.

### 12. Licensing

- **`REUSE.toml`** declares:
  - `assets/**` (minus `credits.lua`) as `CC-BY-4.0`,
  - code, scripts, shaders, build and tool files as `MIT`,
  - `third_party/<dep>/**` under each dependency's license,
  - docs as `CC-BY-4.0`.
  `LICENSES/` holds `MIT.txt` and `CC-BY-4.0.txt`, plus the third-party license texts.
- **`credits.lua`** in each package is a list of `{ file, author, source, license,
  exception? }`. `tools/check_credits.py` checks that every asset file has an entry, that
  every entry points to an existing file, and that licenses are on the allowlist (CC0-1.0,
  CC-BY-4.0) unless `exception` holds a rationale.
- `engine.credits()` returns the merged credits of all loaded packages plus
  `THIRD_PARTY_NOTICES.md` entries. The M0 demo shows them as a text list.

### 13. Continuous integration

`.github/workflows/ci.yml` runs on GitHub-hosted runners (Ubuntu 24.04, Windows 2022,
macOS 14) with the jobs listed in §4.4 of the plan. The emsdk version is pinned in
`cmake/emsdk-version.txt`, and dependencies are cached by their pin hash. The
`determinism` job downloads the hash artifacts from the Linux, Windows, macOS and
`web-node` jobs and compares them.

### 14. Base package demo content

The M0 assets are tiny and are generated reproducibly by scripts in `tools/gen/`:

- a low-poly carpet `.glb`,
- an 8 × 8 pixel font,
- a sprite sheet,
- a synthesised WAV chime and a short OGG loop.

They are licensed CC BY 4.0 with credits entries. `scripts/main.lua` shows the spinning
carpet, the sprite and text, plays the sound, and lets every input device rotate the
carpet.

## Risks / Trade-offs

- [llvmpipe with the SDL offscreen + EGL driver unavailable on a runner image] → Fall back
  to `xvfb-run` with the X11 driver. Both are scripted in CI.
- [Golden images differ between llvmpipe and Chromium's software WebGL] → Tolerance
  thresholds, with documented per-backend goldens as a last resort.
- [Platform-specific source files (`#ifdef _WIN32`, macOS paths) can't be covered on
  the Linux coverage run] → Keep them tiny and isolated in `src/platform/os_*.c`, list
  them in the exclusion file with a justification, and exercise them in the Windows and
  macOS scenario jobs.
- [Pthreads web builds need COOP/COEP, which some static hosts can't send] → The `web-st`
  fallback is selected automatically by the shell.
- [A 95 % gate from the first commit slows early prototyping] → Fake platform and clock
  helpers, and scenario tests that cover glue code cheaply. Prototypes live on branches
  that are not merged until tested.
- [The Lua sandbox is mistaken for security] → Documented as accident and determinism
  protection only.
- [Async VFS complicates scripts] → Scripts and data are preloaded at boot, so `require`
  is synchronous, and only heavy assets use handles.

## Migration Plan

Not applicable: this is a greenfield change with nothing to migrate or roll back.

## Open Questions

- Exact patch versions of SDL3, Lua 5.4, emsdk and the third-party headers. These are
  pinned when implementation starts.
- Whether to add coverage collection on Windows and macOS later and merge it into the gate
  (it would shrink the exclusion list).
- Linux distribution format (AppImage or tarball). This is decided in the release
  milestone (M11).
