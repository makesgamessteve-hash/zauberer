-- SPDX-License-Identifier: MIT
-- Harvest and cooking in the game: on foot the mage gathers from the plants
-- in reach, butchers a hare, fills cups at a pool, lays a campfire and lights
-- it with Kindle, cooks the meat, boils the water and brews herbal tea, eats
-- and drinks; the pack has a limit, and raw meat spoils on a warm day.
-- spec: harvest-and-cooking: Harvest and cooking in the game
scenario { name = "harvest" }
local game = demo.game
local pack = demo.pack
local cooking = demo.cooking
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if #world.plants(here, 100) > 20 and sim.fire_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
local p = game.pack
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
-- Step off next to a plant with something to give.
-- spec: harvest-and-cooking: Gathering
local plants = require("life.plants")
local target
for _, pl in ipairs(world.plants(here, 120)) do
  local offers = plants.available(game.plants, pl, world.time().time, world.sky(pl.pos).season)
  for _, o in ipairs(offers) do
    if not target and not o.whole then target = pl end
  end
end
expect(target, "a plant with something to give")
local c = game.carpet
c.pos = world.move(target.pos, 0, 2, 0)
c.pos.alt = world.ground_height(c.pos) + 1
c.speed = 0
step(2)
press("mount")
expect(game.walker ~= nil, "on foot")
game.walker.pos = world.move(target.pos, 0, 0.5, 0)
game.walker.pos.alt = world.ground_height(game.walker.pos)
local before = pack.weight(p)
press("gather")
expect(pack.weight(p) > before, "gathered from the plant")
expect(news("Gathered"), "the HUD says what")
-- Butcher a hare.
local hare = sim.spawn_creature("hare", world.move(game.walker.pos, 0, 1, 0))
sim.hurt(hare, "magic", 1000)
step(2)
press("gather")
expect(pack.count(p, "raw_meat") == 3 and pack.count(p, "hide") == 1, "meat and a hide")
expect(sim.creature(hare) == nil, "the carcass is used up")
-- Fill cups at a pool.
local pool = world.move(game.walker.pos, 1.6, 4, 0)
world.deform(pool, 3, -1.5)
sim.add_water(pool, 3, 15)
step(60)
game.walker.pos = world.move(pool, 0, 0, 0)
game.walker.pos.alt = world.ground_height(pool)
local gone = {}
for _, pl in ipairs(world.plants(game.walker.pos, 4)) do gone[#gone + 1] = pl.id end
for _, id in ipairs(gone) do world.cut_plant(id) end -- nothing else within reach
press("gather")
expect(pack.count(p, "pool_water") == 4, "four cups of pool water")
-- A campfire, lit with Kindle; meat cooked, water boiled, tea brewed.
pack.add(p, "deadwood", 4, 0)
pack.add(p, "healing_herbs", 2, 0)
game.walker.pos = world.move(game.walker.pos, 0, 8, 0)
game.walker.pos.alt = world.ground_height(game.walker.pos)
step(2)
press("cook")
expect(news("no fire near"))
press("campfire")
expect(pack.count(p, "deadwood") == 0 and news("campfire"), "laid")
input.inject("cast_right", 1, 0, 30) -- the right hand holds Kindle
step(32)
local lit = false
for _ = 1, 60 * 5 do
  step(1)
  if cooking.fire_near(game.walker.pos) then lit = true break end
end
expect(lit, "the campfire burns")
press("cook")
expect(pack.count(p, "cooked_meat") == 3 and pack.count(p, "raw_meat") == 0, "meat cooked")
expect(pack.count(p, "herbal_tea") == 2 and pack.count(p, "boiled_water") == 2, "water boiled, tea brewed")
expect(news("Cooked"))
-- Eat and drink: cooked meat first, tea when sick.
local s = game.survival
s.food, s.sick = 20, 0.5
press("eat")
expect(s.food > 30 and pack.count(p, "cooked_meat") == 2, "ate cooked meat")
press("drink")
expect(s.sick < 0.5 and pack.count(p, "herbal_tea") == 1, "drank the remedy first")
expect(news("You drink herbal tea"))
-- The pack has a limit.
-- spec: harvest-and-cooking: The pack
local fit = pack.add(p, "wood", 50, 0)
expect(fit < 50 and pack.weight(p) <= pack.CARRY + 1e-9 and news("Too heavy"), "only what fits: " .. fit)
-- Raw meat spoils on a warm day; cooked keeps longer.
local q = pack.new()
pack.add(q, "raw_meat", 1, 0)
pack.add(q, "cooked_meat", 1, 0)
pack.spoil(q, 24, 25, false)
expect(pack.load(q, "raw_meat") > 0.5 and pack.load(q, "cooked_meat") < pack.load(q, "raw_meat"), "spoilt")
local back = pack.new(pack.snapshot(p))
expect(pack.weight(back) == pack.weight(p), "kept with the game")
demo.ecology.pause(game.ecology, false)
