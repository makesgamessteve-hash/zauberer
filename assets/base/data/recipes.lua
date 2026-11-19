-- SPDX-License-Identifier: CC-BY-4.0
-- Alchemy's recipes (concept 3.4): what goes into the vessel and the steps
-- of the apparatus; the chemistry decides what comes out (data/reactions.lua).
--   needs   item = count taken from the pack
--   steps   { "grind" }, { "heat", deg C, hours }, { "filter" },
--           { "charge", colour, mana from the mage }, { "reduce", kg of water left },
--           { "distil", deg C } (what boils below it is kept, the rest thrown out)
--   yields  the item it is for (all that bottles comes out)
return {
  { id = "healing_draught", name = "Healing draught", needs = { healing_herbs = 10, boiled_water = 1 },
    steps = { { "grind" }, { "heat", 60, 3 }, { "filter" }, { "charge", "life", 10 }, { "heat", 80, 1 },
      { "reduce", 0 } },
    yields = "healing_draught" },
  { id = "astringent_tincture", name = "Astringent tincture", needs = { oak_bark = 4, spirit = 2 },
    steps = { { "grind" }, { "heat", 60, 4 }, { "filter" } },
    yields = "astringent_tincture" },
  { id = "spirit", name = "Spirit of wine", needs = { dates = 100, boiled_water = 2 },
    steps = { { "heat", 28, 48 }, { "distil", 85 } },
    yields = "spirit" },
  { id = "light_quintessence", name = "Quintessence of light", needs = { spirit = 2 },
    steps = { { "charge", "light", 30 }, { "heat", 60, 2 } },
    yields = "light_quintessence" },
}
