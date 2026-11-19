-- SPDX-License-Identifier: CC-BY-4.0
-- The reactions of chemistry (concept 5.11).
--   reactants, products  substance = proportion (kg)
--   temp, best           the range it runs in (deg C) and where it runs fastest
--   rate                 share of what can react that reacts an hour at best
--   solvent              only steeped in a solvent (as much as there is matter)
--   catalyst             only with this substance present
--   mana                 colour = mana per kg of products, charged into the vessel
return {
  { id = "maceration", reactants = { herb_matter = 1 }, products = { life_essence = 0.4, spent_matter = 0.6 },
    solvent = true, temp = { 10, 99 }, best = 60, rate = 0.6 },
  { id = "bark_extraction", reactants = { bark_matter = 1 }, products = { tannin = 0.2, spent_matter = 0.8 },
    solvent = true, temp = { 30, 110 }, best = 90, rate = 0.5 },
  { id = "healing_draught", reactants = { life_essence = 1, water = 2 }, products = { healing_draught = 3 },
    mana = { life = 40 }, temp = { 50, 99 }, best = 80, rate = 2 },
  { id = "astringent", reactants = { tannin = 1, spirit = 2 }, products = { astringent = 3 },
    temp = { 10, 75 }, best = 40, rate = 1 },
  { id = "fermentation", reactants = { sugar = 1 }, products = { spirit = 0.5, water = 0.5 }, catalyst = "yeast",
    temp = { 12, 38 }, best = 28, rate = 0.05 },
  { id = "quintessence", reactants = { spirit = 1 }, products = { light_quintessence = 1 }, mana = { light = 60 },
    temp = { 40, 77 }, best = 60, rate = 1 },
}
