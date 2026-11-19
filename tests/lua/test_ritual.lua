-- SPDX-License-Identifier: MIT
-- Unit tests of ritual magic (assets/base/scripts/magic/ritual.lua).
local ritual = require("magic.ritual")
local pack = require("player.pack")

-- spec: ritual-magic: Laying the circle
test.case("the time, the place, the ingredients; channelling to the end or broken", function()
  local rain = ritual.get("call_rain")
  test.expect(ritual.timely(rain, 0.3) and not ritual.timely(rain, 0.9) and not ritual.timely(rain, 0))
  test.expect(ritual.timely(ritual.get("unfold_room"), 0) and ritual.timely(ritual.get("ward_camp"), 1))
  local s = ritual.new()
  local p = pack.new()
  local here = world.position(0, 100, 100, 50, 50, 0)
  local ctx = { pos = here, pack = p, daylight = 0.3, on_foot = true }
  test.expect(select(2, ritual.begin(s, rain, { pos = here, pack = p, daylight = 0.3 })) == "a circle is laid on foot")
  test.expect(select(2, ritual.begin(s, rain, { pos = here, pack = p, daylight = 0.9, on_foot = true }))
    == "not the time: twilight")
  test.expect(select(2, ritual.begin(s, rain, ctx)):find("not enough boiled water", 1, true))
  test.expect(select(2, ritual.begin(s, ritual.get("unfold_room"), { pos = here, pack = p, daylight = 0,
    on_foot = true })) == "by the pitched tent")
  pack.add(p, "boiled_water", 2, 0)
  test.expect(ritual.begin(s, rain, ctx) and pack.count(p, "boiled_water") == 0)
  test.expect(select(2, ritual.begin(s, rain, ctx)) == "a ritual is already under way")
  -- Channelled from the mage (the land here gives nothing: not loaded).
  local mage = { mana = { water = 100, storm = 100 } }
  local r = ritual.tick(s, 10, { pos = here, mage = mage, now = 0 })
  test.expect(r == nil and math.abs(ritual.progress(s) - 0.5) < 1e-9 and mage.mana.water == 80)
  test.expect(ritual.line(s, p):find("Call rain 50%", 1, true))
  r = ritual.tick(s, 10, { pos = here, mage = mage, now = 0 })
  test.expect(r.done == rain and mage.mana.water == 60 and mage.mana.storm == 80 and not s.active)
  local eff = ritual.effects(s, 3600)
  test.expect(eff.rain == rain.rain)
  eff = ritual.effects(s, 7 * 3600)
  test.expect(eff.rain == nil and eff.rain_ended and next(s.effects) == nil)
  -- Broken: moved away, hurt, or out of mana; it lashes back by what was drawn.
  pack.add(p, "boiled_water", 2, 0)
  ritual.begin(s, rain, ctx)
  ritual.tick(s, 5, { pos = here, mage = mage, now = 0 })
  r = ritual.tick(s, 1, { pos = world.move(here, 0, 10, 0), mage = mage, now = 0 })
  test.expect(r.broken == rain and r.why == "the circle left" and r.backlash > 0)
  pack.add(p, "boiled_water", 2, 0)
  ritual.begin(s, rain, ctx)
  r = ritual.tick(s, 1, { pos = here, mage = mage, now = 0, hurt = true })
  test.expect(r.broken and r.why == "hurt")
  pack.add(p, "boiled_water", 2, 0)
  ritual.begin(s, rain, ctx)
  r = ritual.tick(s, 1, { pos = here, mage = { mana = { water = 0, storm = 0 } }, now = 0 })
  test.expect(r.broken and r.why:find("too little", 1, true))
  -- Mastery makes rituals cheaper; the effects are kept with the game.
  pack.add(p, "boiled_water", 2, 0)
  ritual.begin(s, rain, { pos = here, pack = p, daylight = 0.3, on_foot = true, factor = 0.5 })
  local m2 = { mana = { water = 100, storm = 100 } }
  ritual.tick(s, 20, { pos = here, mage = m2, now = 0 })
  test.expect(m2.mana.water == 80, "half the mana")
  local back = ritual.new(ritual.snapshot(s))
  test.expect(back.effects.rain and ritual.new({ effects = { rain = 5 } }).effects.rain == nil)
end)
