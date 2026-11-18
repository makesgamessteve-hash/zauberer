-- SPDX-License-Identifier: MIT
-- Unit tests of food and drink in the item registry (assets/base/scripts/life/items.lua).
local items = require("life.items")

-- spec: harvest-and-cooking: Food and drink are data
test.case("food values, what things become, and broken entries", function()
  local all = items.all()
  test.expect(all.raw_meat.cooked == "cooked_meat" and all.cooked_meat.kcal > all.raw_meat.kcal)
  test.expect(all.swamp_water.boiled == "boiled_water" and all.swamp_water.dirty > all.river_water.dirty)
  test.expect(all.healing_herbs.brew == "herbal_tea" and all.herbal_tea.cures.sick > 0)
  test.expect(all.sea_water.salty and all.fly_agaric.poison > 0)
  test.expect(all.wood.fuel > 0 and all.deadwood.fuel > 0 and all.reed_fibre.fuel > 0)
  test.expect(all.kapok_bark.brew == "bark_tea" and all.bark_tea.cures.sick > 0)
  local base = { id = "x", name = "X", kind = "food", weight = 1 }
  local function with(k, v)
    local t = {}
    for key, val in pairs(base) do t[key] = val end
    t[k] = v
    return t
  end
  local cases = {
    { with("raw", 2), "raw out of range" },
    { with("kcal", -1), "kcal out of range" },
    { with("spoils", "pickled"), "spoils must be" },
    { with("cooked", "nothing"), "cooked is no item" },
    { with("boiled", "nothing"), "boiled is no item" },
  }
  for _, c in ipairs(cases) do
    local ok, err = items.check({ c[1] })
    test.expect(not ok and err:find(c[2], 1, true), tostring(err))
  end
end)
