# Tasks

## 1. Heights

- [x] 1.1 `world/delta`: `ZB_EDIT_HEIGHT` with validation; applying it reshapes the chunk and its height range; verify "terrain-deformation: Heights are kept as edits"
- [x] 1.2 `zb_world_deform`: a smooth bowl or mound across chunk borders, plants removed, edits kept; verify "Craters and mounds"

## 2. The active tier

- [x] 2.1 `zb_surface_water_ground_changed`, `zb_fire_ground_changed`, `zb_tier_ground_changed`; verify "A breached dam floods the valley"

## 3. Lua

- [x] 3.1 `world.deform`; scenario `deform.lua`; verify "Deformation from Lua"
- [x] 3.2 Regenerate the Lua API reference; lint; fast checks (unit, headless, rendered, perf)
