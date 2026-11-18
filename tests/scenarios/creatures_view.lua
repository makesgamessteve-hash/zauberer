-- SPDX-License-Identifier: MIT
-- Rendered: wolves, deer and a storm bird in front of the carpet, one deer
-- dead on its side, drawn from low-polygon parts in their colours: eight
-- parts for a walker (body, haunches, head, four legs), four for a flier
-- (body, head, two wings). A deer far beyond the range is not drawn.
-- spec: creatures: Creatures are drawn
scenario { name = "creatures view" }
local game = demo.game
game.hud = false
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
for _, id in ipairs(game.fauna.ids) do sim.remove_creature(id) end -- only the ones placed here
demo.ecology.pause(game.ecology, true)
local spot = world.move(here, game.carpet.heading, 40, 0)
local function above(p, h)
  local q = world.move(p, 0, 0, 0)
  q.alt = world.ground_height(q) + h
  return q
end
game.camera_override = function()
  local t = world.to_local(above(spot, 12)) -- the walkers and the bird above them in view
  return { eye = { t[1], t[2] + 18, t[3] + 45 }, target = t, up = { 0, 1, 0 }, fov = 60, near = 0.5, far = 5000 }
end
step(2)
local before = render.stats().model_draws
local placed = {
  { "wolf", -6, 0 }, { "wolf", -3, 2 }, { "deer", 2, -2 }, { "deer", 5, 1 }, { "storm_bird", 0, 0 },
}
local ids = {}
for i, p in ipairs(placed) do
  ids[i] = sim.spawn_creature(p[1], world.move(spot, 1.57, p[2], p[3]))
end
expect(not sim.hurt(ids[4], "magic", 1000), "one deer lies dead")
sim.spawn_creature("deer", world.move(spot, 0, 900, 0)) -- beyond the drawing range
step(60)
local drawn = render.stats().model_draws - before
expect(drawn >= 4 * 8 + 4 and drawn < 4 * 8 + 4 + 8, "parts drawn: " .. drawn)
