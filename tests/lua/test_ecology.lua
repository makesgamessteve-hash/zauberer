-- SPDX-License-Identifier: MIT
-- Unit tests of population dynamics (assets/base/scripts/life/ecology.lua).
local ecology = require("life.ecology")

local SPECIES = {
  { id = "deer", density = 15, habitat = { forest = 1, open = 0.7 }, growth = 0.6 },
  { id = "hare", density = 40, habitat = { open = 1, forest = 0.5 }, growth = 2.0 },
  { id = "wolf", density = 1.5, habitat = { forest = 1, open = 0.8 }, growth = 0.4, eats = { deer = 0.3, hare = 0.3 } },
}
local FOREST = { forest = 1, open = 0, wetland = 0, dry = 0, cold = 0 }
local DESERT = { forest = 0, open = 0, wetland = 0, dry = 1, cold = 0 }

local function bounded(r)
  for id, n in pairs(r.n) do
    if not (n >= 0 and n <= r.k[id] + 1e-9) or n ~= n then return false, id end
  end
  return true
end

-- spec: ecology: Populations per chunk
test.case("capacity from habitat, logistic growth, bounds", function()
  local deer = SPECIES[1]
  test.expect_near(ecology.capacity(deer, FOREST), 15 * ecology.CHUNK_KM2, 1e-9)
  test.expect_eq(ecology.capacity(deer, DESERT), 0)
  local r = ecology.record(SPECIES, FOREST, 0)
  test.expect_near(r.n.deer, r.k.deer * ecology.START, 1e-9)
  r.n.deer, r.n.hare, r.n.wolf = 0.05, 0.1, 0
  ecology.advance(SPECIES, r, 10 * ecology.YEAR)
  test.expect(r.n.deer > 0.5 * r.k.deer, "deer grew back: " .. r.n.deer)
  test.expect(bounded(r))
  local before = r.n.deer
  ecology.advance(SPECIES, r, 0)
  test.expect_eq(r.n.deer, before, "no time, no change")
  -- A desert holds none: what was there dies out.
  local d = ecology.record(SPECIES, DESERT, 0)
  d.n.deer = 2
  ecology.advance(SPECIES, d, ecology.YEAR)
  test.expect_eq(d.n.deer, 0)
end)

-- spec: ecology: The food web
test.case("predators eat prey down and starve without it", function()
  local with = ecology.record(SPECIES, FOREST, 0)
  local without = ecology.record(SPECIES, FOREST, 0)
  with.n.wolf, without.n.wolf = with.k.wolf, 0
  ecology.advance(SPECIES, with, ecology.YEAR / 2)
  ecology.advance(SPECIES, without, ecology.YEAR / 2)
  test.expect(with.n.deer < without.n.deer and with.n.hare < without.n.hare, "wolves keep the prey down")
  local hungry = ecology.record(SPECIES, FOREST, 0)
  hungry.n.wolf, hungry.n.deer, hungry.n.hare = hungry.k.wolf, 0, 0
  ecology.advance(SPECIES, hungry, 3 * ecology.YEAR)
  test.expect(hungry.n.wolf < 0.3 * hungry.k.wolf, "starving wolves: " .. hungry.n.wolf / hungry.k.wolf)
  test.expect(bounded(hungry) and bounded(with))
  -- A long absence is caught up in at most MAX_STEPS steps and stays bounded.
  local long = ecology.record(SPECIES, FOREST, 0)
  ecology.advance(SPECIES, long, 1e6)
  test.expect(bounded(long))
end)

-- spec: ecology: Migration
test.case("species flow from crowded chunks to thin ones", function()
  local full, empty = ecology.record(SPECIES, FOREST, 0), ecology.record(SPECIES, FOREST, 0)
  full.n.deer, empty.n.deer = full.k.deer, 0
  local total = full.n.deer + empty.n.deer
  ecology.migrate(SPECIES, { full, empty }, ecology.YEAR / 4)
  test.expect(empty.n.deer > 0.2 * empty.k.deer and full.n.deer < full.k.deer, "deer moved in")
  test.expect_near(full.n.deer + empty.n.deer, total, 1e-9)
  test.expect(bounded(full) and bounded(empty))
  local desert = ecology.record(SPECIES, DESERT, 0)
  ecology.migrate(SPECIES, { full, desert }, ecology.YEAR)
  test.expect_eq(desert.n.deer, 0, "never into land that holds none")
end)

-- The biomes of created worlds by the names world.biome gives.
test.case("every land biome has its habitat, and wetlands make swamp water", function()
  local smalllife = require("life.smalllife")
  local land = { "beach", "dunes", "cliffs", "mangroves", "salt marsh", "estuary", "rainforest", "seasonal forest",
    "cloud forest", "savanna", "sand desert", "rocky desert", "salt flat", "xeric shrubland", "mediterranean scrub",
    "steppe", "deciduous forest", "mixed forest", "temperate rainforest", "taiga", "tundra", "polar desert",
    "ice sheet", "montane forest", "alpine meadow", "scree and cliffs", "glacier", "volcanic slopes", "marsh",
    "swamp forest", "peat bog", "floodplain", "ashlands", "stormspires", "glasswood", "wildheart", "drowned vale",
    "deepstone", "the hollows", "mirefen" }
  for _, name in ipairs(land) do
    local shares = ecology.biome_habitat(name)
    local sum = 0
    for _, v in pairs(shares or {}) do sum = sum + v end
    test.expect(shares and sum > 0 and sum <= 1 + 1e-9, name)
  end
  test.expect(ecology.biome_habitat("deep ocean") == nil and ecology.biome_habitat(nil) == nil)
  test.expect(ecology.biome_habitat("swamp forest").wetland > 0 and ecology.biome_habitat("mixed forest").forest > 0)
  test.expect(smalllife.wet_biome("swamp forest") and smalllife.wet_biome("mangroves") and smalllife.wet_biome("marsh"))
  test.expect(not smalllife.wet_biome("mixed forest") and not smalllife.wet_biome(nil))
end)
