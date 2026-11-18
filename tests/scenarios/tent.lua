-- SPDX-License-Identifier: MIT
-- The tent in the game: P refuses to pitch it in water and pitches it on
-- dry ground (the carpet is folded into it); E at its door goes inside,
-- where the HUD shows the rooms, A/D walk between them, the vault shelves
-- the pack, the bedchamber gives a warm night while it freezes outside, and
-- the hearth cooks; E goes out, P packs it up; it is saved.
-- spec: carpet-tent: The tent in the game
scenario { name = "tent" }
local game = demo.game
local tent, home, bag, pack = demo.tent, demo.home, demo.bag, demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and sim.water_at(here).loaded then break end
  sleep(5)
end
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function said(text)
  for _, line in ipairs(home.hud_lines(game.home_place)) do
    if line:find(text, 1, true) then return true end
  end
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
local w = game.walker
expect(w ~= nil, "on foot")
-- Not in water.
local wet = world.move(w.pos, w.heading, 2, 0)
sim.add_water(wet, 2, 30)
step(10)
expect(sim.water_at(wet).depth > 0.05, "a puddle ahead")
press("tent")
expect(game.tent == nil and said("no pitching in water"), "refused in water")
-- On dry ground, turned the other way.
w.heading = w.heading + math.pi
w.pos = world.move(w.pos, w.heading, 6, 0)
w.pos.alt = world.ground_height(w.pos)
c.pos = world.move(w.pos, 0, 1, 0)
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
expect(tent.can_pitch(world.move(w.pos, w.heading, 2, 0)), "dry, flat ground ahead")
press("tent")
local t = game.tent
expect(t ~= nil and world.distance(game.carpet.pos, t.pos) < 0.1, "pitched, the carpet folded into it")
local door = tent.door(t)
game.walker.pos = world.move(door, 0, 8, 0) -- (away from the door)
game.walker.pos.alt = world.ground_height(game.walker.pos)
step(2)
press("mount")
expect(game.walker ~= nil and not t.inside, "no carpet to step on")
-- Inside, warm while it freezes outside.
game.walker.pos = tent.door(t)
step(2)
press("mount")
expect(t.inside and said("Inside the tent: Hearth room"), "inside")
sim.set_weather({ temperature = -20, wind_east = 12, snow = 5, cloud = 1 })
local s = game.survival
s.core = 35.6
input.inject("steer", 1, 0, 3)
step(4)
expect(game.home_place.room == "bedchamber" and said("[Bedchamber]"), "walked to the bedchamber")
s.rest = 20
game.health.hp = 60
press("sleep")
expect(game.slept and game.slept > 4 and s.rest > 90, "slept")
expect(s.core > 36.5 and game.health.hp > 60, "warm and mended: " .. s.core)
for _ = 1, 2 do
  input.inject("steer", 1, 0, 3)
  step(5)
end
expect(game.home_place.room == "vault", "in the vault")
pack.add(game.pack, "cooked_meat", 5, 0)
press("stow")
expect(bag.in_vault(game.bag, "cooked_meat") == 5 and pack.weight(game.pack) == 0 and said("Shelved"), "shelved")
press("unstow")
expect(pack.count(game.pack, "cooked_meat") == 5, "taken off the shelves")
input.inject("steer", 1, 0, 3)
step(5)
expect(game.home_place.room == "hearth")
pack.add(game.pack, "raw_meat", 2, 0)
press("cook")
expect(pack.count(game.pack, "cooked_meat") == 7 and said("Cooked"), "cooked at the hearth")
-- Out, and packed up.
press("mount")
expect(not t.inside and game.walker and world.distance(game.walker.pos, tent.door(t)) < 0.5, "out at the door")
local saved = tent.snapshot(t)
press("tent")
expect(game.tent == nil and world.distance(game.carpet.pos, t.pos) < 0.5, "packed up, the carpet laid out")
local back = tent.new(saved)
expect(back and world.distance(back.pos, t.pos) < 0.01 and tent.new(nil) == nil, "kept with the game")
sim.natural_weather()
