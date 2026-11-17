# Proposal: Engine Foundation

## Why

Zauberer has a concept (`docs/concept/`) and a plan (`docs/implementation-plan.md`) but
no code. Every later milestone depends on the same base: a portable C engine on SDL3 and
OpenGL, content loaded from package folders beside the binary, a Lua game layer, and
quality gates (95 % coverage, automated game tests, licensing checks) that are much
cheaper to set up first than to add later. This is roadmap milestone **M0**.

## What Changes

- Add a CMake build with presets that produces the engine for Windows, Linux, macOS and
  Web (Emscripten; pthreads and single-thread variants) from one source tree.
- Add the C core (memory, containers, math, deterministic hashing/RNG, logging, jobs) and a
  thin SDL3 platform layer using SDL3 main callbacks on every platform.
- Add a fixed-step game loop with interpolated rendering and a headless mode.
- Add the package system: `assets/base` and `mods/*` folders next to the binary, manifests,
  dependency ordering, overlay lookup, an asynchronous virtual file system and lazy HTTP
  loading with caching on the web.
- Add the Lua 5.4 host: sandbox, lifecycle hooks, error isolation, API versioning and hot
  reload in development builds.
- Add action-based input for keyboard/mouse, gamepad and touch, with automatic touch mode
  on touch-primary web devices.
- Add a minimal renderer: glTF models, billboard sprites, bitmap text, low internal
  resolution with nearest-neighbour upscale, and a multi-view abstraction ready for stereo.
- Add audio playback of WAV and OGG through a software mixer.
- Add the test machinery: Unity C unit tests, Lua unit tests, headless scenario tests,
  replay tests, coverage gates at 95 % for C and Lua, a spec-coverage check, Playwright
  web smoke tests and a CI matrix for all platforms.
- Add licensing infrastructure: MIT for code, CC BY 4.0 for assets, REUSE/SPDX metadata,
  per-package credits and a generated in-game credits screen.
- Ship a tiny base package: a low-poly carpet model, a sprite, a sound, a font and a
  `main.lua` demo that the exit demo and scenario tests use.

## Capabilities

### New Capabilities

- `platform-targets`: how the game builds, starts and runs on each target platform, and what it does when a platform lacks required features.
- `game-loop`: fixed-step simulation, interpolation, headless execution and deterministic stepping.
- `asset-packages`: package folders beside the binary, manifests, load order, overlay lookup, asynchronous loading and web delivery.
- `lua-scripting`: the Lua host, lifecycle hooks, sandbox, error isolation, API versioning and hot reload.
- `input-actions`: device-independent actions, bindings and their persistence, gamepad support and automatic touch mode.
- `rendering-core`: models, sprites, text, internal-resolution rendering and multi-view rendering.
- `audio-playback`: decoding and mixing of sounds from packages, including browser audio unlocking.
- `test-automation`: unit, scenario and replay tests, coverage and spec-coverage gates, and the CI matrix.
- `content-licensing`: license metadata for every file, asset attribution and the generated credits screen.

### Modified Capabilities

None. This is the first change.

## Impact

- **New code:** `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `src/{core,platform,vfs,script,render,audio,app}/`,
  `web/`, `tests/`, `tools/`, `third_party/`, `assets/base/`, `LICENSES/`, `REUSE.toml`,
  `.github/workflows/`.
- **Dependencies:** SDL3 (zlib), Lua 5.4 (MIT), stb_image, stb_vorbis and stb_truetype
  (MIT / public domain), cgltf (MIT), a glad-generated GL loader, and Unity (MIT, tests
  only). Tools only: gcovr, LuaCov, luacheck, Playwright, the REUSE tool and Emscripten.
- **Platforms:** desktop builds start with a window. Web builds need WebGL 2. The
  pthreads web build needs cross-origin isolation (COOP/COEP), and the single-thread
  build doesn't. Mobile web gets touch mode automatically. WebXR is not part of this
  change, but the renderer's view abstraction prepares for it (M1 `add-webxr-session`).
- **Developer workflow:** every later change must keep the coverage, spec-coverage,
  determinism and licensing gates green.
