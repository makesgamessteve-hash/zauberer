-- SPDX-License-Identifier: MIT
-- The M6 exit demo, played through: the mage, cloaked in drab, walks past a
-- wary herd of deer within 45 m unseen (shifted to vivid, the herd sees and
-- flees); a blizzard blows for six hours and the cloak keeps the mage warm;
-- in the storm the carpet is pitched as a tent and the mage steps inside into
-- warm rooms, where goods go onto the vault's shelves; then the tent is
-- packed up, the carpet flies 5 km on, and the goods come out of the vault
-- through the bag for void mana.
-- spec: carpet-tent: The M6 exit demo
scenario { name = "carpet and home" }
local game = demo.game
local tent, bag, pack = demo.tent, demo.bag, demo.pack
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.fire_at(here).loaded and sim.water_at(here).loaded then break end
  sleep(5)
end
demo.ecology.pause(game.ecology, true)
world.set_time(world.time().time - world.time().time % 86400 + 43200) -- noon
local LAPSE = 240
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

-- Off the carpet, into the cloak, drab.
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 1
step(2)
press("mount")
local w = game.walker
expect(w ~= nil, "on foot")
press("cloak")
local k = game.cloak
while k.colour ~= "drab" do press("cloak_colour") end
expect(k.worn and said("Cloaked (drab)"), "cloaked in drab")

-- 1. A wary herd: five deer 50 m to the side of the mage's way (beyond
-- where they would hear the quiet mage).
local start = world.move(w.pos, 0, 0, 0)
local herd = {}
for i = 1, 5 do
  herd[i] = sim.spawn_creature("deer", world.move(start, w.heading, 30 + i * 3, 52 + (i % 2) * 3))
end
step(30)
local seen, fled = 0, 0
for m = 0, 60, 1.6 / 6 do -- walking past, 1.6 m/s
  w.pos = world.move(start, w.heading, m, 0)
  w.pos.alt = world.ground_height(w.pos)
  w.speed = 1.6
  step(10)
  for _, id in ipairs(herd) do
    local d = sim.creature(id)
    seen = seen + (d.sees_player and 1 or 0)
    fled = fled + (d.action == "flee" and 1 or 0)
  end
end
expect(seen == 0 and fled == 0, "past the herd unseen: seen " .. seen .. ", fled " .. fled)
-- (The same mage shifted to vivid, closer, where the herd can see: it flees.)
w.pos = world.move(start, w.heading, 40, 25)
w.pos.alt = world.ground_height(w.pos)
while k.colour ~= "vivid" do press("cloak_colour") end
step(60)
fled = 0
for _, id in ipairs(herd) do fled = fled + (sim.creature(id).action == "flee" and 1 or 0) end
expect(fled >= 3, "in vivid, the herd sees and flees: " .. fled)
for _, id in ipairs(herd) do sim.remove_creature(id) end

-- 2. A blizzard: six hours of it, walking in the cloak.
sim.set_weather({ temperature = -18, wind_east = 14, wind_north = 0, snow = 6, rain = 0, cloud = 1, storm = 0 })
while k.colour ~= "drab" do press("cloak_colour") end
local s = game.survival
local hp = game.health.hp
local coldest = s.core
pack.add(game.pack, "cooked_meat", 3, 0)
pack.add(game.pack, "boiled_water", 6, 0)
local stop = world.time().time + 6 * 3600
while world.time().time < stop do
  world.set_time(world.time().time + LAPSE)
  w.speed = 1.6
  step(1)
  coldest = math.min(coldest, s.core)
  if s.food < 50 then press("eat") end
  if s.water < 50 then press("drink") end
end
expect(coldest >= 35.5 and game.health.hp >= hp - 5, "warm in the cloak: " .. coldest .. ", health " .. game.health.hp)
expect(not said("freezing"))

-- 3. The tent, pitched in the storm; inside, warm rooms.
press("cloak") -- (spread out beside the mage)
expect(not k.worn)
w = game.walker
local pitched = false
for turn = 0, 7 do
  w.heading = turn * math.pi / 4
  press("tent")
  if game.tent then
    pitched = true
    break
  end
  c = game.carpet
  w.pos = world.move(c.pos, 0, 0, 1.2)
  w.pos.alt = world.ground_height(w.pos)
end
expect(pitched, "pitched in the storm")
local t = game.tent
w.pos = tent.door(t)
step(2)
press("mount")
expect(t.inside, "inside")
s.core = 35.6 -- (cold from the storm outside)
stop = world.time().time + 4 * 3600
while world.time().time < stop do
  world.set_time(world.time().time + LAPSE)
  step(1)
end
expect(s.core > 36.5, "warm inside: " .. s.core)
expect(sim.weather().temperature == -18, "while it freezes outside")
-- Goods onto the vault's shelves.
for _ = 1, 5 do
  if game.home_place.room ~= "vault" then
    input.inject("steer", 1, 0, 3)
    step(5)
  end
end
expect(game.home_place.room == "vault", "in the vault")
pack.add(game.pack, "cooked_meat", 6, 0)
pack.add(game.pack, "boiled_water", 4, 0)
local meat, water = pack.count(game.pack, "cooked_meat"), pack.count(game.pack, "boiled_water")
press("stow")
expect(bag.in_vault(game.bag, "cooked_meat") == meat and bag.in_vault(game.bag, "boiled_water") == water, "shelved")
expect(pack.weight(game.pack) == 0)

-- 4. Out, packed up, and 5 km on: the goods out of the vault through the bag.
press("mount")
expect(not t.inside)
w = game.walker
w.pos = tent.door(t)
step(1)
press("tent")
expect(game.tent == nil, "packed up")
c = game.carpet
w.pos = world.move(c.pos, 0, 0.5, 0)
w.pos.alt = world.ground_height(w.pos)
step(1)
press("mount")
expect(game.walker == nil, "on the carpet")
sim.natural_weather()
local from = world.move(c.pos, 0, 0, 0)
c.pos = world.move(from, 1.2, 5000, 0)
c.pos.alt = world.ground_height(c.pos) + 40
for _ = 1, 900 do
  step(1)
  if sim.fire_at(c.pos).loaded then break end
  sleep(5)
end
expect(world.distance(c.pos, from) > 4900, "5 km away")
local mage = game.magic.caster.mage
mage.mana.void = 5
press("unstow")
expect(pack.count(game.pack, "cooked_meat") == meat and mage.mana.void == 5 - bag.REACH_MANA,
  "the meat through the bag")
expect(said("You reach into the vault"))
step(60 * bag.REACH_SECONDS + 2)
press("stow")
press("unstow")
expect(pack.count(game.pack, "boiled_water") == water, "and the water")
demo.ecology.pause(game.ecology, false)
