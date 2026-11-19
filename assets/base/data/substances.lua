-- SPDX-License-Identifier: CC-BY-4.0
-- The substances of chemistry (concept 5.11), era-plausible under their old
-- names. Goods bring them into a vessel (items' `contains`).
--   phase      "solid", "liquid" or "gas"
--   boils      deg C at which a liquid boils off (and is caught by distilling)
--   solvent    other matter steeps in it
--   dissolves  it stays in solution (filtering keeps it)
--   bottle     { item, kg }: bottled as that item, so many kg each
return {
  { id = "water", name = "Water", phase = "liquid", boils = 100, solvent = true, dissolves = true,
    bottle = { item = "boiled_water", kg = 0.5 } },
  { id = "spirit", name = "Spirit of wine", phase = "liquid", boils = 78, solvent = true, dissolves = true,
    bottle = { item = "spirit", kg = 0.1 } },
  { id = "sugar", name = "Sugar", phase = "solid", dissolves = true },
  { id = "yeast", name = "Wild yeast", phase = "solid" },
  { id = "herb_matter", name = "Herb matter", phase = "solid" },
  { id = "bark_matter", name = "Bark", phase = "solid" },
  { id = "spent_matter", name = "Spent matter", phase = "solid" },
  { id = "life_essence", name = "Essence of healing herbs", phase = "liquid", boils = 190, dissolves = true },
  { id = "tannin", name = "Tannin", phase = "solid", dissolves = true },
  { id = "healing_draught", name = "Healing draught", phase = "liquid", boils = 105, dissolves = true,
    bottle = { item = "healing_draught", kg = 0.05 } },
  { id = "astringent", name = "Astringent tincture", phase = "liquid", boils = 90, dissolves = true,
    bottle = { item = "astringent_tincture", kg = 0.1 } },
  { id = "light_quintessence", name = "Quintessence of light", phase = "liquid", boils = 300, dissolves = true,
    bottle = { item = "light_quintessence", kg = 0.1 } },
}
