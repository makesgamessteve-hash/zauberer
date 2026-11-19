-- SPDX-License-Identifier: MIT
-- The M7 exit demo, played through: in spring the mage flies north to the
-- temperate meadows, gathers healing herbs from the flowers in bloom and
-- brews a healing draught in the
-- tent's laboratory; in the workshop makes a cord and a horn lantern and
-- enchants it with light, and it burns on without the mage's mana; at dusk
-- over the drought-struck land the mage calls rain, and the dry ground
-- drinks; at the nearest deep water summons a water elemental, which
-- follows; and bonds a fox as a familiar. Every discipline grows with it.
-- (What is not gathered here - water, hides, resin, fibres - comes out of
-- the bag; gathering them is M5's.)
-- spec: discipline-progression: The M7 exit demo
scenario { name = "the disciplines" }
local game = demo.game
local pack, tent, home = demo.pack, demo.tent, demo.home
local plants = require("life.plants")
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and sim.water_at(here).loaded and mana.at(here).loaded
    and world.chunk_stage(here) == 3 then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local mage = game.magic.caster.mage
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local function said(text)
  local lines = demo.flight_lines()
  if game.tent and game.tent.inside then lines = home.hud_lines(game.home_place) end
  for _, line in ipairs(lines) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local function to_room(id)
  for _ = 1, 10 do
    if game.home_place.room == id then return true end
    input.inject("steer", 1, 0, 3)
    step(5)
  end
  return game.home_place.room == id
end

-- Spring in the north, in the morning: the carpet flies 500 km north to
-- the temperate meadows.
local day = world.time().time - world.time().time % 86400
world.set_time(day + 8 * 86400 + 9 * 3600)
local c = game.carpet
c.pos = world.move(c.pos, 0, 500000, 0)
c.pos.alt = world.ground_height(c.pos) + 40
c.speed = 0
for _ = 1, 900 do
  step(1)
  if sim.fire_at(c.pos).loaded and sim.water_at(c.pos).loaded and mana.at(c.pos).loaded
    and world.chunk_stage(c.pos) == 3 then break end
  sleep(5)
end
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
local w = game.walker
expect(w, "on foot")

-- 1. Healing herbs, gathered from the meadow flowers in bloom.
local now = world.time().time
local flowers = {}
for _, pl in ipairs(world.plants(w.pos, 250)) do
  for _, o in ipairs(plants.available(game.plants, pl, now, world.sky(pl.pos).season)) do
    if o.item == "healing_herbs" then flowers[#flowers + 1] = pl end
  end
end
expect(#flowers > 0, "meadow flowers in bloom")
local start = world.move(w.pos, 0, 0, 0)
for _, pl in ipairs(flowers) do
  if pack.count(game.pack, "healing_herbs") >= 10 then break end
  w.pos = world.move(pl.pos, 0, 0, 0)
  step(1)
  press("gather")
end
expect(pack.count(game.pack, "healing_herbs") >= 10, "herbs: " .. pack.count(game.pack, "healing_herbs"))
w.pos = start
step(2)

-- 2. The tent: a healing draught in the laboratory.
pack.add(game.pack, "boiled_water", 2, 0)
press("tent")
local t = game.tent
expect(t, "pitched")
w.pos = tent.door(t)
step(1)
press("mount")
expect(t.inside, "inside")
expect(to_room("laboratory"))
mage.mana.life = 30
press("cook")
expect(pack.count(game.pack, "healing_draught") >= 2 and said("Brewed"), "healing draughts")

-- 3. The workshop: a cord, a horn lantern, the lantern enchanted with light.
expect(to_room("workshop"))
pack.add(game.pack, "palm_fibre", 3, 0)
pack.add(game.pack, "deadwood", 1, 0)
pack.add(game.pack, "resin", 1, 0)
pack.add(game.pack, "hide", 1, 0)
local function choose(name)
  for _ = 1, 20 do
    local h = game.home_place
    local nc = #demo.crafting.crafts()
    local i = h.work or 1
    local job = i <= nc and demo.crafting.crafts()[i] or demo.artificing.enchantments()[i - nc]
    if job.name == name then return true end
    press("cloak_colour")
  end
  return false
end
expect(choose("Cord"))
press("cook")
expect(choose("Horn lantern"))
press("cook")
expect(pack.count(game.pack, "lantern") == 1, "a horn lantern")
mage.mana.light = 25
expect(choose("Lantern of perpetual light"))
press("cook")
expect(demo.artificing.working(game.artifacts, "lantern_of_light"), "the lantern is alight")
press("mount")
expect(not t.inside, "out")
w.pos = tent.door(t)
press("tent")
expect(game.tent == nil, "packed up")

-- 4. The drought, and the rain called at dusk.
sim.set_weather({ rain = 0, cloud = 0, temperature = 33, wind_east = 1, wind_north = 0 })
for _ = 1, 400 do
  local d = world.sky(w.pos).daylight
  if d > 0.05 and d < 0.45 then break end
  world.set_time(world.time().time + 120)
  step(1)
end
mage.mana.light = 0 -- (the lantern burns on the land's light alone)
mage.mana.water, mage.mana.storm = 60, 40
pack.add(game.pack, "boiled_water", 2, 0)
for _ = 1, 5 do
  if demo.ritual.rituals()[game.ritual.chosen].id == "call_rain" then break end
  press("ritual_next")
end
press("ritual")
expect(game.ritual.active, "the circle is laid at dusk")
for _ = 1, 60 * 21 do
  w.speed = 0
  step(1)
  if not game.ritual.active then break end
end
expect(game.ritual_done == "call_rain" and sim.weather().rain > 4, "rain over the land")
local before = sim.water_stats()
step(60 * 30)
local after = sim.water_stats()
expect(after.rained > before.rained and after.soaked > before.soaked,
  string.format("the land drinks: %.1f -> %.1f m3", before.soaked, after.soaked))
for _ = 1, 45 do
  world.set_time(world.time().time + 240)
  step(1)
end
expect(demo.artificing.working(game.artifacts, "lantern_of_light"), "the lantern burns on, through the night")

-- 5. At the nearest deep water, a water elemental.
local home_pos = game.home.pos
local shore, heading, dist
for ring = 1, 200 do
  for k = 0, 31 do
    local p = world.move(home_pos, k * math.pi / 16, ring * 100, 0)
    local kind, _, depth = world.water(p)
    if not shore and kind and kind ~= "river" and depth > 2 then
      shore, heading, dist = p, k * math.pi / 16, ring * 100
    end
  end
  if shore then break end
end
expect(shore, "deep water")
-- (back on the carpet, beside the folded tent's spot)
w.pos = world.move(game.carpet.pos, 0, 1, 0)
w.pos.alt = world.ground_height(w.pos)
step(1)
press("mount")
expect(game.walker == nil, "on the carpet")
-- The water's edge: back towards home from the deep water until the ground is dry.
local edge
for m = 0, dist, 5 do
  local p = world.move(home_pos, heading, dist - m, 0)
  if not world.water(p) then
    edge = p
    break
  end
end
expect(edge, "the shore")
c = game.carpet
c.pos = world.move(edge, 0, 0, 0)
c.pos.alt = world.ground_height(c.pos) + 40
for _ = 1, 900 do
  step(1)
  if sim.water_at(c.pos).loaded then break end
  sleep(5)
end
c.pos = world.move(edge, 0, 0, 0)
c.pos.alt = world.ground_height(c.pos) + 1
c.speed = 0
step(60)
press("mount")
w = game.walker
expect(w, "on foot by the water")
mage.mana.water = 40
press("summon")
local sm = game.summoning
expect(#sm.bound == 1 and sm.bound[1].summons == "water_elemental", "a water elemental from the water")

-- 6. A familiar: a fox comes by, offered meat and life mana.
local fox = sim.spawn_creature("fox", world.move(w.pos, w.heading, 4, 0))
pack.add(game.pack, "raw_meat", 1, 0)
mage.mana.life = 20
press("bond")
expect(sm.familiar and sm.familiar.creature == fox and said("A fox is your familiar"), "a familiar")

-- Every discipline grew.
local d = game.disciplines
for _, name in ipairs({ "alchemy", "artificing", "ritual", "summoning" }) do
  expect(d.xp[name] > 0, name .. " practised")
end
sim.natural_weather()
demo.ecology.pause(game.ecology, false)
