-- SPDX-License-Identifier: MIT
-- Fire from Lua: light the meadow by the carpet in a west wind, watch it burn
-- downwind and out, and read the simulation.
-- spec: thermal-and-fire: Fire from Lua
scenario { name = "fire" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
expect(sim.fire_at(here).loaded, "the carpet's ground is in the active tier")
-- A place with fuel close by (meadow, shrubs or forest).
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
local cell = sim.fire_at(spot)
expect(cell.material, "the fuel's material")
expect(sim.material(cell.material).burns, "the fuel burns")
sim.set_weather({ wind_east = 6, wind_north = 0, rain = 0, snow = 0, storm = 0 })
local w = sim.weather()
expect_eq(w.wind_east, 6)
expect_eq(w.wind_north, 0)
expect_eq(w.rain, 0)
expect(sim.ignite(spot, 4) > 0, "it catches")
expect(sim.fire_at(spot).burning)
step(60 * 20)
local st = sim.fire_stats()
expect(st.chunks > 0 and st.awake > 0, "chunks awake")
expect(st.burning > 0 or st.burnt > 0, "it burns")
expect(st.max_temperature > 300, "hot: " .. st.max_temperature)
-- Rain puts it out.
sim.set_weather({ rain = 60 })
step(60 * 90)
expect_eq(sim.fire_stats().burning, 0, "the rain put it out")
expect(sim.fire_at(spot).moisture > 0.3, "wet fuel")
sim.add_heat(spot, 2, 1000)
-- Invalid calls are refused.
expect_error(function() sim.ignite(spot, 100) end, "radius must be 0..64 metres")
expect_error(function() sim.add_heat(spot, 100, 1) end, "radius must be 0..64 metres")
expect_error(function() sim.add_heat(spot, 1, -5) end, "joules must be 0..1e12")
expect_error(function() sim.set_weather({ rain = -1 }) end, "rain must be 0..500")
expect_error(function() sim.set_weather({ wind_east = 90 }) end, "wind_east must be -60..60")
