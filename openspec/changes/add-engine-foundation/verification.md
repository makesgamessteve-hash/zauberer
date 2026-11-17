<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Verification notes: add-engine-foundation (M0)

Results of running the M0 exit demo and the test levels. "CI" means the GitHub Actions
workflow (`.github/workflows/ci.yml`); "local" means a Linux x86-64 development container
(Ubuntu 24.04, GCC 13, Clang 18, Mesa 25.2 llvmpipe, Emscripten 4.0.23, Chromium via
Playwright 1.63).

## Exit demo

| Platform | How it was run | Result |
| --- | --- | --- |
| Linux desktop | local, `build/dev/bin/zauberer` with the SDL offscreen video driver on llvmpipe (GL 4.5 core) | Demo runs (carpet, motes, HUD text, music, input actions), first frame presented, exits 0; rendered goldens match |
| Desktop web | local, headless Chromium (SwiftShader WebGL 2), single-threaded and threaded builds | Both builds start, render and pass the browser tests; WebGL output matches the desktop golden image |
| Mobile web | local, Chromium with Pixel 7 emulation | Starts in touch mode, on-screen controls set up, frames render without console errors |
| Windows | CI `windows-msvc` (MSVC 2022, release preset) | Builds without warnings; unit, headless engine (incl. the replay with recorded hashes) and tool tests pass; the windowed test is skipped (the runner has no OpenGL 3.3 driver) |
| macOS | CI `macos` (universal arm64 + x86-64, release preset) | Not yet run: the hosted macOS runner was never assigned to the job (queued on every run) |

Interactive play on Windows, macOS and physical phones has not been done yet.

## CI (GitHub Actions, run 6)

| Job | Result |
| --- | --- |
| lint (clang-format, reuse, OpenSpec, tool tests, layering, licenses, credits, spec coverage, API docs, luacheck, reproducible assets) | pass |
| linux-gcc-coverage (unit, engine, rendered on llvmpipe; C and Lua gates) | pass |
| linux-clang-asan (Clang ASan + UBSan, rendered tests excluded) | pass |
| linux-render (release; goldens and the README commands) | pass |
| windows-msvc | pass |
| web (web, web-st, web-node builds; Node.js tests; Playwright) | pass |
| macos | waiting for a runner |
| determinism | waits for macos; the replay test already checks the committed hashes on Linux, Windows and WebAssembly |

## Test levels (local)

| Level | Result |
| --- | --- |
| Unit tests (Unity, 20 executables incl. leak checks) | pass in `dev`, `coverage`, `asan` (GCC ASan/UBSan) and `web-node` |
| Engine tests (`tests/engine`): 25 headless, 9 rendered | pass (headless also under Node.js in `web-node`) |
| Lua tests (`tests/lua`, 13 cases) | pass, JUnit XML written |
| Browser tests (`tests/web`, 9) | pass |
| Tool tests (`tests/tools`) | pass |
| Line coverage | C 97.3 %, Lua 98.1 % (gate 95 %, baseline 97.0 % / 98.0 %) |
| Spec scenario coverage | 85 / 85 scenarios tagged |
| Replay determinism | `tests/replays/demo.zbrec` reproduces its recorded state hashes natively and in WebAssembly (Node.js) |
| `reuse lint`, `check_licenses`, `check_credits`, `check_includes`, `openspec validate --all --strict`, `luacheck`, clang-format | pass |
| Generated assets | byte-identical when regenerated (`tools/gen/gen_all.py`) |
