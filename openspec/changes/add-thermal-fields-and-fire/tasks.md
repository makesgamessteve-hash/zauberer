# Tasks

## 1. World

- [x] 1.1 Ash surface material (terrain, terrain view, registry); burnt edits in `world/delta` with healing by game time; edits indexed per chunk; verify "world-persistence: Burnt ground heals"
- [x] 1.2 `zb_world_burn`, `zb_world_focus`, deltas applied at the world's time

## 2. Simulation

- [x] 2.1 `sim/fire`: chunks of 2 m cells near the focus, ghost cells across edges and seams, ambient and moisture from the climate; verify "thermal-and-fire: Heat without fire"
- [x] 2.2 Ignition, burning, radiation leaning with wind and slope, scars; verify "Fire spreads and burns out" and "Wind and slope lean the fire"
- [x] 2.3 Water, bare ground, wet fuel and rain; verify "Water and rain stop fire"
- [x] 2.4 Embers and spot fires; verify "Embers start spot fires"
- [x] 2.5 Determinism; verify "Deterministic fire"; budget in `perf_fire`; verify "Fire budget"

## 3. Game

- [x] 3.1 Engine: run at 20 Hz around the focus points, follow world changes, draw flame sprites (generated art)
- [x] 3.2 Lua `sim.ignite`, `sim.add_heat`, `sim.fire_at`, `sim.fire_stats`, `sim.set_weather`, `sim.weather`; scenario; verify "Fire from Lua"
- [x] 3.3 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
