-- SPDX-License-Identifier: MIT
-- Rendered: a river of a created world seen from above is drawn as water,
-- and its ripples move (rendered tests).
-- spec: water-bodies: Water is drawn
scenario { name = "water view" }
local game = demo.game
game.hud = false
game.world_options = { radius = 400000, quality = "quick", seed = 11 }
game.set_mode("create")
for _ = 1, 20000 do
  step(1)
  if game.creation.progress and game.creation.progress.done then break end
  sleep(2)
end
expect(game.fly_in_new_world())
-- An inland stretch of river: look around until one is well above the sea.
local river, surface
for k = 0, 15 do
  local from = world.move(game.carpet.pos, k * math.pi / 8, (k // 4) * 15000 + 1, 0, 0)
  local found = world.find_water(from, "river", 400000)
  local kind, level = world.water(found)
  if kind == "river" and level > 5 then
    river, surface = found, level
    break
  end
end
expect(river, "an inland river")
local function at(alt)
  return { face = river.face, x = river.x, y = river.y, lx = river.lx, ly = river.ly, alt = surface + alt }
end
-- The carpet waits high above (streaming follows it); the camera looks straight down.
game.carpet.pos, game.carpet.speed = at(400), 0
game.camera_override = function()
  return { eye = world.to_local(at(120)), target = world.to_local(at(0)), up = { 0, 0, -1 }, fov = 50, near = 1,
    far = 4000 }
end
local function settled()
  local s = world.stream_stats()
  return s.queued == 0 and s.jobs == 0
end
for _ = 1, 600 do
  step(1)
  if settled() and render.stats().water_items > 0 then break end
  sleep(5)
end
step(10)
expect(render.stats().water_items > 0, "water is drawn")
local size = render.capture("internal")
local cx, cy = size.width // 2, size.height // 2
local bluish, total = 0, 0
for y = cy - 20, cy + 20, 2 do
  for x = cx - 20, cx + 20, 2 do
    local r, _, b = render.pixel(x, y)
    bluish = bluish + (b > r + 10 and 1 or 0)
    total = total + 1
  end
end
expect(bluish > total // 4, string.format("the river is bluish (%d of %d pixels)", bluish, total))
local first = {}
for y = cy - 30, cy + 30, 2 do
  for x = cx - 30, cx + 30, 2 do
    local pr, pg, pb = render.pixel(x, y)
    first[#first + 1] = pr * 65536 + pg * 256 + pb
  end
end
step(45)
render.capture("internal")
local changed, i = 0, 0
for y = cy - 30, cy + 30, 2 do
  for x = cx - 30, cx + 30, 2 do
    i = i + 1
    local pr, pg, pb = render.pixel(x, y)
    changed = changed + ((pr * 65536 + pg * 256 + pb) ~= first[i] and 1 or 0)
  end
end
expect(changed > 0, "ripples move")
