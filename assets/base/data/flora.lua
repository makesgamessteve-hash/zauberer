-- SPDX-License-Identifier: CC-BY-4.0
-- The plant species of the base game. Each grows where the climate suits it:
--   layer        "tree", "shrub" or "ground" (understory, drawn only near)
--   sprite       frame of sprites/flora.lua; clump: the impostor far forests use
--   height       metres, smallest and largest grown plant; width: sprite width / height
--   temperature  annual mean (deg C): lowest, best, highest
--   rain         annual precipitation (mm): least, best, most
--   frost        the coldest month's mean may not fall below this (deg C)
--   grows_on     surface materials; slope: steepest ground (tangent)
--   wet          only in wetlands and on river banks
--   density      relative abundance where it thrives
--   seasons      colour of the foliage in spring, summer, autumn and winter
--                (multipliers of the sprite's colours)
return {
  { id = "oak", name = "Oak", layer = "tree", sprite = "oak", clump = "clump_broadleaf", height = { 12, 24 },
    width = 0.75, temperature = { 3, 11, 19 }, rain = { 450, 900, 1800 }, frost = -25, grows_on = { "soil" },
    slope = 0.7, density = 1.0, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.35, 0.85, 0.5 }, { 0.7, 0.62, 0.55 } } },
  { id = "beech", name = "Beech", layer = "tree", sprite = "beech", clump = "clump_broadleaf", height = { 15, 30 },
    width = 0.7, temperature = { 5, 10, 16 }, rain = { 600, 1100, 2000 }, frost = -18, grows_on = { "soil" },
    slope = 0.75, density = 1.0, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.35, 0.85, 0.5 }, { 0.7, 0.62, 0.55 } } },
  { id = "spruce", name = "Spruce", layer = "tree", sprite = "spruce", clump = "clump_conifer", height = { 15, 35 },
    width = 0.45, temperature = { -8, 2, 10 }, rain = { 350, 800, 2000 }, frost = -45,
    grows_on = { "soil", "gravel" }, slope = 0.85, density = 1.2, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "fir", name = "Fir", layer = "tree", sprite = "fir", clump = "clump_conifer", height = { 15, 40 },
    width = 0.4, temperature = { -2, 5, 12 }, rain = { 700, 1400, 3000 }, frost = -30, grows_on = { "soil" },
    slope = 0.9, density = 1.0, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "pine", name = "Pine", layer = "tree", sprite = "pine", clump = "clump_conifer", height = { 10, 30 },
    width = 0.6, temperature = { -4, 8, 22 }, rain = { 250, 600, 1500 }, frost = -40,
    grows_on = { "soil", "sand", "gravel" }, slope = 0.9, density = 0.8, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "birch", name = "Birch", layer = "tree", sprite = "birch", clump = "clump_broadleaf", height = { 8, 20 },
    width = 0.55, temperature = { -6, 4, 12 }, rain = { 300, 700, 1500 }, frost = -45, grows_on = { "soil", "gravel" },
    slope = 0.8, density = 0.7, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.4, 1.2, 0.5 }, { 0.7, 0.62, 0.55 } } },
  { id = "palm", name = "Coconut palm", layer = "tree", sprite = "palm", clump = "clump_tropical", height = { 10, 22 },
    width = 0.7, temperature = { 21, 27, 32 }, rain = { 800, 2000, 4000 }, frost = 15, grows_on = { "sand", "soil" },
    slope = 0.4, density = 0.6, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "date_palm", name = "Date palm", layer = "tree", sprite = "date_palm", clump = "clump_dry",
    height = { 8, 20 }, width = 0.7, temperature = { 18, 25, 32 }, rain = { 50, 250, 700 }, frost = 3,
    grows_on = { "sand", "soil" }, slope = 0.3, wet = true, density = 1.5, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "acacia", name = "Umbrella acacia", layer = "tree", sprite = "acacia", clump = "clump_dry",
    height = { 5, 12 }, width = 1.1, temperature = { 18, 25, 32 }, rain = { 200, 600, 1200 }, frost = 5,
    grows_on = { "soil", "sand", "gravel" }, slope = 0.5, density = 0.5,
    seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1.1, 1.0, 0.75 }, { 1.1, 1.0, 0.75 } } },
  { id = "cypress", name = "Cypress", layer = "tree", sprite = "cypress", clump = "clump_conifer",
    height = { 10, 25 }, width = 0.3, temperature = { 10, 16, 22 }, rain = { 300, 650, 1200 }, frost = -8,
    grows_on = { "soil", "gravel" }, slope = 0.7, density = 0.5, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "kapok", name = "Kapok", layer = "tree", sprite = "tropical", clump = "clump_tropical", height = { 25, 50 },
    width = 0.8, temperature = { 20, 26, 32 }, rain = { 1300, 2600, 5000 }, frost = 14, grows_on = { "soil" },
    slope = 0.8, density = 1.3, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "snag", name = "Dead tree", layer = "tree", sprite = "snag", height = { 6, 15 }, width = 0.5,
    temperature = { -10, 5, 30 }, rain = { 100, 400, 3000 }, frost = -50, grows_on = { "soil", "gravel", "mud" },
    slope = 0.9, density = 0.04, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } } },
  { id = "hazel", name = "Hazel", layer = "shrub", sprite = "shrub", height = { 1.5, 4 }, width = 1.4,
    temperature = { 0, 9, 20 }, rain = { 400, 900, 2500 }, frost = -30, grows_on = { "soil" }, slope = 0.8,
    density = 1.0, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.35, 0.85, 0.5 }, { 0.7, 0.62, 0.55 } } },
  { id = "saltbush", name = "Saltbush", layer = "shrub", sprite = "dry_shrub", height = { 0.6, 2 }, width = 1.5,
    temperature = { 8, 18, 30 }, rain = { 80, 300, 700 }, frost = -12, grows_on = { "soil", "sand", "gravel" },
    slope = 0.7, density = 1.0, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1.1, 1.0, 0.8 }, { 1.1, 1.0, 0.8 } } },
  { id = "saguaro", name = "Column cactus", layer = "shrub", sprite = "cactus", height = { 2, 9 }, width = 0.5,
    temperature = { 15, 22, 32 }, rain = { 50, 250, 600 }, frost = 0, grows_on = { "sand", "gravel", "soil" },
    slope = 0.6, density = 0.5, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 0.9, 0.95, 0.95 } } },
  { id = "grass", name = "Grass", layer = "ground", sprite = "grass", height = { 0.3, 0.9 }, width = 1.0,
    temperature = { -6, 12, 30 }, rain = { 200, 800, 3500 }, frost = -45, grows_on = { "soil" }, slope = 0.8,
    density = 2.5, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.2, 1.05, 0.7 }, { 1.1, 1.0, 0.75 } } },
  { id = "fern", name = "Fern", layer = "ground", sprite = "fern", height = { 0.4, 1.2 }, width = 1.2,
    temperature = { 2, 14, 28 }, rain = { 800, 1800, 5000 }, frost = -25, grows_on = { "soil" }, slope = 0.85,
    density = 1.2, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.35, 0.85, 0.5 }, { 0.7, 0.62, 0.55 } } },
  { id = "meadow_flowers", name = "Meadow flowers", layer = "ground", sprite = "flowers", height = { 0.3, 0.7 },
    width = 1.0, temperature = { -2, 10, 24 }, rain = { 350, 800, 2000 }, frost = -35, grows_on = { "soil" },
    slope = 0.7, density = 0.8, seasons = { { 1.05, 1.1, 0.95 }, { 1, 1, 1 }, { 1.1, 0.95, 0.75 }, { 0.7, 0.62, 0.55 } } },
  { id = "reeds", name = "Reeds", layer = "ground", sprite = "reeds", height = { 1.5, 3 }, width = 0.8,
    temperature = { 0, 15, 30 }, rain = { 100, 900, 4000 }, frost = -30, grows_on = { "mud", "soil", "sand" },
    slope = 0.2, wet = true, density = 3.0, seasons = { { 1, 1, 1 }, { 1, 1, 1 }, { 1.15, 1.0, 0.7 }, { 1.1, 0.95, 0.7 } } },
}
