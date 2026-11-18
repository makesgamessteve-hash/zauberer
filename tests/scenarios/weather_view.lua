-- SPDX-License-Identifier: MIT
-- Rendered: clouds grey the noon sky, and rain falls around the camera.
-- spec: weather: Weather in the game
scenario { name = "weather view" }
local game = demo.game
game.hud = false
local x, y = world.to_cartesian(game.carpet.pos)
local noon = (math.pi - math.atan(y, x)) / (2 * math.pi) % 1 * 86400 + 86400
local looking_up = true
game.camera_override = function()
  local p = game.carpet.pos
  local e = world.to_local({ face = p.face, x = p.x, y = p.y, lx = p.lx, ly = p.ly, alt = p.alt })
  if looking_up then
    return { eye = e, target = { e[1] + 0.1, e[2] + 100, e[3] + 1 }, up = { 0, 0, -1 }, fov = 70, near = 1,
      far = 5000 }
  end
  return { eye = e, target = { e[1] + 100, e[2], e[3] }, up = { 0, 1, 0 }, fov = 70, near = 0.5, far = 5000 }
end
local function blue_sky()
  render.pin_time(0)
  step(3)
  local size = render.capture("internal")
  local blue, total = 0, 0
  for py = 0, size.height - 1, 3 do
    for px = 0, size.width - 1, 3 do
      local r, _, b = render.pixel(px, py)
      total = total + 1
      blue = blue + (b > r + 20 and b > 120 and 1 or 0)
    end
  end
  return blue / total
end
world.set_time(noon)
sim.set_weather({ cloud = 0, rain = 0, snow = 0, storm = 0 })
local clear = blue_sky()
sim.set_weather({ cloud = 1 })
local grey = blue_sky()
expect(clear > 0.8 and grey < 0.2, "clouds grey the sky: " .. clear .. " -> " .. grey)
looking_up = false -- level, into the rain
step(2)
local before = render.stats().sprite_draws
sim.set_weather({ rain = 4 })
step(2)
expect(render.stats().sprite_draws > before + 40, "rain falls: " .. render.stats().sprite_draws)
