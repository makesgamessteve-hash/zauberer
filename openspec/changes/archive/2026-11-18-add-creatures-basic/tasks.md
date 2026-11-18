# Tasks

## 1. Creatures

- [x] 1.1 `sim/creatures`: species, spawning, moving on the ground, on ice and in the air, waiting outside the tier; verify "creatures: Species are data", "On the ground, in the air, out of the water"
- [x] 1.2 Senses, stimuli and the utility choice; verify "Fear of fire, hunters and the player", "Scent on the wind"
- [x] 1.3 Harm, healing, death and events; hash; verify "Harm and death", "Deterministic creatures"
- [x] 1.4 The tier owns, steps and times the creatures; perf test for 200 creatures

## 2. In the game

- [x] 2.1 `sim.*` creature functions; `data/creatures.lua`; scenario `creatures.lua`; verify "Creatures from Lua"
- [x] 2.2 `app/creature_view`; verify "Creatures are drawn"; the game spawns and draws creatures
- [x] 2.3 Regenerate the Lua API reference; lint; checks (unit, headless, rendered, perf)
