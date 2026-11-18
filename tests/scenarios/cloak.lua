-- SPDX-License-Identifier: MIT
-- The cloak in the game: on foot by the carpet, H wraps it around the mage
-- (the carpet is gone from the ground) and . shifts its colour; a wary deer
-- does not see the drab mage where it sees the plain one and flees; from
-- the air H wraps the carpet and the mage glides down unhurt; H spreads it
-- out again beside the mage; it is saved.
-- spec: carpet-cloak: The cloak in the game
scenario { name = "cloak" }
local game = demo.game
local cloak = demo.cloak
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
world.set_time(world.time().time - world.time().time % 86400 + 43200) -- noon
local function news(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
expect(game.walker ~= nil, "on foot")
press("cloak")
local k = game.cloak
expect(k.worn and news("Cloaked (plain)"), "wrapped")
press("mount")
expect(game.walker ~= nil, "no carpet to step on while it is worn")
press("cloak_colour")
press("cloak_colour")
expect(k.colour == "drab" and news("Cloaked (drab)"), "drab")
-- A wary deer 45 m off: the drab mage, standing still, is not seen.
local w = game.walker
w.speed = 0
local deer = sim.spawn_creature("deer", world.move(w.pos, w.heading, 45, 0))
step(60)
expect(sim.creature(deer).sees_player == nil and sim.creature(deer).action ~= "flee", "unseen in drab")
press("cloak_colour") -- plain
press("cloak_colour") -- vivid
step(60)
local d = sim.creature(deer)
expect(d.sees_player ~= nil and d.action == "flee", "seen in vivid, and it flees: " .. d.action)
-- From the air: wrapped in the cloak, the mage glides down unhurt.
press("cloak") -- (spread out)
expect(not k.worn and world.distance(c.pos, w.pos) < 3, "spread out beside the mage")
press("mount")
expect(game.walker == nil, "back on the carpet")
c = game.carpet
c.pos.alt = world.ground_height(c.pos) + 60
step(2)
local hp = game.health.hp
press("cloak")
expect(k.worn and game.walker ~= nil and not game.walker.grounded, "in the air, cloaked")
local landed = false
for _ = 1, 60 * 40 do
  step(1)
  if game.walker.grounded then
    landed = true
    break
  end
end
expect(landed and game.health.hp >= hp, "glided down unhurt: " .. game.health.hp)
local back = cloak.new(cloak.snapshot(k))
expect(back.worn and back.colour == k.colour, "kept")
demo.ecology.pause(game.ecology, false)
