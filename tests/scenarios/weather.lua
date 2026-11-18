-- SPDX-License-Identifier: MIT
-- Weather from Lua: the world's own weather at the carpet, a weather the
-- scripts set, back to the world's, and lightning that sets the forest alight.
-- spec: weather: Weather in the game
scenario { name = "weather" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
step(60)
-- The world's own weather is what the simulations see.
local w = sim.weather()
expect(w.natural, "the world's own weather")
local nat = world.weather(here)
expect(nat.cloud >= 0 and nat.cloud <= 1 and nat.rain >= 0 and nat.snow >= 0)
expect_near(w.wind_east, nat.wind_east, 3, "the wind at the carpet")
expect(math.abs(nat.temperature) < 60)
-- A weather the scripts set holds until they let go.
sim.set_weather({ wind_east = 0, wind_north = 0, rain = 0, snow = 0, cloud = 0, storm = 0 })
step(120)
w = sim.weather()
expect(not w.natural and w.cloud == 0 and w.rain == 0, "the scripts' weather holds")
sim.natural_weather()
step(70)
expect(sim.weather().natural, "back to the world's weather")
-- A storm the scripts set sends lightning; lightning sets dry fuel alight.
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
local before = sim.weather().strikes
expect(sim.lightning(spot) > 0, "the strike caught")
expect_eq(sim.weather().strikes, before + 1)
expect(sim.fire_at(spot).burning)
sim.set_weather({ storm = 1, cloud = 1, rain = 0, wind_east = 0, wind_north = 0 })
step(60 * 120)
expect(sim.weather().strikes > before + 1, "a storm brings lightning")
-- Invalid weather is refused.
expect_error(function() sim.set_weather({ cloud = 2 }) end, "cloud must be 0..1")
expect_error(function() sim.set_weather({ snow = -1 }) end, "snow must be 0..500")
