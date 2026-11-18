-- SPDX-License-Identifier: MIT
-- Rendered: boulders, crates and logs dropped in front of the carpet are
-- drawn as meshes in their materials' colours.
-- spec: rigid-bodies: Bodies are drawn
scenario { name = "bodies view" }
local game = demo.game
game.hud = false
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
local spot = world.move(here, game.carpet.heading, 40, 0)
local function above(p, h)
  local q = world.move(p, 0, 0, 0)
  q.alt = world.ground_height(q) + h
  return q
end
game.camera_override = function()
  local t = world.to_local(above(spot, 0))
  return { eye = { t[1], t[2] + 12, t[3] + 18 }, target = t, up = { 0, 1, 0 }, fov = 60, near = 0.5, far = 5000 }
end
step(2)
local before = render.stats().model_draws
local kinds = {
  { shape = "sphere", material = "rock", radius = 0.8 },
  { shape = "box", material = "wood", size = { 1.5, 1, 1 } },
  { shape = "capsule", material = "pine_wood", radius = 0.3, length = 3 },
  { shape = "box", material = "iron", size = { 0.6, 0.6, 0.6 } },
}
for i = 0, 11 do
  local k = kinds[i % #kinds + 1]
  local desc = { shape = k.shape, material = k.material, radius = k.radius, length = k.length, size = k.size,
    pos = above(world.move(spot, i * 0.52, 3 + (i % 3), 0), 2 + i * 0.7), spin = { east = 0.3 * (i % 2) } }
  sim.spawn(desc)
end
step(60 * 4)
local stats = render.stats()
expect(stats.model_draws >= before + 12, "bodies drawn: " .. stats.model_draws .. " (" .. before .. " before)")
expect_eq(sim.body_stats().count, 12)
