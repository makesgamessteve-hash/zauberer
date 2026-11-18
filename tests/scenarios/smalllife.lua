-- SPDX-License-Identifier: MIT
-- Small life in the game: mushrooms in the woods and meadows on an autumn
-- day after rain, picked and back the next day; the sea is salty, a still
-- pool dirty and dirtier by carrion; a carcass rots away in three days;
-- bees by the flowers on summer days, fireflies on summer nights, mosquitoes
-- over wet ground at dusk that bite.
-- spec: small-life: Small life in the game
scenario { name = "small life" }
local smalllife = demo.smalllife
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if #world.plants(here, 150) > 50 and sim.water_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local state = game.smalllife
local function at_season(from, to, light_from, light_to)
  local t = world.time().time
  for hour = 0, 48 * 24, 1 do
    world.set_time(t + hour * 3600)
    local sky = world.sky(here)
    if sky.season >= from and sky.season < to and sky.daylight >= light_from and sky.daylight <= light_to then
      return true
    end
  end
  return false
end
-- Autumn after rain: mushrooms.
expect(at_season(2.0, 2.6, 0.5, 1), "an autumn day")
sim.set_weather({ rain = 4, cloud = 1, temperature = 12 }, 3600)
step(10)
local found, at = nil, nil
for r = 0, 160, 32 do
  for k = 0, 7 do
    local p = world.move(here, k * math.pi / 4, r, 0)
    local list = smalllife.mushrooms(state, p, world.time().time)
    if #list > 0 and not found then found, at = list, p end
  end
end
expect(found, "mushrooms somewhere near")
local picked = smalllife.pick(state, at, world.time().time)
local n = 0
for item, count in pairs(picked) do
  expect(require("life.items").get(item), "a known item")
  n = n + count
end
expect(n > 0)
expect(smalllife.pick(state, at, world.time().time) == nil, "picked clean today")
local snap = smalllife.snapshot(state, world.time().time)
expect(next(snap.picked) ~= nil, "kept with the game")
expect(#smalllife.mushrooms(demo.smalllife.new(snap), at, world.time().time) == 0, "and still picked after loading")
world.set_time(world.time().time + 86400)
step(2)
local _, key = smalllife.mushrooms(state, at, world.time().time)
expect(state.picked[key] ~= math.floor(world.time().time / 86400), "the next day the patch may fruit again")
sim.natural_weather()
-- Water: the sea is salty; a still pool is dirty, more so by carrion.
local sea
for r = 1, 200 do
  for k = 0, 15 do
    local p = world.move(here, k * math.pi / 8, r * 100, 0)
    if not sea and world.water(p) == "ocean" then sea = p end
  end
  if sea then break end
end
expect(sea and smalllife.water(sea).salty, "the sea is salty")
local pool = world.move(here, 1.2, 50, 0)
world.deform(pool, 5, -2)
sim.add_water(pool, 4, 40)
step(60)
local w = smalllife.water(pool)
expect(w and w.kind == "pool" and not w.salty and w.dirty >= 0.4, "a still pool")
local deer = sim.spawn_creature("deer", world.move(pool, 0, 8, 0))
sim.hurt(deer, "magic", 1000)
step(2)
expect(smalllife.water(pool).dirty > w.dirty, "carrion fouls it")
-- The carcass rots away in three days.
local t = world.time().time
world.set_time(t + 4 * 86400)
step(5)
expect(sim.creature(deer) == nil, "rotted away")
-- Bees by what is in flower on a spring day, fireflies on summer nights;
-- mosquitoes over the pool at dusk, and they bite.
local plants = require("life.plants")
expect(at_season(0.35, 0.55, 0.7, 1), "a spring day")
local season = world.sky(here).season
local flowers
for _, p in ipairs(world.plants(here, 250)) do
  if not flowers and p.species ~= "grass" and plants.stage(p, season).flowers then flowers = p.pos end
end
expect(flowers, "something in flower near")
game.carpet.pos = world.move(flowers, 0, 0, 0)
game.carpet.pos.alt = world.ground_height(flowers) + 2
sim.set_weather({ rain = 0, cloud = 0, temperature = 24 }, 3600)
step(60 * 3)
expect(smalllife.swarms(state, "bee") > 0, "bees")
expect(smalllife.pollination(state, flowers) == 1, "pollinating")
local sights = smalllife.sights(state)
expect(#sights > 0 and sights[1].pos[2] > world.to_local(flowers)[2] - 5, "insects to draw near the ground")
expect(at_season(0.9, 1.6, 0, 0.05), "a summer night")
step(60 * 3)
expect(smalllife.swarms(state, "firefly") > 0, "fireflies")
expect(smalllife.swarms(state, "bee") == 0, "no bees at night")
game.carpet.pos = world.move(pool, 0, 0, 0)
game.carpet.pos.alt = world.ground_height(pool) + 2
expect(at_season(0.9, 1.6, 0.1, 0.45), "a summer dusk")
step(60 * 3)
expect(smalllife.swarms(state, "mosquito") > 0, "mosquitoes over the pool")
local bites = 0
for _, sw in ipairs(state.swarms) do
  if sw.kind == "mosquito" then bites = math.max(bites, smalllife.bites(state, sw.anchor)) end
end
expect(bites > 0, "they bite")
sim.natural_weather()
demo.ecology.pause(game.ecology, false)
