-- SPDX-License-Identifier: CC-BY-4.0
-- Artificing's enchantments (concept 3.4): a base object, a bound weave, a
-- power source and a trigger; or an upgrade woven into the carpet or bag.
--   base      an item (consumed into the artifact), "carpet" or "bag"
--   weave     { verb, colors, form }: the weave bound into it
--   source    "ambient" (draws from the mana field), "wearer" (from the mage),
--             "crystal" (its own charge only)
--   trigger   "passive" (always on while charged)
--   capacity  mana it holds; drain: mana a second while working
--   cost      colour = mana to enchant (times 1.5 less the base's conductivity)
--   needs     item = count also used
--   upgrade   carpet upgrade woven in (data/carpet_upgrades.lua); folds: bag folds added
return {
  { id = "lantern_of_light", name = "Lantern of perpetual light", base = "lantern",
    weave = { verb = "create", colors = { "light" }, form = "circle" }, source = "ambient", trigger = "passive",
    capacity = 30, drain = 0.01, cost = { light = 20 } },
  { id = "weatherproof_weave", name = "Weatherproof weave", base = "carpet", upgrade = "weatherproof",
    cost = { water = 12, storm = 6 }, needs = { resin = 2 } },
  { id = "fire_ward_lining", name = "Fire ward lining", base = "carpet", upgrade = "fire_ward",
    cost = { fire = 10, water = 10 }, needs = { hide = 1 } },
  { id = "swift_weave", name = "Swift weave", base = "carpet", upgrade = "swift_weave", cost = { storm = 15 },
    needs = { cord = 1 } },
  { id = "silent_glide", name = "Silent glide", base = "carpet", upgrade = "silent_glide",
    cost = { void = 10, storm = 5 }, needs = { cord = 1 } },
  { id = "folding_knot", name = "Folding knot", base = "bag", folds = 6, cost = { void = 20 }, needs = { cord = 2 } },
  { id = "ash_ward", name = "Ash ward", base = "staff",
    weave = { verb = "ward", colors = { "fire", "decay" }, form = "circle" }, source = "ambient", trigger = "passive",
    capacity = 40, drain = 0.005, cost = { water = 25, light = 15 } },
}
