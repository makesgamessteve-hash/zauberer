# Tasks

## 1. Noise

- [x] 1.1 Implement `world/noise` (gradient noise, fractal sum, ridged multifractal) with unit tests for range, continuity and determinism

## 2. Terrain model and stages

- [x] 2.1 Implement the continent model v1 and the sea-level fit; verify "terrain-generation: Land fraction" and "Ocean and mountains"
- [x] 2.2 Implement stages 0–2 with the Catmull-Rom macro interpolation and the 2 m detail grid; verify "Stage dependencies", "Stages agree", "Shared edges" and "Footprint by stage"
- [x] 2.3 Implement the surface rule; verify "Material rules" and "Materials in a generated chunk"
- [x] 2.4 Verify determinism: "Same seed, same terrain" (worker threads), "Different seeds differ", "Two peers" and the golden "Golden worldgen hashes" on Linux, Windows and web-node

## 3. Budget, scripting and integration

- [x] 3.1 Add the release-build perf test for "Measured time" and run it in the Linux release CI job
- [x] 3.2 Expose `world.height` and `world.surface` to Lua; verify "Height and surface from Lua"; regenerate the API reference
- [x] 3.3 Keep C coverage ≥ 95 % and map every scenario to a tagged test (`tools/spec_coverage.py`)
