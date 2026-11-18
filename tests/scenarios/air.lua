-- SPDX-License-Identifier: MIT
-- Air from Lua: the wind by the carpet follows the weather, scent drifts
-- downwind, and a fire's smoke goes into the air.
-- spec: air-and-gases: Air from Lua
scenario { name = "air" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.wind_at(here).loaded and sim.fire_at(here).loaded then break end
  sleep(5)
end
expect(sim.wind_at(here).loaded, "the carpet's air is in the active tier")
-- Only the scent released here (animals leave theirs too).
demo.ecology.pause(game.ecology, true)
for _, id in ipairs(game.fauna.ids) do sim.remove_creature(id) end
local released = sim.air_stats().emitted.scent
sim.set_weather({ wind_east = 6, wind_north = 0, rain = 0, snow = 0, storm = 0 })
local w = sim.wind_at(here)
expect(w.east > 0 and w.speed > 1, "a wind from the west: " .. w.east .. ", " .. w.north)
expect_near(w.speed, math.sqrt(w.east * w.east + w.north * w.north), 1e-9)
-- Scent released for ten seconds drifts east (heading pi/2).
for _ = 1, 10 do
  sim.emit(here, "scent", 5)
  step(60)
end
local down = sim.gas_at(world.move(here, math.pi / 2, 30, 0)).scent
local up = sim.gas_at(world.move(here, -math.pi / 2, 30, 0)).scent
expect(down > 0 and down > 10 * up, "scent downwind: " .. down .. " upwind: " .. up)
local st = sim.air_stats()
expect(st.chunks > 0 and st.awake > 0)
expect_near(st.emitted.scent - released, 50, 1e-9)
expect(st.mass.scent > 0)
-- A fire's smoke goes into the air.
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
expect(sim.ignite(spot, 4) > 0)
step(60 * 10)
st = sim.air_stats()
expect(st.emitted.smoke > 0 and st.mass.smoke > 0, "smoke in the air")
-- Invalid calls are refused.
expect_error(function() sim.emit(here, "plasma", 1) end, "gas must be smoke, steam, scent or fumes")
expect_error(function() sim.emit(here, "smoke", -1) end, "kg must be 0..1e6")
