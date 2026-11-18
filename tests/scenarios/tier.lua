-- SPDX-License-Identifier: MIT
-- The active tier from Lua: its cost against the budget, and a fire left
-- burning while the carpet flies away for a month.
-- spec: simulation-tiers: Tier from Lua
scenario { name = "tier" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
sim.set_weather({ wind_east = 3, wind_north = 0, rain = 0, snow = 0, storm = 0, cloud = 0 })
step(120)
local st = sim.tier_stats()
expect_eq(st.budget_ms, 6)
expect(st.total_ms >= 0 and st.fire_ms >= 0 and st.water_ms >= 0 and st.air_ms >= 0 and st.weather_ms >= 0)
expect(st.total_ms <= st.budget_ms * 4, "a quiet tier is cheap: " .. st.total_ms)
-- A fire near the carpet.
local spot
for r = 0, 120, 6 do
  for _, a in ipairs({ 0, 1.6, 3.1, 4.7 }) do
    local p = world.move(here, a, r, 0)
    local c = sim.fire_at(p)
    if not spot and c.loaded and not c.water and c.fuel > 0.2 and c.moisture < 0.25 then
      spot = p
    end
  end
end
expect(spot, "fuel near the carpet")
expect(sim.ignite(spot, 6) > 0)
step(60 * 3)
expect(sim.fire_at(spot).burning or sim.fire_at(spot).burnt, "it burns")
-- Away for a month: the fire is remembered, then has burnt on.
game.carpet.pos = world.move(here, 0, 6000, 0)
for _ = 1, 600 do
  step(10)
  if sim.tier_stats().dormant_fires > 0 or not sim.fire_at(spot).loaded then break end
end
expect(not sim.fire_at(spot).loaded, "the fire's chunk left the active tier")
world.set_time(world.time().time + 30 * 86400)
game.carpet.pos = here
for _ = 1, 900 do
  step(10)
  if sim.fire_at(spot).loaded then break end
end
local c = sim.fire_at(spot)
expect(c.loaded and c.burnt, "a burn scar where the fire was")
expect(not c.burning, "long burnt out")
expect_eq(sim.tier_stats().dormant_fires, 0)
