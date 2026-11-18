-- SPDX-License-Identifier: MIT
-- Unit tests of the carpet's condition (assets/base/scripts/flight/condition.lua).
local condition = require("flight.condition")
local pack = require("player.pack")

-- spec: carpet-condition: Burns, tears, wet and dry
test.case("burns, tears, soaking and drying, and how it flies", function()
  local c = condition.new()
  test.expect(c.integrity == 100 and c.wetness == 0 and #condition.hud_lines(c) == 0)
  local lim = condition.limits(c)
  test.expect(lim.speed == 1 and lim.climb == 1 and lim.fly)
  -- Flames under it, low: it burns; high up, it does not.
  condition.tick(c, 1, 0, { exposed = true, burning = true, height = 2 })
  test.expect(c.integrity == 100 - condition.BURN)
  condition.tick(c, 1, 0, { exposed = true, burning = true, height = 20 })
  test.expect(c.integrity == 100 - condition.BURN)
  -- Skidding along the ground fast tears it.
  condition.tick(c, 1, 0, { exposed = true, grounded = true, speed = 40 })
  test.expect(c.integrity == 100 - condition.BURN - condition.TEAR)
  -- Rain soaks it; worn (not exposed) it stays dry; it dries in time, faster by a fire.
  condition.tick(c, 0, 1, { exposed = true, rain = 5, air = 10 })
  test.expect(math.abs(c.wetness - condition.RAIN_SOAK * 5) < 1e-9)
  condition.tick(c, 0, 1, { exposed = false, rain = 5, air = 10 })
  test.expect(c.wetness < condition.RAIN_SOAK * 5, "worn, it dries")
  c.wetness = 1
  test.expect(condition.limits(c).speed < 0.75 and condition.limits(c).climb <= 0.5)
  test.expect(condition.hud_lines(c)[1]:find("soaked", 1, true))
  local d = condition.new({ wetness = 1 })
  condition.tick(c, 0, 0.5, { exposed = true, air = 10 })
  condition.tick(d, 0, 0.5, { exposed = true, air = 10, fire = true })
  test.expect(d.wetness < c.wetness and c.wetness < 1, "dried faster by a fire")
  condition.tick(d, 1, 0, { exposed = true, on_water = true })
  test.expect(d.wetness > 0, "soaked on the water")
  -- Torn to the weft it cannot fly.
  condition.tick(c, 20, 0, { exposed = true, burning = true, height = 1 })
  test.expect(c.integrity == 0 and not condition.limits(c).fly and condition.limits(c).speed < 0.7)
  test.expect(condition.hud_lines(c)[1]:find("grounded", 1, true))
end)

-- spec: carpet-condition: Patching and upgrades
test.case("field patches, the workshop's repair, and upgrades in the slots", function()
  local c = condition.new({ integrity = 0, grounded_out = true })
  local p = pack.new()
  local mage = { mana = { life = 0, void = 0 } }
  local ok, why = condition.patch(c, p, mage)
  test.expect(not ok and why:find("no thread", 1, true))
  pack.add(p, "reed_fibre", 3, 0)
  ok, why = condition.patch(c, p, mage)
  test.expect(not ok and why:find("mana", 1, true))
  mage.mana.void = 5
  test.expect_eq(condition.patch(c, p, mage), condition.PATCH)
  test.expect(mage.mana.void == 5 - condition.PATCH_MANA and pack.count(p, "reed_fibre") == 1)
  test.expect(condition.limits(c).fly, "airworthy again")
  pack.add(p, "hide", 1, 0)
  mage.mana.life = 10
  for _ = 1, 5 do condition.patch(c, p, mage) pack.add(p, "hide", 1, 0) end
  test.expect_eq(c.integrity, condition.FIELD_MAX)
  test.expect(select(2, condition.patch(c, p, mage)):find("no better", 1, true))
  condition.repair(c)
  test.expect_eq(c.integrity, 100)
  -- Upgrades.
  test.expect(condition.upgrades().weatherproof and condition.upgrades().fire_ward)
  test.expect(condition.install(c, "weatherproof") and condition.has(c, "weatherproof"))
  test.expect(select(2, condition.install(c, "weatherproof")) == "already woven in")
  test.expect(select(2, condition.install(c, "gold_thread")) == "no such upgrade")
  condition.tick(c, 0, 2, { exposed = true, rain = 10 })
  test.expect_eq(c.wetness, 0, "weatherproof")
  test.expect(condition.install(c, "fire_ward") and condition.install(c, "swift_weave"))
  test.expect(select(2, condition.install(c, "silent_glide")) == "no free slot")
  condition.tick(c, 1, 0, { exposed = true, burning = true, height = 1 })
  test.expect(math.abs(c.integrity - (100 - condition.BURN * 0.3)) < 1e-9, "the ward lining")
  test.expect(math.abs(condition.limits(c).speed - 1.15) < 1e-9)
  -- Kept with the game.
  local back = condition.new(condition.snapshot(c))
  test.expect(back.integrity == c.integrity and #back.upgrades == 3 and condition.has(back, "fire_ward"))
  local odd = condition.new({ integrity = 900, wetness = -1, upgrades = { "nothing", "weatherproof" }, slots = "x" })
  test.expect(odd.integrity == 100 and odd.wetness == 0 and #odd.upgrades == 1)
end)
