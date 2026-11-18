-- SPDX-License-Identifier: MIT
-- The carpet's condition in the game: rain soaks it and it flies slower;
-- low over flames it burns; torn to the weft it sinks and cannot climb; on
-- foot by it, M patches it with fibres and void mana; in dry air it dries;
-- the HUD shows it and the condition is saved.
-- spec: carpet-condition: Condition in the game
scenario { name = "condition" }
local game = demo.game
local condition = demo.condition
local pack = demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
local function news(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local cond = game.condition
-- Rain soaks it, and it flies slower.
sim.set_weather({ rain = 8, cloud = 1, temperature = 12 })
for _ = 1, 600 do
  world.set_time(world.time().time + 60) -- an hour of rain, ten minutes a tick
  step(1)
  if cond.wetness > 0.6 then break end
end
expect(cond.wetness > 0.6 and news("Carpet 100% whole, wet") or news("soaked"), "soaked: " .. cond.wetness)
local c = game.carpet
c.speed = 0
input.inject("steer", 0, 1, 600)
step(600)
local wet_speed = c.speed
expect(wet_speed < 60 * 0.85 and wet_speed > 30, "slower when wet: " .. wet_speed)
-- Low over flames, it burns.
sim.set_weather({ rain = 0, cloud = 0, temperature = 20 })
c.speed = 0
step(2)
c.pos.alt = world.ground_height(c.pos) + 2
sim.add_fuel(c.pos, 20, 4)
sim.ignite(c.pos, 3)
local before = cond.integrity
for _ = 1, 120 do
  step(1)
  c.speed = 0
  c.pos.alt = world.ground_height(c.pos) + 2
  if sim.fire_at(c.pos).burning and cond.integrity < before - 4 then break end
end
expect(cond.integrity < before, "burnt: " .. cond.integrity)
-- Torn to the weft, it sinks and cannot climb.
sim.cool(c.pos, 10, 1e12)
cond.integrity = 1
cond.wetness = 0
sim.add_fuel(c.pos, 0, 0)
condition.tick(cond, 1, 0, { exposed = true, burning = true, height = 1 })
expect(cond.integrity == 0 and news("torn to the weft"), "grounded")
c.pos.alt = world.ground_height(c.pos) + 30
input.inject("climb", 1, 0, 300)
local high = c.pos.alt
step(300)
expect(c.pos.alt < high - 10, "it sinks though it is told to climb")
-- Patched on foot with fibres and void mana, it flies again.
for _ = 1, 600 do
  step(1)
  if c.grounded then break end
end
c.speed = 0
input.inject("mount", 1, 0, 1)
step(3)
expect(game.walker ~= nil, "on foot")
pack.add(game.pack, "reed_fibre", 4, 0)
game.magic.caster.mage.mana.void = 10
input.inject("patch", 1, 0, 1)
step(2)
expect(cond.integrity == condition.PATCH and condition.limits(cond).fly and news("Patched"), "patched")
-- Saved with the game.
local back = condition.new(condition.snapshot(cond))
expect(back.integrity == cond.integrity, "kept")
sim.natural_weather()
