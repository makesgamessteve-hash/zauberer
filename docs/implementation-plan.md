# Zauberer: Implementation Plan

This document turns the [game concept](concept/README.md) into an engineering plan. It
covers:

- the architecture,
- the technology stack,
- the repository layout,
- the quality strategy (OpenSpec, ≥ 95 % coverage, automated game tests),
- a milestone roadmap with the planned OpenSpec changes,
- key decisions and risks.

**How work flows:** every milestone is delivered as one or more **OpenSpec changes**
(`openspec/changes/<id>/`), each with a proposal, behaviour specs, a design and a task
list. A change is implemented, verified against its specs by automated tests, and then
archived, which merges its specs into `openspec/specs/`, the living truth of what the
game does. The first change, [`add-engine-foundation`](../openspec/changes/add-engine-foundation/proposal.md),
is fully specified. Later changes are proposed when their milestone starts, so that they
can build on what the earlier ones taught us.

---

## 1. Architecture

### 1.1 Layers

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ Lua game layer (assets/base/scripts, mods)                                  │
│   story · quests · magic rules · disciplines · UI · NPC/creature policies   │
│   data registries (materials, species, weaves, recipes, rooms, cultures …)  │
├──────────────────────────────── scripting API (versioned) ──────────────────┤
│ C engine                                                                    │
│  game/    entity store, magic primitives, gameplay systems needing speed    │
│  sim/     fields (heat, moisture, gas, mana) · shallow water · fire ·       │
│           chemistry · ecology/populations · physics · weather · scheduler   │
│  world/   planet model · regions · chunks · generators · streaming · saves  │
│  render/  frontend (scene → draw lists) · GL backend · retro post · views   │
│  audio/   mixer · 3D emitters · streaming music                             │
│  input/   actions · bindings · gamepad · touch & virtual controls · replay  │
│  script/  Lua host · bindings · sandbox · hot reload · worker Lua states    │
│  vfs/     packages · overlay lookup · async loading · web fetch + cache     │
│  platform/ SDL3: window, GL context, input, audio device, threads, paths,   │
│           web glue (Emscripten), WebXR bridge, touch detection              │
│  core/    memory (arenas, pools) · containers · math · deterministic math · │
│           hashing & counter-based RNG · jobs · log · profiler · strings     │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Dependency rules** (enforced by CMake target links and a CI include-graph check):

- `core` depends on nothing but the C standard library. It is pure, portable and
  fully unit-testable.
- `platform` is the only module that includes SDL or Emscripten headers. It is kept
  **thin**: no logic that could live elsewhere.
- `world`, `sim`, `game`, `input` and the render **frontend** depend on `core` and on
  each other downward only. None of them knows SDL or GL. `platform` feeds raw device
  events into `input`.
- The render **backend** is the only code that calls OpenGL.
- `script` binds everything for Lua and is the only module that includes Lua headers.
- `app/main.c` is the composition root. It wires the modules together, and nothing
  depends on it.

### 1.2 Main loop and scheduling

- **SDL3 main callbacks** (`SDL_AppInit`, `SDL_AppIterate`, `SDL_AppEvent` and
  `SDL_AppQuit`) are used on every platform. They map naturally onto the browser's
  frame loop, so desktop and web share one code path.
- **Fixed-step simulation:** a 60 Hz base tick with an accumulator and a clamp on the
  number of catch-up steps per frame. Subsystems tick at divided rates through a
  **multi-rate scheduler** (see [Simulation §5.3](concept/05-simulation.md#53-multi-rate-scheduler)).
  Rendering interpolates between the last two states.
- **Headless mode:** the same loop runs without a window, GL or audio device, as fast
  as possible or stepped explicitly. It is used for automated tests, world-generation
  tools and servers in the future.
- **WebXR:** while an XR session is active, the XR frame callback (JavaScript) calls an
  exported engine function that runs the iterate step with the XR views. The normal
  iterate step skips rendering during that time.

### 1.3 Threading

- **Main thread:** platform events, Lua game logic, render submission.
- **Job system:** a fixed worker pool with work stealing, used for chunk generation
  stages, meshing, field updates partitioned by chunk (with ghost cells), population
  updates, pathfinding and asset decoding. Jobs carry priorities and can be cancelled.
- **Generation in Lua** (building grammars, furnishing, set pieces) runs in **separate
  Lua states per worker**, with read-only access to world data.
- **Determinism under threading:** jobs write only to their own outputs. Reductions run
  in a fixed order, and all randomness is counter-based.
- **Web:** the default web build uses Emscripten pthreads, which requires cross-origin
  isolation (COOP/COEP headers, or a service-worker shim on static hosts). A
  **single-thread web build** runs jobs inline with time slicing, for hosts that can't
  isolate.

### 1.4 Coordinates and precision

- World position = `(face, chunk_x, chunk_y)` integers + `float3` local offset.
- A **floating origin** anchors physics and rendering near the camera and re-anchors as
  the player moves. GPU matrices are camera-relative.
- Planet-scale computations (macro grid, sun position) use double precision where needed,
  and never in hot loops.

### 1.5 Determinism

This is required for portable seeds and saves, replays and stable tests.

- Counter-based hashing RNG (seed ⊕ layer ⊕ coordinates ⊕ purpose). There is no global
  RNG state.
- Compiler flags forbid fused multiply-add contraction (`-ffp-contract=off` and the
  equivalent for MSVC), with no fast-math.
- World generation uses `core/dmath`, which has its own deterministic `sin`, `cos`, `exp`,
  `log` and `pow`. Generation never calls platform `libm`.
- **Golden hash tests:** given seeds produce identical chunk hashes on Linux, Windows,
  macOS and WebAssembly. CI compares the hashes across platform jobs.
- **Long simulations are stored, not replayed.** Deep-time geology and the history
  simulations run for hundreds of steps and are sensitive to tiny differences. Their
  results are saved as the **world bundle**, so saves never depend on recomputing them.
  Cross-platform determinism of world creation is still a goal, verified by golden hashes
  on reduced-size worlds.

### 1.6 Entities

- Generational entity IDs.
- Core components (transform, physics body, creature state, plant state, emitter and so
  on) are stored as dense structure-of-arrays pools in C, and systems iterate over them.
- **Script components** are Lua tables keyed by entity ID, so mods can add new components
  without C changes.
- Events flow from C to Lua through a queue, which is dispatched once per tick in a
  deterministic order.

### 1.7 Rendering

- **API subset:** OpenGL **3.3 core** on desktop (macOS caps at 4.1, so no compute
  shaders) and **OpenGL ES 3.0 / WebGL 2** on the web. Only features available in all of
  them are used: VAOs, instancing, uniform buffers, texture arrays and integer
  attributes. There are no geometry, tessellation or compute shaders.
- **Shaders:** one GLSL source per shader. The engine prepends a version and precision
  header for each target.
- **Frontend and backend split:**
  - The frontend culls and builds sorted **draw lists**. This is pure C, and unit tests
    run it against a null backend.
  - The GL backend executes the draw lists. Tests cover it with offscreen rendering on
    Mesa llvmpipe in CI and compare against golden images.
- **Views:** every frame renders a list of views: 1 for a flat screen, 2 for XR, using
  `OVR_multiview2` when available. Stereo support exists from Milestone 1, so XR never
  needs a retrofit.
- **Passes:**
  1. sky,
  2. terrain (LOD rings, curvature),
  3. opaque models and kit pieces,
  4. voxel meshes,
  5. vegetation instancing (alpha-tested),
  6. water,
  7. particles and sprites,
  8. retro post-processing (palette, dither, fog composite) at the internal resolution,
  9. upscale to the window,
  10. UI.

### 1.8 Asset access (VFS)

- Packages are folders (§9 of the concept). Lookup is an overlay in package priority
  order.
- The **asynchronous API is primary** (request → handle → ready callback or poll), because
  the web can't block on file reads. On desktop, worker threads serve the requests. A
  synchronous wrapper exists only for tools and tests.
- **Web:** the deploy step generates an `assets/index` manifest (paths, sizes, hashes).
  Files are fetched over HTTP on demand and cached in the browser (Cache API or
  IndexedDB), invalidated by hash. A small boot set (manifests, scripts, data, UI) is
  prefetched before `main.lua` runs. Assets are **never** embedded in the `.wasm`.

### 1.9 Persistence

```
saves/<world>/
  world.lua        seed, planet options, generator version, package set, play time
  bundle/          the world bundle, written once at world creation:
    planet.bin       macro-grid fields (elevation, climate, rivers, biomes, mana, soil)
    rocks.bin        rock-record dictionary and per-cell strata columns, mineral deposits
    ley.bin          ley graph and nexuses
    history.db       cultures, languages, faiths, polities, figures, events, present state
  global.bin       macro simulation state (weather, factions, rivals, story director)
  player.bin       player, carpet, bag, tent interior state
  script.lua       serialised Lua state (quests, journal, Codex)
  regions/<face>.<rx>.<ry>.bin   chunk deltas for a 16 × 16 chunk region (compressed)
```

- Apart from the world bundle, only **deltas** are stored: terrain edits, removed or
  added plants, structure damage, moved items, persistent entities and chunk simulation
  timestamps.
- The world bundle is typically 10–30 MB compressed and can be exported to share a
  world.
- Every file has a versioned header and a checksum. Writes are atomic (write to a temp
  file, then rename). Old save formats are loaded by migration code, with fixtures under
  test.
- On the web, saves live in IndexedDB through Emscripten's IDBFS.

### 1.10 Platform specifics

| Platform | Notes |
| --- | --- |
| Windows | MSVC or clang-cl, x64. Assets beside the `.exe`, user data in `%APPDATA%/Zauberer` |
| Linux | GCC or Clang, x64 (arm64 later). AppImage or tarball |
| macOS | Clang, universal (arm64 + x64). OpenGL 4.1 core profile (deprecated by Apple but functional); assets in `Contents/Resources` beside the binary inside the app bundle |
| Web | Emscripten, WebGL 2, pthreads (plus a single-thread fallback build). `index.html` shell, `assets/` folder fetched lazily. Touch UI on coarse-pointer devices |
| WebXR | `immersive-vr` through a small JS bridge (our own code, MIT). Stereo views, controller poses, select and squeeze events → engine input actions |

---

## 2. Technology stack

| Purpose | Choice | License | Notes |
| --- | --- | --- | --- |
| Language | C11 (C17 compilers) | — | No VLAs, explicit-width integers, allocators passed explicitly |
| Platform | **SDL 3** (3.2+) | zlib | Window, GL context, input, audio, threads, paths; main callbacks |
| Graphics | **OpenGL 3.3 core / ES 3.0** | — | GL loader generated with glad 2 (license recorded when vendored) |
| Scripting | **Lua 5.4** | MIT | Built from source on all platforms incl. WASM |
| Images | stb_image | MIT / public domain | PNG decoding |
| Audio decode | stb_vorbis | MIT / public domain | OGG Vorbis |
| Fonts | stb_truetype + own bitmap font | MIT / public domain | |
| Models | cgltf | MIT | glTF 2.0 |
| Voxels | ogt_vox | MIT (to confirm when vendoring) | MagicaVoxel `.vox` |
| Compression | LZ4 | BSD-2-Clause | Save regions (decided in the persistence change) |
| Build | CMake ≥ 3.25 with presets; Emscripten SDK | BSD-3 / MIT | Dependencies fetched by pinned hash (FetchContent) or vendored in `third_party/` |
| C unit tests | **Unity** (ThrowTheSwitch) | MIT | One test executable per module, run by CTest |
| C coverage | gcov/llvm-cov + **gcovr** | GPL runtime exception / Apache-2.0 / BSD-3 (tools only) | `--fail-under-line 95` |
| Lua tests and coverage | Built-in `test` module + **LuaCov** | MIT | Run inside the headless engine |
| Lua lint | luacheck | MIT | |
| Web E2E | **Playwright** (Chromium) + a WebXR emulation runtime (e.g. IWER) | Apache-2.0 / MIT | Desktop, mobile-touch and XR smoke tests |
| Licensing lint | REUSE tool | GPL-3.0 (tool only, not shipped) | Every file has SPDX coverage |
| Spec workflow | **OpenSpec** | MIT | `openspec validate --strict` in CI |
| Profiling | Built-in markers; optional Tracy in dev builds | BSD-3 | |

All shipped dependencies are permissive (MIT, zlib, BSD or public domain) and compatible
with the MIT-licensed code. `THIRD_PARTY_NOTICES.md` lists them with exact versions.

---

## 3. Repository layout

```
CMakeLists.txt  CMakePresets.json        build entry; presets: dev, release, coverage, asan, web, web-st
cmake/                                    toolchains, warnings, coverage, sanitizer, dependency pins
src/
  core/ platform/ vfs/ script/ input/ render/ audio/ world/ sim/ game/
  app/main.c                              composition root
web/                                      HTML shell, JS bridges (webxr.js, touch detection, fetch cache)
assets/base/                              the base game package (scripts, data, art, audio, …)
tests/
  unit/<module>/                          Unity tests per C module
  lua/                                    Lua unit tests (run in headless engine)
  scenarios/                              automated game tests (Lua, headless, deterministic)
  replays/                                recorded input streams + expected state hashes
  golden/                                 worldgen hashes, reference images, save fixtures
  web/                                    Playwright tests
  fuzz/                                   libFuzzer targets (nightly)
tools/                                    asset index generator, spec-coverage checker, coverage merge, packaging
third_party/                              vendored sources with their license files
docs/                                     concept, this plan, API reference (generated)
openspec/                                 config, specs (truth), changes (in flight + archive)
LICENSES/  REUSE.toml                     MIT, CC-BY-4.0 texts; license map for all files
```

---

## 4. Quality strategy

### 4.1 OpenSpec workflow

1. **Propose:** use `/openspec-propose` or write the files by hand to get a proposal,
   delta specs, a design and tasks. Each spec scenario is phrased as *WHEN / THEN* and
   is meant to become a test.
2. **Review:** a human reviews the proposal before implementation. `openspec validate
   --strict` must pass.
3. **Apply:** use `/openspec-apply-change` and work through the tasks. Each task group
   lands its own tests and docs.
4. **Verify:** CI is green, coverage gates hold, and **spec coverage** is complete (every
   scenario has a test, see 4.3).
5. **Archive:** `openspec archive <id>` merges the delta specs into `openspec/specs/`.

### 4.2 Test pyramid

| Level | What | Tooling | Runs |
| --- | --- | --- | --- |
| **Unit (C)** | Every module in `src/`, with the backend boundary faked through function tables | Unity, CTest, ASan/UBSan | Every push |
| **Unit (Lua)** | Game-layer modules: magic rules, registries, UI logic, quest graph | Headless engine + `test` module, LuaCov | Every push |
| **Property and invariant** | Water mass conserved, energy bounded, populations non-negative, no NaN/Inf; randomised over seeds | Unity + seeded generators | Every push (short), nightly (long) |
| **Golden** | Worldgen chunk hashes per generator version; reference images (llvmpipe, tolerance); save-format fixtures | CTest + tools | Every push |
| **Scenario (automated game tests)** | Deterministic headless game situations scripted in Lua: spawn, inject input, step N ticks, assert on world state; optionally rendered with screenshots | `zauberer --headless --scenario` | Every push |
| **Replay** | Recorded input streams replayed; state hash checked every N ticks | Same runner | Every push |
| **Web E2E** | Web build boots, renders, reacts to input; touch UI on mobile emulation; XR session in emulation | Playwright | Every push (smoke), nightly (full) |
| **Performance** | Benchmark scenarios (flight path, forest fire, city fly-through) record tick, frame and generation times plus memory | Runner + trend dashboard | Nightly; gating thresholds from M3 |
| **Soak** | An autopilot explorer flies for 2 game-hours across biomes, checking invariants, Lua errors and memory growth | Runner | Nightly |
| **Fuzz** | Save loading, package manifests, sprite descriptors, the glTF wrapper and the Lua-facing data validators | libFuzzer | Nightly |

### 4.3 Coverage gates

- **C:** line coverage ≥ **95 %** over `src/` (combined unit + scenario + golden runs on
  the Linux coverage build). Branch coverage is reported, with a target of ≥ 85 %.
- **Lua:** line coverage ≥ **95 %** over `assets/base/scripts` (LuaCov, combined Lua unit
  and scenario runs).
- **Exclusions** are limited to `third_party/` and `tests/`. A short, reviewed allowlist
  (`tools/coverage-exclusions.txt`) covers code that can't run in CI, for example a
  driver-specific GL workaround. Each entry needs a written justification, and the list
  is expected to stay under 1 % of lines. Inline exclusion markers are only allowed for
  unreachable defensive branches, with a comment explaining why.
- **Spec coverage:** `tools/spec-coverage` parses all scenarios in `openspec/specs/` and
  in active changes. It fails if a scenario has no test that references it by
  `capability: scenario name`.
- **Ratchet:** coverage percentages may not drop on `main`.

### 4.4 Continuous integration (GitHub Actions)

| Job | Content |
| --- | --- |
| `lint` | clang-format check, clang-tidy (selected checks), luacheck, REUSE lint, `openspec validate --all --strict`, include-graph rules |
| `linux-gcc-coverage` | Debug build with coverage → all tests → gcovr and LuaCov gates → spec-coverage check → report artifact |
| `linux-clang-asan` | ASan and UBSan build → unit, scenario and replay tests |
| `linux-render` | Rendered scenario tests and golden images with Mesa llvmpipe (xvfb or SDL offscreen + EGL) |
| `windows-msvc` | Release and debug build → unit and scenario tests |
| `macos` | Universal build → unit and scenario tests |
| `web` | Emscripten builds (pthreads and single-thread) → Lua and scenario tests under Node → Playwright smoke (desktop, mobile touch, XR emulation) |
| `determinism` | Collects worldgen and replay hashes from every platform job and fails on any mismatch |
| `nightly` | Fuzzing, soak, performance trends, full Playwright suite |
| `package` (tags) | Per-platform archives (binary + `assets/`), web bundle deployable to static hosting |

### 4.5 Coding standards

- `zb_` prefix for public symbols, `zb_<module>_` per module, and `static` for everything
  internal.
- Errors are returned as result codes with context. `abort` is used only for broken
  invariants, through `ZB_ASSERT`, which is active in debug and test builds.
- There is no hidden global state. Subsystems are explicit context structs, which makes
  them testable in isolation and runnable several times in one test process.
- Memory goes through allocators (arena, pool, general purpose with leak tracking in
  tests). Every test fails on leaks.
- Warnings are errors (`-Wall -Wextra -Wpedantic -Wconversion` and `/W4` on MSVC) in CI.
- `docs/` and the generated Lua API reference are updated in the same change as the
  code.

### 4.6 Definition of Done (per OpenSpec change)

- [ ] `openspec validate <id> --strict` passes.
- [ ] All tasks are checked, and every spec scenario is covered by an automated test.
- [ ] C and Lua line coverage ≥ 95 %, with no ratchet regression.
- [ ] CI is green on Linux, Windows, macOS and Web. Determinism hashes match.
- [ ] Docs and the API reference are updated, and new assets have license and attribution
      entries.
- [ ] The change is archived and its specs are merged into `openspec/specs/`.

---

## 5. Roadmap

Every milestone ends with a **playable or demonstrable build on all platforms**. The
planned change IDs are provisional and are refined when the milestone starts. Sizes are
relative (S < M < L < XL).

```
M0 Foundation ─► M1 First Flight ─► M2 A Planet ─► M3 Living Physics ─► M4 First Magic ─┐
                                                                                         │
      ┌──────────────────────────────────────────────────────────────────────────────────┘
      ▼
M5 Living Things ─► M6 Carpet & Home ─► M7 The Disciplines ─► M8 Places & People ─► M9 The Story ─► M10 Release
                          (Vertical Slice A after M4, Vertical Slice B after M8 prologue work)
```

### M0: Foundation (M)

- **Goal:** an engine skeleton on all platforms with the full quality machinery running
  from day one.
- **Changes:** `add-engine-foundation`, which is **fully specified** in
  `openspec/changes/add-engine-foundation/`.
- **Capabilities:** `platform-targets`, `asset-packages`, `lua-scripting`, `game-loop`,
  `input-actions`, `rendering-core`, `audio-playback`, `test-automation`,
  `content-licensing`.
- **Exit demo:** on Windows, Linux, macOS and Web, the binary loads `assets/base`. Lua
  spins a low-poly carpet model over a sprite backdrop and plays a sound, and mouse,
  keyboard, gamepad and touch input rotate it. The headless scenario suite passes in
  CI, and the coverage gates are active.

### M1: First Flight (L)

- **Goal:** fly endlessly over streamed terrain with the retro look, on all platforms.
- **Changes:**
  - `add-planet-coordinates` (cube-sphere addressing, floating origin),
  - `add-terrain-chunks` (noise-based continents v1, chunk stages 0–2),
  - `add-chunk-streaming` (rings, priority queue, budgets, hysteresis),
  - `add-retro-renderer` (internal resolution, palette and dither, fog, sky, terrain LOD,
    curvature),
  - `add-carpet-flight`,
  - `add-touch-and-gamepad-controls`,
  - `add-webxr-session` (stereo view of the scene, head tracking: a spike that locks the
    architecture in early).
- **Exit demo:** 10 minutes of uninterrupted flight in any direction. At least 60 fps on
  a mid-range desktop and at least 30 fps on mid-range mobile web. Golden worldgen
  hashes match across platforms, and an XR headset (or emulation) shows the world in
  stereo.

### M2: A Planet (XL)

- **Goal:** a real planet with a geological past, consistent geography, biomes, water and
  forests.
- **Changes:**
  - `add-deep-time-geology` (plate tectonics through ≈ 500 Myr, erosion and
    sedimentation, sea level, ice ages, rock record, minerals, fossils, ley network,
    Earth-likeness steering, amplification to the macro grid),
  - `add-climate-and-biomes` (present climate, hydrology, biomes, biogeographic
    provinces, magical biomes),
  - `add-world-creation` (creation pipeline, options, time-lapse globe, cancellation,
    world bundle),
  - `add-water-bodies` (oceans, rivers, lakes: rendering and flow fields),
  - `add-vegetation` (species registry, placement, the four-level sprite forest),
  - `add-sky-and-time` (day and night, seasons visual),
  - `add-world-persistence` (save and load, deltas, migrations).
- **Exit demo:** create a world while watching its continents drift in the time-lapse.
  Fly from a coastal desert over a rain-shadowed range into a temperate rainforest, see
  the strata in a canyon wall, and follow a river to the sea. Then save, quit, load and
  continue. A nightly corpus of seeds meets the Earth-likeness targets.

### M3: Living Physics (L)

- **Goal:** the simulation backbone.
- **Changes:**
  - `add-material-registry`,
  - `add-thermal-fields-and-fire`,
  - `add-shallow-water`,
  - `add-air-and-gases` (wind field, smoke, scent),
  - `add-weather` (global systems, local precipitation, lightning),
  - `add-simulation-tiers` (tiers, catch-up, scheduler),
  - `add-terrain-deformation`,
  - `add-rigid-bodies`.
- **Exit demo:** lightning ignites a dry forest, and the fire runs downwind and uphill
  and stops at a river. Rain douses it. Fly away and return after a skip of 30 game days,
  and the burn scar shows early regrowth. Breach a natural dam and flood a valley. Water
  mass and energy invariants hold in tests.

### M4: First Magic (L), then Vertical Slice A

- **Goal:** the spell grammar on top of the simulation, and first creatures.
- **Changes:**
  - `add-mana-fields` (emergent sources, depletion, ley lines, nexuses),
  - `add-weave-grammar` (words, gestures, essence, matter, power, cost model, misfires),
  - `add-gesture-recognition` (2D mouse/touch/stick; 3D XR in M10),
  - `add-spellbook` (prepared weaves, hand slots, discovery, familiarity),
  - `add-mana-motes`,
  - `add-creatures-basic` (a few species, utility AI, perception),
  - `add-damage-model`.
- **Vertical Slice A:** a 15-minute playable demo. Fly, scare wolves with fire, set the
  grass alight by accident, put it out with *Unmake Fire*, collect motes from a slain
  storm bird, and freeze a lake to cross on foot.

### M5: Living Things (L)

- **Goal:** ecology and survival.
- **Changes:**
  - `add-ecology` (populations, virtualisation, food web, migration, succession),
  - `add-fauna-behaviour` (full utility AI, herds and packs, scent tracking),
  - `add-plant-life-cycle`,
  - `add-small-life` (fungi, insects, microbes as fields and swarms),
  - `add-survival` (needs, body temperature, conditions, disease),
  - `add-codex`,
  - `add-harvest-and-cooking`.
- **Exit demo:** survive 7 in-game days in two contrasting biomes by foraging, hunting and
  cooking. Get sick from swamp water and cure it with boiled water and herbs. Watch
  predators follow scent downwind.

### M6: Carpet and Home (M)

- **Goal:** the carpet's three modes and the endless bag.
- **Changes:**
  - `add-endless-bag`,
  - `add-carpet-cloak` (insulation, camouflage via the visibility model, glide),
  - `add-carpet-tent` (pocket space, seamless entry, rooms, vault link),
  - `add-carpet-condition` (integrity, wetness, repairs, upgrade slots).
- **Exit demo:** camouflage past a wary herd, survive a blizzard in the cloak, pitch the
  tent in the storm, step inside into warm rooms, store goods in the vault and take
  them out through the bag elsewhere.

### M7: The Disciplines (XL)

- **Goal:** alchemy, artificing, ritual magic, summoning and the discipline progression
  system.
- **Changes:**
  - `add-chemistry` (substances, reactions, processes),
  - `add-alchemy-lab`,
  - `add-crafting` (workbench, materials),
  - `add-artificing` (enchanting, triggers, power sources),
  - `add-ritual-magic` (circles, timing, regional effects, blight),
  - `add-summoning` (elementals, familiars, contracts, upkeep),
  - `add-discipline-progression` (practice, study, trials).
- **Exit demo:** brew a healing draught from gathered herbs, enchant a lantern of
  perpetual light, perform a rain ritual over a drought-struck valley, summon a water
  elemental from a lake, and bond a familiar.

### M8: Places and People (XL), then Vertical Slice B

- **Goal:** peoples and their history, settlements, enterable architecture, citadels,
  dungeons and NPCs.
- **Changes:**
  - `add-peopling-and-cultures` (style families, cultures, languages, faiths, polities,
    the *Settled world* variant with a reconstructed chronicle, the history database),
  - `add-lore-generation` (chronicles told from each culture's perspective, legends,
    inscriptions, place names, NPC knowledge, Codex history),
  - `add-settlements` (siting, layout, land use),
  - `add-building-generation` (program, massing, floor plans, kit pieces, furnishing, lazy
    detail),
  - `add-castles`,
  - `add-caves-and-dungeons` (voxel volumes, graph grammars, simulation puzzles),
  - `add-npcs` (generation, schedules, dialogue, services),
  - `add-factions-and-economy`,
  - `add-structural-integrity`,
  - `add-history-simulation` (the optional *Rise and fall* variant: societies, wars,
    faiths, collapse over thousands of years, written into the same history database).
    It is planned last in M8 and may slip past M9 without blocking anything, because
    every consumer reads the shared history database.
- **Vertical Slice B:** the prologue in Anarvan and Act I through to leaving the
  valley. This needs the set-piece template support from `add-building-generation`.
- **Exit demo:** fly into a generated oasis city and enter any house. Rest in a
  caravanserai, explore a citadel of 100+ rooms, and read two chronicles that tell the
  same war from opposite sides. Trade and get directions, and descend into a karst cave
  with an underground river.

### M9: The Story (XL)

- **Goal:** the complete narrative and the territory game.
- **Changes:**
  - `add-narrative-director` (beat graph, site anchoring, rumours, backstory written into
    the history database at world creation),
  - `add-story-act-2` (Circle remnants, Great Weaves),
  - `add-rival-mages-and-domains` (sanctums, servants, contests),
  - `add-enemy-campaign` (global destruction timeline, blight),
  - `add-story-act-3-and-endings`,
  - `add-post-game-world-state`.
- **Exit demo:** the full story can be completed from start to each ending. An automated
  "story bot" scenario plays through all beats headlessly on fixed seeds.

### M10: Release (L)

- **Goal:** platform completeness, accessibility, modding and 1.0.
- **Changes:**
  - `add-webxr-play` (3D gestures, controller flight, comfort options, XR UI),
  - `add-accessibility`,
  - `add-localisation` (English and German),
  - `add-modding-sdk` (API docs, example mod, mod manager UI),
  - `add-release-pipeline` (signing where applicable, store-agnostic packages, web
    deploy),
  - the performance and polish pass.
- **Exit:** 1.0 release candidates on all platforms, with the documented example mod
  working.

---

## 6. Capability map (target structure of `openspec/specs/`)

| Domain | Capabilities (spec folders, created by the changes above) |
| --- | --- |
| Engine | `platform-targets`, `game-loop`, `asset-packages`, `lua-scripting`, `input-actions`, `rendering-core`, `audio-playback`, `test-automation`, `content-licensing`, `world-persistence`, `modding` |
| World | `planet-coordinates`, `deep-time-geology`, `climate-and-biomes`, `world-creation`, `chunk-streaming`, `terrain`, `water-bodies`, `vegetation`, `sky-and-time`, `weather` |
| Simulation | `materials`, `thermal-and-fire`, `shallow-water`, `air-and-gases`, `simulation-tiers`, `rigid-bodies`, `terrain-deformation`, `chemistry`, `ecology`, `fauna-behaviour`, `small-life`, `structural-integrity` |
| Magic | `mana`, `weave-grammar`, `gesture-recognition`, `spellbook`, `alchemy`, `artificing`, `ritual-magic`, `summoning`, `discipline-progression` |
| Player | `carpet-flight`, `carpet-cloak`, `carpet-tent`, `carpet-condition`, `endless-bag`, `survival`, `codex`, `harvest-and-cooking`, `crafting`, `damage-model` |
| People and places | `peopling`, `cultures`, `languages`, `faiths`, `history-database`, `lore`, `history-simulation`, `settlements`, `building-generation`, `castles`, `caves-and-dungeons`, `npcs`, `factions-and-economy` |
| Story | `narrative-director`, `story`, `rival-mages`, `enemy-campaign` |
| Presentation | `retro-renderer`, `touch-controls`, `gamepad-controls`, `webxr`, `accessibility`, `localisation` |

---

## 7. Key decisions (with alternatives considered)

| # | Decision | Alternatives | Rationale |
| --- | --- | --- | --- |
| D1 | **Cube-sphere planet**, 2,000 km default radius | Infinite plane (Minecraft-like); torus wrap | A plane can't have consistent climate bands and poles; a torus distorts badly. A sphere of this size is endless in practice. The cost is more complex coordinates, solved with chunk addressing and a floating origin |
| D2 | **Heightfield terrain + sparse voxel volumes** | Full voxel world | Full voxels at 1 m over mountains several km high use enormous memory and draw calls. A heightfield is compact and fits the retro look, and voxels only appear where needed (caves, overhangs, floating islands) |
| D3 | **OpenGL 3.3 core / ES 3.0 common subset** | SDL_GPU (Vulkan/Metal/D3D12), WebGPU | Required by the brief, and reaches all targets including WebGL 2 and WebXR today. SDL_GPU has no web backend yet. The frontend/backend split keeps a later port possible |
| D4 | **All simulation on the CPU** (jobs) | GPU compute | No compute shaders on macOS GL 4.1 or WebGL 2. CPU simulation is also easier to make deterministic and to test |
| D5 | **Lua 5.4** | LuaJIT, a custom language, WASM scripting | LuaJIT can't JIT on WASM or iOS and lags behind 5.4 semantics. Lua 5.4 is portable C that compiles everywhere. Performance comes from batch APIs and keeping hot loops in C |
| D6 | **SDL3 main callbacks on every platform** | Classic `main()` loop + `emscripten_set_main_loop` | One code path that suits the browser and WebXR frame driving |
| D7 | **Asynchronous VFS** | Synchronous file reads | The web can't block on file reads, and lazy streaming wants async anyway |
| D8 | **Deterministic fixed-step simulation, counter-based RNG, strict float rules** | Variable step, stateful RNG | Portable seeds and saves, replays, and reproducible tests and bugs |
| D9 | **Delta-only saves + immutable generator versions** | Saving generated chunks | Saves stay small for an endless world. Determinism makes regeneration exact, and versioning protects existing worlds from generator changes |
| D10 | **Unity + gcovr + LuaCov + Lua scenario tests + Playwright** | CMocka, Criterion, busted | Small, MIT-licensed, portable (also on MSVC and WASM). Scenario tests use the real engine headless |
| D11 | **Web: pthreads build + single-thread fallback** | Single-thread only | Worldgen and simulation need parallelism. The fallback keeps the game hostable anywhere |
| D12 | **glTF, PNG, OGG, `.vox`, Lua data** | Custom binary formats | Standard tools (Blender, MagicaVoxel, Audacity) let contributors work without custom tooling |
| D13 | **Time-stepped deep-time tectonics** (≈ 500 Myr) with Earth-likeness steering | A static plate snapshot plus noise | Only a real history produces old and young ranges, sutures, rift lakes, fossils, strata and minerals in plausible places. Steering keeps results Earth-like |
| D14 | **World bundle saved with each world** | Recompute from the seed on every load | Long chaotic simulations are expensive and fragile to recompute. Storing 10–30 MB per world keeps saves valid even when the simulations are later tuned |
| D15 | **Two peopling variants with one output schema** (*Settled world* default, *Rise and fall* optional) | Only one variant | Fast creation by default and deep emergent history for those who want it. One schema means every downstream system works with both |
| D16 | **Oriental setting through style families; fictional cultures and faiths** | Real cultures and religions; a generic fantasy setting | Matches the flying-carpet fantasy while staying respectful and free to simulate history without depicting real peoples or religions |

---

## 8. Risks and mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| **Scope**: the concept is enormous | Never shipping | Strict milestone exits with playable demos, vertical slices early, and every system built minimal first, then deepened. Many systems are data-driven, so content can grow after the code lands |
| Web and mobile performance | Unplayable on phones | Per-platform budgets from M1, a mobile profile (smaller active radius, lower resolution), early perf tests in CI, and the retro style keeps GPU load low |
| Cross-platform determinism breaks | Seeds and saves differ by platform | `core/dmath`, no FMA contraction, and the CI determinism job from M1 on every push |
| WebXR integration with the main loop | Late architecture rework | M1 spike `add-webxr-session`; a view abstraction from M0; XR emulation in CI |
| 95 % coverage on GL and platform code | The gate blocks progress | Thin platform layer, frontend/backend split, llvmpipe image tests, a small reviewed exclusion list |
| Simulation runaway (fires, floods, population explosions) | Broken worlds | Tiered simulation, natural brakes, caps, invariant tests, soak tests |
| Lua performance | Frame spikes | No per-cell Lua, batch APIs, profiling markers, instruction budgets in dev builds, worker Lua states for generation |
| Procedural sameness | Boring world | Layered generation with real causes, cultures and history, magical overlays, authored set pieces placed procedurally |
| Art volume (CC BY 4.0, original) | Content bottleneck | Low-res retro style, palette swaps per biome and culture, modular kits, procedural texture variation, community contributions through the package system |
| Save bloat over long play | Large saves, slow loads | Delta storage, region compaction, pruning of trivial deltas (e.g. regrown grass) |
| World creation too slow on web and mobile | Players wait minutes before playing | "Quick" quality preset, work on the job system, an entertaining time-lapse, cancellation, and the stored world bundle so creation happens only once |
| Degenerate geology or history (one supercontinent with no mountains, one empire conquering everything, everyone dying of plague) | Boring or broken worlds | Earth-likeness metrics with bounded deterministic retries; stabilisers in the history simulation (overextension costs, fragmentation, recovery); a nightly corpus of seeds checked against acceptance metrics |
| Cultural insensitivity in an Oriental setting | Hurtful content, reputational damage | Fictional peoples and faiths, documented real-world references per style family, no ethnic villain tropes, and review by people from the inspiring cultures where possible |
| OpenGL deprecation on macOS | Future breakage | Backend boundary; evaluate SDL_GPU once it reaches the web, or a translation layer |

---

## 9. Open questions for the project owner

These don't block M0. They should be answered before the milestone listed in brackets.

1. **Protagonist aging** (before M5): keep optional aging over in-game years?
2. **Art production** (before M2): who creates the art? Options are the project owner,
   contributors, commissioned artists or procedural-first. This affects the pace of M2
   and later milestones.
3. **Native mobile apps** (before M10): SDL3 supports Android and iOS. Should store builds
   follow 1.0, or does mobile stay web-only?
4. **Multiplayer** (anytime): out of scope for 1.0. Should co-op be planned as a post-1.0
   goal? The deterministic simulation keeps it feasible.
5. **Minimum hardware** (before M1): proposed is a 4-core CPU, 8 GB RAM and an
   integrated GPU from 2018 or later (desktop), and a mid-range 2021+ phone (web).
6. **Right-to-left languages** (before M10): should Persian, Arabic or Turkish
   translations be planned for 1.0? Right-to-left scripts need text shaping (see concept
   §8.7).

**Resolved:**

- The world is a **cube-sphere planet** (D1), confirmed by the project owner.
- The setting is **Oriental**. The placeholder names were replaced accordingly (Anarvan,
  Farrokh, Azhdar), and the title stays *Zauberer* as the working title.
- World creation simulates **deep-time geology**, then peoples the planet either directly
  (*Settled world*, default) or through a simulation of rising and falling societies
  (*Rise and fall*, optional). Both produce cultures, languages, faiths and history
  (D13–D15).
