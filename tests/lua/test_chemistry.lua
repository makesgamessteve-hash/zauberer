-- SPDX-License-Identifier: MIT
-- Unit tests of chemistry (assets/base/scripts/life/chemistry.lua).
local chemistry = require("life.chemistry")

local function near(a, b) return math.abs(a - b) < 1e-6 end

-- spec: chemistry: Substances and reactions are data
test.case("the registry is checked", function()
  test.expect(chemistry.get("water").solvent and chemistry.get("healing_draught").bottle.item == "healing_draught")
  local ok, err = chemistry.check({ { id = "a", phase = "plasma" } }, {})
  test.expect(not ok and err:find("no phase", 1, true))
  local subs = { { id = "a", phase = "solid" }, { id = "b", phase = "liquid" } }
  local cases = {
    { { id = "r", reactants = { c = 1 }, products = { b = 1 }, temp = { 0, 50 }, best = 10, rate = 1 },
      "no substance c" },
    { { id = "r", reactants = { a = 0 }, products = { b = 1 }, temp = { 0, 50 }, best = 10, rate = 1 }, "positive" },
    { { id = "r", reactants = {}, products = { b = 1 }, temp = { 0, 50 }, best = 10, rate = 1 }, "nothing reacts" },
    { { id = "r", reactants = { a = 1 }, products = { b = 1 }, temp = { 0, 50 }, best = 60, rate = 1 }, "best" },
    { { id = "r", reactants = { a = 1 }, products = { b = 1 }, temp = { 0, 50 }, best = 9, rate = 1, catalyst = "z" },
      "no catalyst" },
  }
  for _, c in ipairs(cases) do
    local good, why = chemistry.check(subs, { c[1] })
    test.expect(not good and why:find(c[2], 1, true), tostring(why))
  end
  test.expect(not chemistry.check({ { id = "a", phase = "solid", bottle = { item = "gold", kg = 1 } } }, {}))
end)

-- spec: chemistry: Processes in a vessel
test.case("steeping, heating with mana, boiling off, filtering, distilling and bottling", function()
  local v = chemistry.vessel()
  test.expect(select(2, chemistry.add(v, "wood", 1)) == "it does not go in a vessel")
  test.expect(near(chemistry.add(v, "healing_herbs", 20), 0.2))
  -- Dry, the herbs do not steep.
  chemistry.heat(v, 60, 2)
  test.expect(near(chemistry.amount(v, "herb_matter"), 0.2) and chemistry.amount(v, "life_essence") == 0)
  chemistry.add(v, "boiled_water", 1)
  local slow = chemistry.vessel()
  chemistry.add(slow, "healing_herbs", 20)
  chemistry.add(slow, "boiled_water", 1)
  chemistry.grind(v)
  chemistry.heat(v, 60, 1)
  chemistry.heat(slow, 60, 1)
  test.expect(chemistry.amount(v, "life_essence") > chemistry.amount(slow, "life_essence"), "ground goes faster")
  chemistry.heat(v, 60, 4)
  test.expect(chemistry.amount(v, "herb_matter") < 0.01 and chemistry.amount(v, "life_essence") > 0.07)
  -- Too cold, nothing happens; out of its range neither.
  local cold = chemistry.vessel()
  chemistry.add(cold, "healing_herbs", 20)
  chemistry.add(cold, "boiled_water", 1)
  chemistry.heat(cold, 5, 4)
  test.expect(chemistry.amount(cold, "life_essence") == 0)
  -- Filtering takes out the spent matter; the essence stays.
  local residue = chemistry.filter(v)
  test.expect(residue.amounts.spent_matter > 0 and chemistry.amount(v, "spent_matter") == 0)
  test.expect(chemistry.amount(v, "life_essence") > 0)
  -- Without life mana the draught does not come; with it, it does, using it up.
  chemistry.heat(v, 80, 1)
  test.expect(chemistry.amount(v, "healing_draught") == 0, "no life mana")
  chemistry.charge(v, "life", 5)
  local made = chemistry.heat(v, 80, 1)
  test.expect(made.healing_draught and made.healing_draught > 0.1 and (v.mana.life or 0) < 5)
  local essence = chemistry.amount(v, "life_essence")
  chemistry.charge(v, "life", 100)
  chemistry.heat(v, 80, 2)
  test.expect(chemistry.amount(v, "life_essence") < essence, "more mana, more draught")
  -- Boiling hot, the water boils off.
  local before = chemistry.amount(v, "water")
  chemistry.heat(v, 110, 0.5)
  test.expect(chemistry.amount(v, "water") < before)
  chemistry.reduce(v, 0)
  test.expect(chemistry.amount(v, "water") == 0)
  local bottled = chemistry.bottle(v)
  test.expect(bottled.healing_draught and bottled.healing_draught >= 2,
    "draughts: " .. tostring(bottled.healing_draught))
  test.expect(chemistry.amount(v, "healing_draught") < 0.1, "what makes no full bottle stays")
end)

-- spec: chemistry: Fermenting, distilling and knowing
test.case("fermentation needs yeast and time; distilling catches the spirit; analysis names", function()
  local v = chemistry.vessel()
  chemistry.add(v, "dates", 100)
  chemistry.add(v, "boiled_water", 2)
  chemistry.heat(v, 28, 48)
  test.expect(chemistry.amount(v, "spirit") > 0.1, "fermented: " .. chemistry.amount(v, "spirit"))
  local plain = chemistry.vessel()
  plain.amounts.sugar = 0.6
  plain.amounts.water = 1
  chemistry.heat(plain, 28, 48)
  test.expect(chemistry.amount(plain, "spirit") == 0, "no yeast, no spirit")
  local spirit = chemistry.distil(v, 85)
  test.expect(spirit.amounts.spirit > 0.1 and not spirit.amounts.water and chemistry.amount(v, "spirit") == 0)
  test.expect(chemistry.total(spirit) < chemistry.total(v))
  -- Quintessence of light from the spirit with light mana.
  chemistry.charge(spirit, "light", 60 * spirit.amounts.spirit)
  chemistry.heat(spirit, 60, 2)
  test.expect(chemistry.amount(spirit, "light_quintessence") > 0)
  -- Knowing.
  local known = {}
  test.expect_eq(chemistry.name("spirit", known), "an unknown liquid")
  local new = chemistry.analyse(spirit, known)
  test.expect(#new >= 1 and chemistry.name("spirit", known) == "Spirit of wine")
  test.expect(#chemistry.analyse(spirit, known) == 0 and chemistry.name("nothing", known) == "nothing")
end)
