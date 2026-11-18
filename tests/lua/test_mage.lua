-- SPDX-License-Identifier: MIT
-- Unit tests of the mage's mana (assets/base/scripts/magic/mage.lua).
local mage = require("magic.mage")

-- spec: weave-grammar: The mage's mana
test.case("one reservoir for all colours", function()
  local m = mage.new({ capacity = 50, mana = { fire = 20 } })
  test.expect_eq(mage.total(m), 20)
  test.expect_eq(mage.add(m, "water", 40), 30) -- only 30 fits
  test.expect_eq(mage.add(m, "life", 5), 0)
  test.expect_eq(mage.add(m, "life", -5), 0)
  test.expect(mage.has(m, { fire = 20, water = 30 }))
  test.expect(not mage.take(m, { fire = 25 }), "short")
  test.expect_eq(m.mana.fire, 20, "nothing taken when short")
  test.expect(mage.take(m, { fire = 5, water = 10 }))
  test.expect_eq(m.mana.fire, 15)
  test.expect_eq(m.mana.water, 20)
end)

test.case("attunement, channeling and strain", function()
  local m = mage.new({ channel = 10, attunement = { fire = 50 } })
  test.expect_near(mage.efficiency(m, { "fire" }, 0.1), 50 / 250 + 0.1, 1e-9)
  test.expect_near(mage.efficiency(m, { "fire", "water" }, 0), 25 / 250, 1e-9)
  test.expect_eq(mage.efficiency(mage.new({ attunement = { fire = 100 } }), { "fire" }, 0.5), 0.6)
  test.expect_eq(mage.overcharge(m, 10, 1), 0)
  test.expect_near(mage.overcharge(m, 30, 1), 2, 1e-9)
  test.expect_near(mage.overcharge(m, 5, 0), 1, 1e-9) -- a quarter second at least
  mage.strain_by(m, 2)
  test.expect_eq(m.strain, 40)
  mage.strain_by(m, 10)
  test.expect_eq(m.strain, 100)
  test.expect(mage.burnt_out(m))
  mage.tick(m, 600, { fire = 200 })
  test.expect(m.strain < 90 and not mage.burnt_out(m), "strain eases")
  test.expect(m.attunement.fire > 50, "attuned by a fiery place")
  local before = m.attunement.water
  mage.cast(m, { "water" })
  test.expect_near(m.attunement.water, before + 0.5, 1e-9)
  mage.tick(m, 1)
  local snap = mage.snapshot(m)
  test.expect_eq(snap.capacity, 100)
  test.expect_eq(snap.mana.fire, 0)
  test.expect_eq(snap.attunement.water, m.attunement.water)
end)
