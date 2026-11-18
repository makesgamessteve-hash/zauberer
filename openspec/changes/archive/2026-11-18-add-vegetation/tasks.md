# Tasks

## 1. Species

- [x] 1.1 Plant atlas generator (`tools/gen/gen_flora.py`) and `sprites/flora.{png,lua}`
- [x] 1.2 `data/flora.lua` and `world/flora` (parsing, niches, cover); verify "vegetation: The species registry", "Broken registries are refused", "Plants grow where their climate suits them" and "Biomes carry their plants"

## 2. Placement

- [x] 2.1 Chunk climate (created world and noise world), stage 0 canopy, stage 3 plants; verify "Plants on every chunk near the player"
- [x] 2.2 The ecology ring in streaming, flora loaded by the engine
- [x] 2.3 Golden hash natively and in WebAssembly; budgets; verify "Deterministic plants" and "Vegetation budgets"

## 3. Rendering

- [x] 3.1 Foliage draw kind, instance buffers, per-species tints, GL pass and shaders
- [x] 3.2 Terrain view bands (near, mid, far clumps, canopy surface), seasons; verify "Four levels of forest", "Seasons colour the plants" and "Plants are drawn"

## 4. Lua

- [x] 4.1 `world.flora`, `world.plants`, foliage render stats; verify "Plants from Lua"
- [x] 4.2 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
