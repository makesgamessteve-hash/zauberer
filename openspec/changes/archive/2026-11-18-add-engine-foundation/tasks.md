# Tasks

## 1. Repository and build scaffolding

- [x] 1.1 Create `CMakeLists.txt`, `CMakePresets.json` (`dev`, `release`, `coverage`, `asan`) and `cmake/` helpers; verify `cmake --preset dev && cmake --build --preset dev` produces a `zauberer` executable that exits 0
- [x] 1.2 Pin SDL3, Lua 5.4, stb, cgltf and Unity by URL + SHA-256 (or vendor into `third_party/` with license files) and record their versions and licenses in `THIRD_PARTY_NOTICES.md`; verify a clean configure fetches and builds all of them on Linux, Windows and macOS
- [x] 1.3 Add warning and floating-point flags (`-Wall -Wextra -Wpedantic -Wconversion -Werror`, `/W4 /WX`, `-ffp-contract=off`, `/fp:precise`, no fast-math); verify the flags appear in `compile_commands.json` for every target
- [x] 1.4 Add the Emscripten presets `web`, `web-st` and `web-node` with the emsdk version pinned in `cmake/emsdk-version.txt`; verify each preset produces its HTML/JS/WASM (or Node) artifacts
- [x] 1.5 Add `REUSE.toml` mapping code to MIT and assets/docs to CC-BY-4.0 (the license texts are already in `LICENSES/`); verify `reuse lint` passes on the repository
- [x] 1.6 Add `.github/workflows/ci.yml` with the `lint` job (clang-format check, `reuse lint`, `openspec validate --all --strict`); verify the job passes on a push to the development branch

## 2. Test machinery

- [x] 2.1 Integrate Unity and add the `zb_add_unit_test()` CMake helper; verify a deliberately failing sample test makes `ctest` fail and report file, line and test name, then remove the sample
- [x] 2.2 Add `tools/spec_coverage.py` (collects scenario names from `openspec/specs` and active changes, searches `tests/` for `spec: <capability>: <scenario>` tags) with its own Python unit tests; verify it lists every scenario of this change as uncovered at this point
- [x] 2.3 Add the `coverage` workflow job with gcovr (`--fail-under-line 95`), `tools/coverage-exclusions.txt` with the justification and ≤ 1 % checks, and the `tools/coverage-baseline.json` ratchet, each with Python unit tests; verify the job fails on a branch whose coverage is artificially lowered
- [x] 2.4 Add `tools/check_includes.py` that enforces the module dependency rules; verify it fails on a deliberate forbidden include in a scratch commit

## 3. Core library (`src/core`)

- [x] 3.1 Implement the allocator interface, arenas and the tracking allocator, with a teardown leak check wired into `zb_add_unit_test()`; verify unit tests including "test-automation: Leaking test"
- [x] 3.2 Implement dynamic arrays, hash maps and string interning; verify unit tests cover growth, collisions, removal and out-of-memory paths
- [x] 3.3 Implement vector, matrix and quaternion math; verify unit tests against reference values
- [x] 3.4 Implement `zb_hash` and the counter-based `zb_rng` with fixed test vectors; verify identical vectors on the Linux, Windows, macOS and `web-node` CI jobs
- [x] 3.5 Implement logging with levels, sinks and once-per-key warnings; verify unit tests
- [x] 3.6 Implement the job system (worker pool, priorities, cancellation, inline single-thread mode); verify unit tests pass in threaded and inline modes, including in the `asan` job
- [x] 3.7 Verify `src/core` reaches ≥ 95 % line coverage in the coverage job

## 4. Platform layer and game loop

- [x] 4.1 Define the platform interface and implement it with SDL3 (window, GL context, base path, user data path, events), plus a stub headless implementation; verify unit tests against the stub and a windowed smoke run on all desktop CI jobs
- [x] 4.2 Implement the game loop with an injectable clock: fixed 60 Hz ticks, an 8-tick catch-up clamp, interpolation, pause and time scale; verify unit tests for "game-loop: Low frame rate", "High frame rate", "Two-second stall", "Frame between ticks" and "Paused game keeps rendering"
- [x] 4.3 Implement the SDL3 main-callback entry and the SDL-free headless entry with `--headless` and `--ticks N`; verify the scenario test "game-loop: Run a fixed number of ticks"
- [x] 4.4 Implement command-line parsing (`--assets`, `--user-dir`, `--headless`, `--ticks`, `--test`, `--scenario`, `--record`, `--replay`, `--debug-views`, `--lua-coverage`); verify unit tests for valid, missing and malformed arguments
- [x] 4.5 Implement startup errors for a missing content folder and an insufficient GL version (message box, web page message, non-zero exit); verify the scenario tests "platform-targets: Missing content folder", "Content path override" and a unit test with a faked GL version for "Unsupported graphics context"
- [x] 4.6 Implement resize and fullscreen handling with drawable-size events, and orderly shutdown in reverse package order; verify the rendered scenario tests "Resize at runtime" and "Script requests quit"
- [x] 4.7 Implement per-OS user data directories and IDBFS on the web; verify the tests "Data persists across runs" (desktop and Playwright) and "Override for tests"

## 5. Packages and virtual file system (`src/vfs`)

- [x] 5.1 Implement manifest parsing in a restricted Lua state (literals only, instruction cap); verify unit tests for "asset-packages: Valid packages are found" and "Invalid manifest"
- [x] 5.2 Implement discovery and dependency ordering (stable Kahn order, missing dependencies, cycles, API compatibility); verify unit tests for "Dependent mod loads after base", "Missing dependency", "Dependency cycle" and "Package targets a future API"
- [x] 5.3 Implement overlay lookup, merged listings and path normalisation; verify unit tests for "Mod overrides a texture", "Merged listing" and "Escape attempt"
- [x] 5.4 Implement the asynchronous request API on desktop workers, generated placeholders and once-per-path warnings; verify unit tests for "Load completes later" and "Missing texture"
- [x] 5.5 Implement `tools/make_asset_index.py`, the web fetch bridge with Cache Storage, and boot-set prefetching; verify the Playwright tests "Second visit uses the cache" and "Updated asset"

## 6. Lua host (`src/script`)

- [x] 6.1 Build Lua 5.4 with the engine allocator, the library whitelist and a deterministic `math.random`; verify the Lua tests "lua-scripting: Forbidden call", "Binary chunk rejected" and "Repeatable randomness"
- [x] 6.2 Implement per-package environments, `require` through the VFS and entry scripts in load order; verify the scenario tests "Entry scripts run in order" and "Require a package module"
- [x] 6.3 Implement hook registration and dispatch with traceback-based error isolation and the development on-screen error overlay; verify the scenario tests "Tick hook runs per tick" and "Failing tick hook"
- [x] 6.4 Implement the instruction budget and polling hot reload (development builds only); verify the scenario tests "Infinite loop in a hook" and "Edit a script while running"
- [x] 6.5 Implement `engine.info`, `engine.quit`, `engine.hash_state`, `engine.packages` and the key-value `save` API; verify the Lua test "Query platform" and unit tests for the canonical state hash
- [x] 6.6 Extract API annotations into `docs/api/` and `zauberer.d.lua`; verify a CI step fails if the generated files are out of date

## 7. Input (`src/input`)

- [x] 7.1 Implement action definitions and per-tick snapshots; verify the unit test "input-actions: Button held across ticks"
- [x] 7.2 Implement default bindings from package data and persisted user overrides; verify the tests "Mod changes a default binding", "Rebinding survives restart" and "Stale configuration entry"
- [x] 7.3 Implement gamepad support with hot-plugging; verify the unit test "Unplug while holding" with injected device events
- [x] 7.4 Implement touch-mode detection, switching and synthesised-mouse filtering; verify unit tests and the Playwright tests "Phone starts in touch mode", "Keyboard on a tablet" and "Synthesised mouse events"
- [x] 7.5 Implement virtual stick and button primitives with touch ownership; verify the unit test "Virtual stick drives an axis"
- [x] 7.6 Implement input recording and replay; verify the replay test "Replay reproduces actions" and add the first replay fixture under `tests/replays/`

## 8. Rendering (`src/render`)

- [x] 8.1 Add the GL 3.3 loader, WebGL 2 linkage, shader header injection and the version check; verify the shaders compile on llvmpipe and in headless Chromium
- [x] 8.2 Implement the frontend (views, frustum culling, sorted draw lists) and the null backend; verify the unit test "rendering-core: Draw list inspection"
- [x] 8.3 Implement the internal framebuffer and the integer-scaled nearest blit; verify the golden tests "Full HD window" and "Non-integer window size"
- [x] 8.4 Implement static glTF loading through cgltf with a placeholder on failure; verify the golden test "Render the carpet model" and the test "Corrupt model"
- [x] 8.5 Implement instanced, animated sprites; verify the test "Animated sprite"
- [x] 8.6 Implement bitmap-font text with a replacement glyph; verify the golden test "Missing glyph"
- [x] 8.7 Implement multi-view rendering and `--debug-views 2`; verify the golden test "Forced stereo"
- [x] 8.8 Add the `linux-render` CI job (llvmpipe via SDL offscreen + EGL, xvfb fallback) and the Playwright WebGL golden comparison; verify "Desktop and web agree"

## 9. Audio (`src/audio`)

- [x] 9.1 Implement the SDL3 audio stream output and the mixer (64 voices, groups, limiter); verify the unit tests "audio-playback: Many voices without clipping" and "Music volume group" on synthetic buffers
- [x] 9.2 Implement WAV decoding and incremental OGG streaming; verify the unit tests "Play and stop a loop" and "Long track"
- [x] 9.3 Implement the null device with tick-driven voices; verify the scenario test "Headless playback"
- [x] 9.4 Implement the web audio unlock bridge; verify the Playwright test "Music requested before interaction"

## 10. Licensing and credits

- [x] 10.1 Define the `credits.lua` format and implement `tools/check_credits.py` (coverage of asset files, dangling entries, license allowlist, exceptions) with Python unit tests; verify the tests "content-licensing: New sprite without credit" and "Incompatible third-party asset"
- [x] 10.2 Add the license-split check to `REUSE.toml` validation; verify the tests "File without license information" and "Asset declared with the wrong license"
- [x] 10.3 Implement `engine.credits()`, merging package credits and third-party notices; verify the scenario test "Mod assets appear in credits"

## 11. Base package and demo

- [x] 11.1 Add `tools/gen/` scripts that reproducibly generate the carpet `.glb`, pixel font, sprite sheet, WAV chime and OGG loop, plus their credits entries; verify that regenerating them yields byte-identical files
- [x] 11.2 Write `assets/base/package.lua` and `scripts/main.lua` for the demo (spinning carpet, sprite, text, sound, input on all devices, credits list); verify Lua tests and scenarios bring `assets/base/scripts` to ≥ 95 % line coverage
- [x] 11.3 Add fixture packages under `tests/fixtures/` (overriding mod, broken manifest, cyclic mods, future API, failing hook); verify they are used by the package and scripting tests

## 12. Integration

- [x] 12.1 Complete the CI matrix (`linux-gcc-coverage`, `linux-clang-asan`, `linux-render`, `windows-msvc`, `macos`, `web`, `determinism`); verify all jobs pass, including "test-automation: Platform-specific failure" and "Hash mismatch between platforms", by temporarily injected faults on a scratch branch
- [x] 12.2 Map every scenario in this change's specs to at least one tagged automated test; verify `tools/spec_coverage.py` reports zero uncovered scenarios
- [x] 12.3 Implement the web shell's isolation detection and build selection; verify the Playwright tests "Static host without isolation headers" and "Isolated host" against a test server with and without COOP/COEP
- [x] 12.4 Verify the remaining build-level scenarios: "Each preset produces a runnable artifact", "Normal start", "Mobile emulation" and the cross-platform hash scenario "Cross-platform hash"
- [x] 12.5 Document building, running, testing and package creation in `README.md` and `docs/modding/packages.md`; verify the documented commands run in the CI smoke step
- [ ] 12.6 Run the M0 exit demo on Windows, Linux, macOS, desktop web and mobile web, and record the results in the change's verification notes

## Workflow follow-up

- Review and merge the implementation, then archive the change with `openspec archive add-engine-foundation`.
- After archiving, verify that `openspec/specs/` contains the nine capabilities and that `openspec validate --all --strict` passes.

## Notes

- macOS is deferred by project decision (2026-11-17): tasks 1.2, 3.4, 12.1 and 12.4 were
  verified on Linux, Windows and the web builds; the macOS job was removed from CI until
  macOS is taken up again.
- 12.6 (manual run on real devices) stays open as a follow-up for the project owner; the
  change was archived with it open, by agreement.

