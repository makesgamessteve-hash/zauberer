# Tasks

## 1. The grammar

- [x] 1.1 `data/weaves.lua`: verbs, colours and exchange rates, forms, modifiers, named weaves
- [x] 1.2 `magic/weave.lua`: validation, relations, keys, names, words, extent; verify "weave-grammar: Words, colours, forms and modifiers"
- [x] 1.3 Cost; verify "The cost of a weave"
- [x] 1.4 Familiarity, control, misfire chance and deterministic resolution with misfires; verify "Control and misfires"
- [x] 1.5 `magic/mage.lua`; verify "The mage's mana"

## 2. Through the simulation

- [x] 2.1 `zb_fire_cool` and `sim.cool`; verify "thermal-and-fire: Heat sinks for magic"
- [x] 2.2 `zb_surface_water_remove`, `_freeze`, `_thaw`, melting, ice in the water view, `sim.remove_water`, `sim.freeze`, `sim.thaw`; verify "shallow-water: Water for magic"
- [x] 2.3 `magic/effects.lua` and `magic/cast.lua`; scenario `weaves.lua`; verify "weave-grammar: Weaves through the simulation"
- [x] 2.4 Regenerate the Lua API reference; lint; checks (unit, headless, rendered, perf)
