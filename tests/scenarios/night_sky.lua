-- SPDX-License-Identifier: MIT
-- Rendered: looking up from the carpet, the noon sky is bright and blue and
-- the midnight sky dark with stars (rendered tests).
-- spec: sky-and-time: The night sky is drawn
scenario { name = "night sky" }
local game = demo.game
game.hud = false
local x, y = world.to_cartesian(game.carpet.pos)
local noon = (math.pi - math.atan(y, x)) / (2 * math.pi) % 1 * 86400 + 86400
local function eye(alt)
  local p = game.carpet.pos
  return world.to_local({ face = p.face, x = p.x, y = p.y, lx = p.lx, ly = p.ly, alt = p.alt + alt })
end
game.camera_override = function()
  local e = eye(0)
  return { eye = e, target = { e[1] + 0.1, e[2] + 100, e[3] + 1 }, up = { 0, 0, -1 }, fov = 70, near = 1,
    far = 5000 }
end
local function look(time)
  world.set_time(time)
  render.pin_time(0)
  step(3)
  local size = render.capture("internal")
  local blue, bright, dark, total = 0, 0, 0, 0
  for py = 0, size.height - 1, 2 do
    for px = 0, size.width - 1, 2 do
      local r, g, b = render.pixel(px, py)
      total = total + 1
      blue = blue + (b > r + 20 and b > 120 and 1 or 0)
      bright = bright + (r + g + b > 400 and 1 or 0)
      dark = dark + (r + g + b < 120 and 1 or 0)
    end
  end
  return blue / total, bright / total, dark / total
end
local blue = look(noon)
expect(blue > 0.8, "a blue noon sky: " .. blue)
local _, stars, dark = look(noon + 43200)
expect(dark > 0.8, "a dark midnight sky: " .. dark)
expect(stars > 0 and stars < 0.05, "scattered stars: " .. stars)
