# Tasks

## 1. Passes and the look

- [x] 1.1 Add the sky, terrain, fullscreen and post shaders and fog in the model and sprite shaders; render sky, terrain, models and sprites, then the palette pass, then text; verify "retro-rendering: Palette colours only" and "Palette off"
- [x] 1.2 Add the C reference functions for palette, sky and fog; verify "Ordered dithering", "Sky gradient and sun" and "Fog hides the distance"

## 2. Terrain

- [x] 2.1 Build chunk meshes per level with skirts and water in the chunk's frame; verify "Levels of detail", "No cracks between levels", "Water at sea level" and "Curvature"
- [x] 2.2 Draw loaded chunks with per-frame build budgets and release unloaded ones; verify "Mesh build budget"
- [x] 2.3 Move the render origin with the first focus; verify "Origin follows the player"
- [x] 2.4 Add the release perf test "Frame budgets"

## 3. Scripting and integration

- [x] 3.1 Expose `render.atmosphere`, `render.palette`, `world.to_local`, `world.from_local`, `world.up_at`, `world.view_distance` and terrain statistics; verify "Terrain scene" with a rendered scenario and golden image; regenerate the API reference
- [x] 3.2 Keep the M0 goldens unchanged with the palette and atmosphere off; keep C coverage ≥ 95 % and map every scenario to a tagged test
