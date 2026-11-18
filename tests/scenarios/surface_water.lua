-- SPDX-License-Identifier: MIT
-- Surface water from Lua: pour water by the carpet, watch it spread and the
-- balance hold, then let it rain.
-- spec: shallow-water: Water from Lua
scenario { name = "surface water" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
expect(sim.water_at(here).loaded, "the carpet's ground is in the active tier")
-- Dry land near the carpet.
local spot
for r = 0, 120, 6 do
  for _, a in ipairs({ 0, 1.6, 3.1, 4.7 }) do
    local p = world.move(here, a, r, 0)
    local c = sim.water_at(p)
    if not spot and c.loaded and not c.fixed and c.depth == 0 then
      spot = p
    end
  end
end
expect(spot, "dry land near the carpet")
local function balanced()
  local s = sim.water_stats()
  local held = s.on_ground + s.soaked + s.drained
  local given = s.rained + s.added
  return math.abs(held - given) <= 1e-3 * math.max(given, 1)
end
expect(sim.add_water(spot, 4, 200) > 0, "it got water")
local c = sim.water_at(spot)
expect(c.depth > 0.5, "a pool: " .. c.depth)
expect_near(c.level, c.ground + c.depth, 1e-6)
expect(sim.water_stats().awake > 0, "moving water")
step(60 * 10)
local st = sim.water_stats()
expect(st.wet > 10, "it spread: " .. st.wet)
expect(st.added == 200, "200 m^3 poured")
expect(balanced(), "the balance holds")
sim.set_weather({ rain = 30 })
step(60 * 5)
st = sim.water_stats()
expect(st.rained > 0, "rain fell")
expect(balanced(), "the balance holds in the rain")
sim.set_weather({ rain = 0 })
-- Invalid calls are refused.
expect_error(function() sim.add_water(spot, 100, 1) end, "radius must be 0..64 metres")
expect_error(function() sim.add_water(spot, 1, -1) end, "cubic_metres must be 0..1e7")
