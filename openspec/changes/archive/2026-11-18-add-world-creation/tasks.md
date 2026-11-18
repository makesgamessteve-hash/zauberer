# Tasks

## 1. Pipeline

- [x] 1.1 `world/creation`: stages, bounded pieces, progress and attempts; verify "world-creation: Creation in stages"
- [x] 1.2 Cancellation that frees everything; verify "Cancelling"
- [x] 1.3 Options, quality presets and validation; verify "Options and quality"
- [x] 1.4 Time-lapse frames (elevation above the era's sea level, plates)

## 2. The world bundle

- [x] 2.1 `core/deflate` (zlib through stb)
- [x] 2.2 `world/bundle`: build from geology and climate, queries by direction (smooth heights across cube edges); verify "The world bundle"
- [x] 2.3 Versioned file with compressed, checksummed sections; verify "Files survive the round trip" and "Damaged files are refused"
- [x] 2.4 Golden hash natively and in WebAssembly; verify "Deterministic worlds"

## 3. Terrain v2

- [x] 3.1 Heights from the bundle with hills and crags; materials by biome; noise world unchanged; verify "Terrain from the created world"

## 4. Game

- [x] 4.1 Lua `world.create`, `create_step` (worker job), `create_cancel`, `create_finish`, `load`, `info`; engine swaps the played world; verify "A new world from the game" and "Saved worlds come back"
- [x] 4.2 Lua place queries: `biome`, `climate`, `strata`, `minerals`, `province`; verify "Places know their past"
- [x] 4.3 The new-world screen and the time-lapse globe (`world.draw_globe`); verify "The time-lapse globe"
- [x] 4.4 Map every scenario to a tagged test, regenerate the Lua API reference, and keep coverage ≥ 95 %
