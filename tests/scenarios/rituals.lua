-- SPDX-License-Identifier: MIT
-- Ritual magic in the game: in a drought, at dusk, the mage lays the circle
-- for Call rain (/) and channels it still: rain falls over the land for six
-- hours, then the world's weather returns; walking off mid-ritual breaks it
-- and it lashes back; a warded camp keeps a wolf out; by the tent at night
-- a room unfolds.
-- spec: ritual-magic: Rituals in the game
scenario { name = "rituals" }
local game = demo.game
local pack = demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and mana.at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function said(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local function choose(id)
  for _ = 1, 5 do
    if demo.ritual.rituals()[game.ritual.chosen].id == id then return true end
    press("ritual_next")
  end
  return false
end
-- To the moment of dusk.
local function to(daylight_lo, daylight_hi)
  for _ = 1, 400 do
    local d = world.sky(game.walker and game.walker.pos or game.carpet.pos).daylight
    if d > daylight_lo and d < daylight_hi then return true end
    world.set_time(world.time().time + 120)
    step(1)
  end
  return false
end
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
local w = game.walker
expect(w, "on foot")
sim.set_weather({ rain = 0, cloud = 0, temperature = 32 }) -- the drought
local mage = game.magic.caster.mage
mage.mana.water, mage.mana.storm = 60, 40
pack.add(game.pack, "boiled_water", 4, 0)
expect(choose("call_rain"))
world.set_time(world.time().time - world.time().time % 86400 + 12 * 3600)
press("ritual")
expect(said("not the time: twilight"), "not at noon")
expect(to(0.05, 0.45), "dusk")
press("ritual")
expect(game.ritual.active and said("The circle is laid: Call rain"), "laid")
for _ = 1, 60 * 21 do
  w.speed = 0
  step(1)
  if not game.ritual.active then break end
end
expect(game.ritual_done == "call_rain" and sim.weather().rain > 4, "rain: " .. sim.weather().rain)
local stop = world.time().time + 7 * 3600
while world.time().time < stop do
  world.set_time(world.time().time + 240)
  step(1)
end
expect(game.ritual.effects.rain == nil, "after six hours the rain is done")
-- Walking off mid-ritual breaks it, and it lashes back.
expect(to(0.05, 0.45), "dusk again")
mage.mana.water, mage.mana.storm = 60, 40
press("ritual")
step(60 * 5)
local hp = game.health.hp
w.pos = world.move(w.pos, 0, 8, 0)
w.pos.alt = world.ground_height(w.pos)
step(2)
expect(not game.ritual.active and game.health.hp < hp and said("broken"), "broken and lashed back")
-- A warded camp keeps a wolf out.
expect(choose("ward_camp"))
pack.add(game.pack, "resin", 2, 0)
mage.mana.light, mage.mana.earth = 40, 20
press("ritual")
for _ = 1, 60 * 16 do
  w.speed = 0
  step(1)
  if not game.ritual.active then break end
end
expect(game.ritual_done == "ward_camp", "warded")
local wolf = sim.spawn_creature("wolf", world.move(w.pos, 0.5, 30, 0))
local fled = false
for _ = 1, 60 * 6 do
  step(1)
  fled = fled or sim.creature(wolf).action == "flee"
end
expect(fled, "the wolf keeps out")
sim.remove_creature(wolf)
-- By the tent at night, a room unfolds.
expect(choose("unfold_room"))
w.pos = world.move(game.carpet.pos, 0, 1, 0) -- (back by the carpet)
w.pos.alt = world.ground_height(w.pos)
step(2)
press("tent")
expect(game.tent, "the tent is pitched")
expect(to(-1, 0.02), "night")
pack.add(game.pack, "cord", 2, 0)
mage.mana.void = 50
press("ritual")
for _ = 1, 60 * 31 do
  w.speed = 0
  step(1)
  if not game.ritual.active then break end
end
expect(game.ritual_done == "unfold_room" and game.home_place.open.pantry and said("A new room unfolds"), "a room")
sim.natural_weather()
demo.ecology.pause(game.ecology, false)
