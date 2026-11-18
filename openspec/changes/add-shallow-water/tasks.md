# Tasks

## 1. Active grid

- [x] 1.1 `sim/active`: chunks near the focus with the needed stage, neighbours across edges and seams, lookups and area walks
- [x] 1.2 `sim/fire` on the active grid; every fire test still passes

## 2. Surface water

- [x] 2.1 `sim/water`: pipe model with flux limiting; verify "shallow-water: Water runs downhill" and "A basin fills flat and goes still"
- [x] 2.2 Fixed water bodies drain; balance; verify "Water drains into the sea"
- [x] 2.3 Soaking by material and rain; verify "Rain soaks in or runs off"
- [x] 2.4 Sleep, hash, outside the tier; verify "Deterministic water"; `perf_surface_water`; verify "Water budget"

## 3. Game

- [x] 3.1 Engine: run at 20 Hz around the focus points, follow world changes, rain from `sim.set_weather`
- [x] 3.2 Terrain view: standing water meshes
- [x] 3.3 Lua `sim.add_water`, `sim.water_at`, `sim.water_stats`; scenario; verify "Water from Lua"
- [x] 3.4 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
