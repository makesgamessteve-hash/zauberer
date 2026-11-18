# Tasks

## 1. Rivers and lakes

- [x] 1.1 `world/water`: hydraulic geometry, channels from the macro graph, meanders, levels; verify "water-bodies: Channels follow their discharge", "A connected river network" and "Follow a river to the sea"
- [x] 1.2 Lake levels that never spill into an outlet; verify "Lakes at their fill level"

## 2. Terrain

- [x] 2.1 Carved channels, banks and valley fading in stages 0–2 and the analytic height; water materials; verify "Rivers carve their beds"
- [x] 2.2 Water queries per place and per chunk grid, currents; verify "Water at any place"
- [x] 2.3 Golden hash natively and in WebAssembly; budgets; verify "Deterministic water" and "Water budgets"

## 3. Rendering

- [x] 3.1 Water draw kind, render time, GL water pass and shaders
- [x] 3.2 Per-chunk water meshes in the terrain view (replacing the sea plane); verify "retro-rendering: Water at sea level" and "water-bodies: Water is drawn"

## 4. Lua and flight

- [x] 4.1 `world.water`, `world.find_water`, `render.stats().water_items`; verify "Water from Lua"
- [x] 4.2 The carpet floats on water and drifts with currents; verify "The carpet rides on water"
- [x] 4.3 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
