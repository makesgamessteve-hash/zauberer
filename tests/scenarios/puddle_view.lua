-- SPDX-License-Identifier: MIT
-- Rendered: water poured in front of the carpet is drawn as standing water.
-- spec: shallow-water: Water from Lua
scenario { name = "puddle view" }
local game = demo.game
game.hud = false
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
local spot
for r = 20, 140, 5 do
  local p = world.move(here, game.carpet.heading, r, 0)
  local c = sim.water_at(p)
  if not spot and c.loaded and not c.fixed then
    spot = p
  end
end
expect(spot, "land ahead of the carpet")
game.camera_override = function()
  local t = world.to_local(spot)
  return { eye = { t[1], t[2] + 40, t[3] + 60 }, target = t, up = { 0, 1, 0 }, fov = 60, near = 1, far = 5000 }
end
step(2)
local before = render.stats().water_items
expect(sim.add_water(spot, 8, 600) > 0)
step(60)
local after = render.stats().water_items
expect(after > before, "standing water drawn: " .. after .. " (" .. before .. " before)")
