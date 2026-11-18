-- SPDX-License-Identifier: MIT
-- Rendered: a fire lit in front of the carpet is drawn as flames.
-- spec: thermal-and-fire: Fire from Lua
scenario { name = "fire view" }
local game = demo.game
game.hud = false
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
local spot
for r = 20, 140, 5 do
  local p = world.move(here, game.carpet.heading, r, 0)
  local c = sim.fire_at(p)
  if not spot and c.loaded and not c.water and c.fuel > 0.2 and c.moisture < 0.25 then
    spot = p
  end
end
expect(spot, "fuel ahead of the carpet")
-- Look at the fire from 40 m up and 60 m back.
game.camera_override = function()
  local t = world.to_local(spot)
  return { eye = { t[1], t[2] + 40, t[3] + 60 }, target = t, up = { 0, 1, 0 }, fov = 60, near = 1, far = 5000 }
end
sim.set_weather({ wind_east = 0, wind_north = 0, rain = 0 })
step(2)
local before = render.stats().sprite_draws
expect(sim.ignite(spot, 6) > 0)
step(60 * 5)
expect(sim.fire_stats().burning > 0)
local stats = render.stats()
expect(stats.sprite_draws > before + 10, "flames drawn: " .. stats.sprite_draws .. " (" .. before .. " before)")
