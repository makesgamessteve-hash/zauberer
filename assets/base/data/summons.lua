-- SPDX-License-Identifier: CC-BY-4.0
-- Summoning (concept 3.4): elementals and spirits the mage forms, and the
-- animals that can be bonded as familiars.
--   summons:   species (data/creatures.lua), needs ("water": a body of water within
--              reach metres; nil: nothing), cost and upkeep (colour = mana; upkeep
--              a second), will (how hard it pulls at its binding)
--   familiars: species, offering (item = count it takes), trait and what it does
return {
  summons = {
    { id = "water_elemental", name = "Water elemental", species = "water_elemental", needs = "water", reach = 20,
      cost = { water = 30 }, upkeep = { water = 0.05 }, will = 0.3 },
    { id = "wisp", name = "Wisp of light", species = "wisp", cost = { light = 12 }, upkeep = { light = 0.02 },
      will = 0.1 },
  },
  familiars = {
    { species = "fox", offering = { raw_meat = 1 }, trait = "keen_nose", text = "finds more when foraging" },
    { species = "hare", offering = { grass_seed = 1 }, trait = "quick_ears", text = "hears danger sooner" },
    { species = "crane", offering = { reed_roots = 1 }, trait = "sharp_eyes", text = "sees farther" },
  },
}
