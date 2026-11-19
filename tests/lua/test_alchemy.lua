-- SPDX-License-Identifier: MIT
-- Unit tests of alchemy (assets/base/scripts/magic/alchemy.lua).
local alchemy = require("magic.alchemy")
local pack = require("player.pack")

local function recipe(id)
  for _, r in ipairs(alchemy.recipes()) do
    if r.id == id then return r end
  end
end

-- spec: alchemy-lab: Brewing by recipe
test.case("a healing draught from herbs, water and life mana; what is lacking is told", function()
  local r = recipe("healing_draught")
  test.expect(r and alchemy.hours(r) > 4)
  local p = pack.new()
  local mage = { mana = { life = 20 } }
  local made, why = alchemy.brew(r, p, mage, {})
  test.expect(not made and why:find("healing herbs 10", 1, true) and why:find("boiled water 1", 1, true))
  pack.add(p, "healing_herbs", 10, 0)
  pack.add(p, "boiled_water", 1, 0)
  test.expect(next(alchemy.lacks(r, p)) == nil)
  test.expect(alchemy.line(r, p):find("Recipe: Healing draught (boiled water 1/1, healing herbs 10/10", 1, true))
  local known = {}
  local hours
  made, hours = alchemy.brew(r, p, mage, known)
  test.expect(made.healing_draught and made.healing_draught >= 2, "draughts: " .. tostring(made.healing_draught))
  test.expect(hours == alchemy.hours(r) and mage.mana.life == 10 and pack.count(p, "healing_herbs") == 0)
  test.expect(pack.count(p, "healing_draught") == made.healing_draught and known.healing_draught)
  -- Without life mana the herbs steep, but no draught comes.
  pack.add(p, "healing_herbs", 10, 0)
  pack.add(p, "boiled_water", 1, 0)
  local none = alchemy.brew(r, p, { mana = { life = 0 } }, {})
  test.expect((none.healing_draught or 0) == 0)
  -- Mastery: more from the same goods.
  pack.add(p, "healing_herbs", 10, 0)
  pack.add(p, "boiled_water", 1, 0)
  local more = alchemy.brew(r, p, { mana = { life = 40 } }, {}, 1.5)
  test.expect(more.healing_draught > made.healing_draught)
end)

-- spec: alchemy-lab: Spirit and quintessence
test.case("spirit from dates, and a quintessence of light from the spirit", function()
  local p = pack.new()
  pack.add(p, "dates", 200, 0)
  pack.add(p, "boiled_water", 4, 0)
  local mage = { mana = { light = 40 } }
  local made = alchemy.brew(recipe("spirit"), p, mage, {})
  alchemy.brew(recipe("spirit"), p, mage, {})
  test.expect(made.spirit and made.spirit >= 1 and pack.count(p, "spirit") >= 2, "spirit: " .. tostring(made.spirit))
  local q = alchemy.brew(recipe("light_quintessence"), p, mage, {})
  test.expect(q.light_quintessence and q.light_quintessence >= 1 and mage.mana.light == 10)
end)
