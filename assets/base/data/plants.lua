-- SPDX-License-Identifier: CC-BY-4.0
-- The life cycles of the base game's plants (concept 5.12), by flora species
-- id (data/flora.lua). Seasons run 0 .. 4 from the start of the local spring
-- (0 spring, 1 summer, 2 autumn, 3 winter); each plant keeps its own timing
-- within a few days of its species'.
--   leaves, flowers, fruit: { from, to } in seasons (to may pass 4 into the next year)
--   parts: what can be taken from a plant
--     item     the item it gives (data/items.lua)
--     when     "fruit", "flowers" or "leaves": only while the plant bears them;
--              omitted: all year
--     yield    { least, most } for the smallest and the largest grown plant
--     regrow   game days until the part can be taken again from the same plant (a
--              year is 48 days: four seasons of 12)
--     whole    taking it takes the whole plant (felled, dug up), which grows
--              back as the land heals
return {
  oak = { leaves = { 0.3, 2.8 }, flowers = { 0.3, 0.6 }, fruit = { 2.0, 2.7 }, parts = {
    { item = "acorns", when = "fruit", yield = { 20, 120 }, regrow = 40 },
    { item = "oak_bark", yield = { 1, 3 }, regrow = 12 },
    { item = "wood", yield = { 40, 200 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  beech = { leaves = { 0.3, 2.8 }, flowers = { 0.3, 0.5 }, fruit = { 2.0, 2.6 }, parts = {
    { item = "beechnuts", when = "fruit", yield = { 20, 90 }, regrow = 40 },
    { item = "wood", yield = { 40, 200 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  spruce = { leaves = { 0, 4 }, flowers = { 0.4, 0.7 }, fruit = { 1.8, 2.8 }, parts = {
    { item = "resin", yield = { 1, 3 }, regrow = 8 },
    { item = "spruce_tips", when = "flowers", yield = { 5, 20 }, regrow = 40 },
    { item = "wood", yield = { 40, 220 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  fir = { leaves = { 0, 4 }, flowers = { 0.4, 0.7 }, fruit = { 1.8, 2.8 }, parts = {
    { item = "resin", yield = { 1, 3 }, regrow = 8 },
    { item = "wood", yield = { 40, 240 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  pine = { leaves = { 0, 4 }, flowers = { 0.4, 0.7 }, fruit = { 1.9, 3.0 }, parts = {
    { item = "pine_nuts", when = "fruit", yield = { 10, 60 }, regrow = 40 },
    { item = "resin", yield = { 1, 4 }, regrow = 8 },
    { item = "wood", yield = { 30, 180 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  birch = { leaves = { 0.2, 2.7 }, flowers = { 0.2, 0.4 }, fruit = { 1.4, 2.4 }, parts = {
    { item = "birch_bark", yield = { 1, 3 }, regrow = 12 },
    { item = "birch_sap", when = "flowers", yield = { 1, 4 }, regrow = 40 },
    { item = "wood", yield = { 20, 100 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  palm = { leaves = { 0, 4 }, flowers = { 0, 4 }, fruit = { 0, 4 }, parts = {
    { item = "coconut", when = "fruit", yield = { 2, 10 }, regrow = 6 },
    { item = "palm_fibre", yield = { 1, 3 }, regrow = 8 },
  } },
  date_palm = { leaves = { 0, 4 }, flowers = { 0.2, 0.6 }, fruit = { 1.6, 2.6 }, parts = {
    { item = "dates", when = "fruit", yield = { 20, 80 }, regrow = 40 },
    { item = "palm_fibre", yield = { 1, 3 }, regrow = 8 },
  } },
  acacia = { leaves = { 0, 4 }, flowers = { 0.8, 1.4 }, fruit = { 1.6, 2.6 }, parts = {
    { item = "acacia_gum", yield = { 1, 3 }, regrow = 8 },
    { item = "wood", yield = { 20, 100 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  cypress = { leaves = { 0, 4 }, flowers = { 0.1, 0.4 }, fruit = { 1.6, 2.8 }, parts = {
    { item = "resin", yield = { 1, 2 }, regrow = 8 },
    { item = "wood", yield = { 30, 160 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  kapok = { leaves = { 0, 4 }, flowers = { 3.2, 3.8 }, fruit = { 0.0, 0.6 }, parts = {
    { item = "kapok_fibre", when = "fruit", yield = { 4, 20 }, regrow = 40 },
    { item = "kapok_bark", yield = { 1, 3 }, regrow = 12 }, -- steeped, a remedy for fever
    { item = "wood", yield = { 80, 400 }, whole = true },
    { item = "deadwood", yield = { 1, 3 }, regrow = 4 }, -- fallen branches beneath it
  } },
  snag = { parts = { -- a dead tree: dry wood, and the fungi that live on it
    { item = "deadwood", yield = { 10, 60 }, whole = true },
  } },
  hazel = { leaves = { 0.3, 2.7 }, flowers = { 3.6, 4.2 }, fruit = { 1.8, 2.5 }, parts = {
    { item = "hazelnuts", when = "fruit", yield = { 10, 60 }, regrow = 40 },
    { item = "hazel_rods", yield = { 2, 8 }, regrow = 16 },
  } },
  saltbush = { leaves = { 0, 4 }, flowers = { 0.8, 1.2 }, fruit = { 1.2, 1.8 }, parts = {
    { item = "saltbush_leaves", when = "leaves", yield = { 3, 12 }, regrow = 4 },
  } },
  saguaro = { leaves = { 0, 4 }, flowers = { 0.6, 1.0 }, fruit = { 1.0, 1.5 }, parts = {
    { item = "cactus_fruit", when = "fruit", yield = { 4, 30 }, regrow = 40 },
  } },
  grass = { leaves = { 0.1, 3.0 }, flowers = { 0.8, 1.3 }, fruit = { 1.2, 1.9 }, parts = {
    { item = "grass_seed", when = "fruit", yield = { 1, 3 }, regrow = 40 },
    { item = "dry_grass", yield = { 1, 2 }, whole = true },
  } },
  fern = { leaves = { 0.2, 2.8 }, parts = {
    { item = "fiddleheads", when = "leaves", yield = { 2, 6 }, regrow = 40 },
  } },
  meadow_flowers = { leaves = { 0.2, 2.6 }, flowers = { 0.5, 1.8 }, fruit = { 1.8, 2.4 }, parts = {
    { item = "healing_herbs", when = "flowers", yield = { 1, 4 }, regrow = 3 },
  } },
  reeds = { leaves = { 0.2, 3.0 }, flowers = { 1.2, 1.8 }, fruit = { 1.8, 2.8 }, parts = {
    { item = "reed_roots", yield = { 1, 4 }, regrow = 5 },
    { item = "reed_fibre", yield = { 2, 6 }, whole = true },
  } },
}
