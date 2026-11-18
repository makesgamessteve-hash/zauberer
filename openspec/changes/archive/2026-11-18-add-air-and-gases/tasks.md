# Tasks

## 1. Air

- [x] 1.1 `sim/active` takes a resolution per grid; fire and water keep 128
- [x] 1.2 `sim/air`: local wind from the ground and canopy; verify "air-and-gases: The ground shapes the wind"
- [x] 1.3 Gases: semi-Lagrangian transport, spreading, losses, sleep; verify "Gases drift downwind and fade" and "Deterministic air"; `perf_air`; verify "Air budget"

## 2. Coupling and game

- [x] 2.1 Fire leans with the local wind and releases smoke; verify "thermal-and-fire: Fire smokes and follows the local wind"
- [x] 2.2 Engine: run the air, wind from the weather, smoke from fire, smoke puffs drawn (generated art)
- [x] 2.3 Lua `sim.wind_at`, `sim.gas_at`, `sim.emit`, `sim.air_stats`; scenario; verify "Air from Lua"
- [x] 2.4 Tag every scenario, regenerate the Lua API reference, keep coverage ≥ 95 %
