-- SPDX-License-Identifier: MIT
-- Survival in the game: needs drain as game time passes and show on the HUD;
-- on foot by a fire the body feels it; sleeping on foot moves the world's time
-- on and rests; a starving mage loses health; needs are saved with the game.
-- spec: survival: Survival in the game
scenario { name = "survival" }
local game = demo.game
local survival = demo.survival
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local s = game.survival
local function hud_has(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
expect(hud_has("Food well, water well"), "needs on the HUD")
local food, t0 = s.food, world.time().time
step(60 * 20)
local passed = world.time().time - t0
expect(passed > 600, "game time passed: " .. passed)
expect_near(food - s.food, 100 * passed / 86400, 0.5)
-- On foot by a fire.
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(1)
input.inject("mount", 1, 0, 1)
step(3)
expect(game.walker ~= nil, "on foot")
local me = game.walker.pos
expect(not survival.sense(me, game.walker, 0).fire, "no fire yet")
sim.add_heat(world.move(me, 0, 2, 0), 1, 5e7)
step(2)
expect(survival.sense(me, game.walker, 0).fire, "a fire close by is felt")
sim.cool(me, 10, 1e12)
-- Sleep on foot: the world's time moves on, the body rests.
s.rest = 10
local before = world.time().time
input.inject("sleep", 1, 0, 1)
step(3)
local slept = (world.time().time - before) / 3600
expect(slept >= 7 and slept <= 9.5, "slept the night: " .. slept .. " hours")
expect(s.rest >= 99, "rested")
expect(game.slept and game.slept >= 7)
-- Starving hurts.
s.food, s.water = 0, 50
local hp = game.health.hp
step(60 * 30)
expect(game.health.hp < hp, "starving hurts: " .. hp .. " -> " .. game.health.hp)
expect(hud_has("Food out"))
-- Saved with the game.
local again = survival.new(survival.snapshot(s))
expect(again.food == s.food and again.rest == s.rest and again.preset == s.preset)
demo.ecology.pause(game.ecology, false)
