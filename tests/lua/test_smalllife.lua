-- SPDX-License-Identifier: MIT
-- Unit tests of microbes and fungi data (assets/base/scripts/life/smalllife.lua).
local smalllife = require("life.smalllife")
local items = require("life.items")

-- spec: small-life: Microbes as rates
test.case("food spoils with warmth and wet, slower cooked or dried", function()
  local warm = smalllife.spoil(0.01, 24, 25, false, 1)
  local cold = smalllife.spoil(0.01, 24, 5, false, 1)
  local frozen = smalllife.spoil(0.01, 240, -5, false, 1)
  local wet = smalllife.spoil(0.01, 24, 25, true, 1)
  local cooked = smalllife.spoil(0.01, 24, 25, false, 0.5)
  local dried = smalllife.spoil(0.01, 24, 25, false, 0.1)
  test.expect(warm > smalllife.SPOILT, "a warm day spoils raw meat: " .. warm)
  test.expect(cold < warm and cold < smalllife.SPOILT, "cold keeps it: " .. cold)
  test.expect_near(frozen, 0.01, 1e-9)
  test.expect(wet > warm and cooked < warm and dried < cooked)
  test.expect(smalllife.spoil(0.5, 1000, 40, true, 1) <= 1, "never past rotten")
  test.expect(smalllife.SPOILT < smalllife.ROTTEN)
end)

-- spec: small-life: Fungi
test.case("every fungus gives a known item in a season", function()
  local all = items.all()
  for _, f in ipairs(smalllife.fungi()) do
    test.expect(all[f.item], "a known item: " .. f.item)
    test.expect(f.density > 0 and f.season[2] > f.season[1])
    test.expect(f.habitat == "forest" or f.habitat == "open" or f.habitat == "deadwood")
  end
end)
