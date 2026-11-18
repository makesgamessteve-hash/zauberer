-- SPDX-License-Identifier: MIT
-- Rigid bodies from Lua: drop a boulder, a crate and a log by the carpet,
-- watch them come to rest, push one, hit it with a thrown stone and sweep
-- a ray through the scene.
-- spec: rigid-bodies: Bodies from Lua
scenario { name = "bodies" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
expect(sim.water_at(here).loaded, "the carpet's ground is in the active tier")
-- Dry, fairly level ground near the carpet.
local spot
for r = 0, 120, 6 do
  for _, a in ipairs({ 0, 1.6, 3.1, 4.7 }) do
    local p = world.move(here, a, r, 0)
    local c = sim.water_at(p)
    local level = math.abs(world.ground_height(world.move(p, 0, 4, 0)) - world.ground_height(p)) < 0.2
      and math.abs(world.ground_height(world.move(p, 1.6, 4, 0)) - world.ground_height(p)) < 0.2
    if not spot and c.loaded and not c.fixed and c.depth == 0 and level then
      spot = p
    end
  end
end
expect(spot, "level ground near the carpet")
local function above(p, h)
  local q = world.move(p, 0, 0, 0)
  q.alt = world.ground_height(q) + h
  return q
end
local ball = sim.spawn { shape = "sphere", material = "rock", pos = above(spot, 5), radius = 0.4 }
local crate = sim.spawn { shape = "box", material = "wood", pos = above(world.move(spot, 1.6, 4, 0), 3),
  size = { 1, 1, 1 } }
local log = sim.spawn { shape = "capsule", material = "pine_wood", pos = above(world.move(spot, 3.1, 4, 0), 3),
  radius = 0.25, length = 3, spin = { east = 1 } }
expect(ball < crate and crate < log, "ids in order")
local b = sim.body(ball)
expect_eq(b.shape, "sphere")
expect_eq(b.material, "base:rock")
expect_near(b.mass, 4 / 3 * math.pi * 0.4 ^ 3 * 2700, 1e-6)
expect_eq(sim.body(log).length, 3)
expect_eq(sim.body(crate).size[3], 1)
step(60 * 8)
for _, id in ipairs({ ball, crate, log }) do
  local s = sim.body(id)
  expect(s.speed < 0.5, s.shape .. " came to rest: " .. s.speed)
  expect(not s.frozen)
end
b = sim.body(ball)
expect(b.pos.alt - world.ground_height(b.pos) < 0.8, "the ball lies on the ground")
expect_eq(sim.body_stats().count, 3)
-- A push wakes it and lifts it.
local before = sim.body(crate).pos.alt
expect(sim.push(crate, { up = 3000 }))
expect(not sim.body(crate).asleep)
step(10)
expect(sim.body(crate).pos.alt > before + 0.1, "it jumped")
step(60 * 4)
-- A stone thrown down onto the crate.
local c = sim.body(crate)
local from = world.move(c.pos, 0, 0, 0)
from.alt = c.pos.alt + 6
local stone = sim.launch(from, { up = -20 }, 0.05, 1)
expect(sim.projectile(stone).flying)
step(60)
local p = sim.projectile(stone)
expect(p and not p.flying and p.hit, "the stone stopped")
expect_eq(p.hit.body, crate, "it struck the crate")
expect(p.hit.normal.up > 0.7, "on its top")
-- A ray down onto the ball, and one into open air.
b = sim.body(ball)
local top, bottom = world.move(b.pos, 0, 0, 0), world.move(b.pos, 0, 0, 0)
top.alt = b.pos.alt + 10
bottom.alt = b.pos.alt - 10
local hit = sim.sweep(top, bottom)
expect(hit and hit.body == ball, "the ray finds the ball")
expect_near(hit.distance, 10 - 0.4, 0.05)
local high = world.move(spot, 0, 0, 0)
high.alt = 3000
expect_eq(sim.sweep(high, world.move(high, 0, 100, 0), 1), nil, "nothing up there")
expect_eq(#sim.bodies_near(above(spot, 0), 20), 3)
expect(sim.tier_stats().bodies_ms >= 0)
-- Turned and at rest from the start.
local tilted = sim.spawn { shape = "box", material = "rock", pos = above(world.move(spot, 4.7, 5, 0), 0.8),
  size = { 1, 1, 1 }, axis_x = { east = 1, up = 1 }, axis_z = { east = -1, up = 1 }, velocity = { north = 1 },
  asleep = true }
local t = sim.body(tilted)
expect(t.asleep, "at rest")
expect_near(t.axis_z.up, math.sqrt(0.5), 1e-9)
expect(sim.remove_body(tilted))
-- Mistakes.
expect_error(function() sim.push(ball, { east = 1e9 }) end, "impulse.east is out of range")
expect_error(function() sim.spawn { shape = "cone", material = "rock", pos = spot } end, "shape must be")
expect_error(function() sim.spawn { shape = "sphere", material = "water", pos = spot } end, "cannot be made")
expect_error(function() sim.spawn { shape = "sphere", material = "unobtainium", pos = spot } end, "no material")
expect_error(function() sim.launch(spot, { up = 1 }, 5) end, "radius must be")
expect_error(function() sim.push(ball, 3) end, "impulse must be a table")
expect_error(function() sim.sweep(spot, spot, -1) end, "radius must be")
expect(sim.remove_body(log))
expect(not sim.remove_body(log))
expect_eq(sim.body(log), nil)
expect_eq(sim.projectile(99999), nil)
expect_eq(sim.body_stats().count, 2)
