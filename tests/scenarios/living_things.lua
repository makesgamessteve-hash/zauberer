-- SPDX-License-Identifier: MIT
-- The M5 exit demo, played through in a created world: seven days living off
-- the land in two contrasting places on one river, a tropical cloud forest
-- and a reed estuary downstream. The mage hunts with Fireball, forages
-- coconuts, roots and firewood, lays and lights campfires, cooks, boils water,
-- eats, drinks and sleeps by the fire. The days pass in a time-lapse (four
-- game minutes a tick, all of it lived: hunger, thirst, tiredness, warmth and
-- spoiling count it; the fire simulation runs in ticks, so a campfire lasts
-- the stay), in a fair week. One evening a hungry wolf downwind follows the
-- mage's scent up the wind while one as far upwind never comes. In the
-- estuary, out of clean water, the mage drinks from the river unboiled, falls
-- sick, and cures it with boiled water and bark tea steeped from the forest's
-- kapok bark.
-- spec: survival: The M5 exit demo
scenario { name = "living things" }
local game = demo.game
local pack = demo.pack
local cooking = demo.cooking
local codex = demo.codex
local items = require("life.items")
local plants = require("life.plants")
local smalllife = require("life.smalllife")

local HOUR = 3600
local LAPSE = 240 -- game seconds a tick in the time-lapse

-- ---- A world of its own ----
game.world_options = { radius = 400000, quality = "quick", seed = 11 }
game.set_mode("create")
for _ = 1, 20000 do
  step(1)
  if game.creation.progress and game.creation.progress.done then break end
  sleep(2)
end
expect(game.fly_in_new_world(), "the world was made")
local T0 = world.time().time
local s = game.survival
local mage = game.magic.caster.mage

local function wait_loaded(pos)
  for _ = 1, 1200 do
    step(1)
    if sim.water_at(pos).loaded and sim.fire_at(pos).loaded and mana.at(pos).loaded and world.chunk_stage(pos) == 3 then
      return true
    end
    sleep(5)
  end
  return false
end

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

-- The bank of the river nearest home beside a biome: the first dry ground
-- walking from the river towards it, metre by metre.
local function bank_by(biome)
  local home = game.home.pos
  for ring = 1, 150 do
    local r = ring * 500
    local n = math.floor(2 * math.pi * r / 500)
    for k = 0, n - 1 do
      local p = world.move(home, k * 2 * math.pi / n, r, 0)
      if world.water(p) == "river" then
        for j = 0, 7 do
          local h = j * math.pi / 4
          local q = world.move(p, h, 250, 0)
          if world.biome(q) == biome and not world.water(q) then
            for m = 1, 250 do
              local x = world.move(p, h, m, 0)
              if not world.water(x) then
                x.alt = world.ground_height(x)
                return x, h
              end
            end
          end
        end
      end
    end
  end
  return nil
end

-- Flies the carpet to a place (a scripted flight) and waits for the world.
local function fly_to(pos, height)
  local c = game.carpet
  c.pos = world.move(pos, 0, 0, 0)
  c.pos.alt = world.ground_height(c.pos) + (height or 30)
  c.speed = 0
  expect(wait_loaded(c.pos), "the world streams in")
end

local function step_off()
  local c = game.carpet
  c.pos.alt = world.ground_height(c.pos) + 1
  c.speed = 0
  step(2)
  press("mount")
  expect(game.walker ~= nil, "on foot")
end

-- Walks to a place (the path is not the point).
local function walk_to(pos)
  local w = game.walker
  w.pos = world.move(pos, 0, 0, 0)
  w.pos.alt = world.ground_height(w.pos)
  step(2)
end

local forest, forest_land = bank_by("cloud forest")
local estuary, estuary_land = bank_by("estuary")
expect(forest and estuary, "a forest bank and an estuary bank on the river")

-- ---- Hunting ----

-- Fireball from the carpet, 30 m off, leading the quarry (as in Slice A).
local function hunt(radius)
  local best, bd
  for _, id in ipairs(sim.creatures_near(game.carpet.pos, radius)) do
    local cr = sim.creature(id)
    local sp = cr and cr.alive and demo.fauna.species()[cr.species]
    if sp and sp.products and sp.products.raw_meat and sp.diet ~= "carnivore" then
      local d = world.distance(cr.pos, game.carpet.pos)
      if not best or sp.products.raw_meat > bd.meat or (sp.products.raw_meat == bd.meat and d < bd.d) then
        best, bd = id, { meat = sp.products.raw_meat, d = d }
      end
    end
  end
  if not best then return nil end
  local function take_aim()
    local cr = sim.creature(best)
    local d = 30
    local t = d / 40 + 0.05
    local ahead = world.move(cr.pos, cr.heading or 0, (cr.speed or 0) * t, 0)
    local c = game.carpet
    local from = world.move(ahead, math.pi + (cr.heading or 0), d, 0)
    c.pos = from
    local bp, cp = world.to_local(ahead), world.to_local(from)
    c.heading, c.speed = math.atan(bp[1] - cp[1], -(bp[3] - cp[3])), 0
    c.pos.alt = ahead.alt + 0.6 + d * math.tan(-game.rider.pitch) + 0.5 * 9.81 * t * t - 1
  end
  fly_to(sim.creature(best).pos, 10)
  for _ = 1, 8 do
    mage.mana.fire = 40
    input.inject("cast_left", 1, 0, 60)
    for _ = 1, 62 do
      take_aim()
      step(1)
    end
    for _ = 1, 60 do
      step(1)
      if not sim.creature(best).alive then break end
    end
    if not sim.creature(best).alive then break end
  end
  local cr = sim.creature(best)
  return not cr.alive and best or nil, cr.species
end

-- ---- Camp life ----

-- A camp on a river bank: the water's edge, the hearth 4 m inland (off the
-- wet ground: the fire cells are 2 m), and a seat by the fire, picked once it
-- burns (close enough to cook and keep warm, not on hot or burning ground).
-- river: heading to the water.
local camp
local function make_camp(bank, land)
  local function inland(m)
    local p = world.move(bank, land, m, 0)
    p.alt = world.ground_height(p)
    return p
  end
  return { bank = bank, shade = inland(1.5), pos = inland(2), lay = inland(3.4), hearth = inland(4),
    kindle = inland(5.8), river = land + math.pi }
end

local function find_seat()
  for _, r in ipairs({ 2.4, 2.0, 2.8 }) do
    for k = 0, 15 do
      local p = world.move(camp.hearth, camp.river + k * math.pi / 8, r, 0)
      p.alt = world.ground_height(p)
      local c = sim.fire_at(p)
      if c.loaded and not c.burning and not c.water and c.temperature < 50 and cooking.fire_near(p)
        and demo.survival.sense(p, game.walker, 0).fire then
        return p
      end
    end
  end
  return nil
end

-- What the pack holds: the sum over its stacks of worth(item, stack) per item.
local function packed(worth)
  local n = 0
  for id, st in pairs(game.pack.stacks) do n = n + st.count * worth(items.get(id), st) end
  return n
end
local function fuel_kg() return packed(function(it) return it.fuel or 0 end) end
local function herbs() return packed(function(it) return it.brew and 1 or 0 end) end
local function kcal()
  return packed(function(it, st)
    return it.kind == "food" and (it.poison or 0) == 0 and st.load < smalllife.SPOILT and (it.kcal or 0) or 0
  end)
end

-- Walks to the plants within radius offering what is wanted, the best first
-- (want gives an item's worth; nearer when equal), and gathers at each (a few
-- times: a plant gives one part at a time) until there is enough.
local function forage(want, have, enough, radius)
  local now = world.time().time
  local list = {}
  for _, pl in ipairs(world.plants(camp.pos, radius or 250)) do
    local worth = 0
    for _, o in ipairs(plants.available(game.plants, pl, now, world.sky(pl.pos).season)) do
      local by_hand = not o.whole or pl.layer ~= "tree" or pl.species == "snag"
      local w = by_hand and want(items.get(o.item)) or 0
      if type(w) ~= "number" then w = w and 1 or 0 end
      worth = math.max(worth, w * o.count)
    end
    if worth > 0 then list[#list + 1] = { pl = pl, worth = worth, d = world.distance(pl.pos, camp.pos) } end
  end
  table.sort(list, function(a, b) return a.worth > b.worth or (a.worth == b.worth and a.d < b.d) end)
  for i, e in ipairs(list) do
    if have() >= enough or i > 60 then break end
    walk_to(e.pl.pos)
    game.walker.heading = camp.river + math.pi -- (away from the water)
    for _ = 1, 4 do
      local before = have()
      press("gather")
      if have() > before then break end
    end
  end
  return have() >= enough
end

-- Fills cups at the bank, facing the river (the plants about wait).
local function fetch_water(times)
  walk_to(camp.bank)
  game.walker.heading = camp.river
  for _ = 1, times do press("gather") end
  walk_to(camp.pos)
end

local function at_fire()
  walk_to(camp.pos)
end

-- A campfire at the camp, laid from the pack's firewood and lit with Kindle
-- held long enough to reach it from beyond it (a long Kindle heats the
-- caster's own ground too, so the seat is on the other side); true when it
-- burns and a seat by it is found.
local function make_fire()
  local w = game.walker
  if cooking.fire_near(camp.hearth) and camp.seated then
    at_fire()
    return true
  end
  if fuel_kg() < 1 then forage(function(it) return it.fuel end, fuel_kg, 2) end
  walk_to(camp.lay)
  w.heading = camp.river + math.pi
  press("campfire")
  local lit = false
  for _ = 1, 3 do
    walk_to(camp.kindle) -- beyond the hearth, facing it
    w.heading = camp.river
    mage.mana.fire = math.max(mage.mana.fire, 30) -- (channelled as a player would)
    input.inject("cast_right", 1, 0, 70) -- Kindle, held
    step(72)
    for _ = 1, 60 * 3 do
      step(1)
      if cooking.fire_near(camp.hearth) then
        lit = true
        break
      end
    end
    if lit then break end
  end
  local seat = lit and find_seat()
  if seat then
    camp.pos, camp.seated = seat, true
  end
  at_fire()
  return seat ~= nil
end

local function cook()
  at_fire()
  press("cook")
end

-- Lives on at camp until a time (by the fire, in its smoke, once it burns),
-- eating and drinking when hungry or thirsty; false if the mage went down.
local function live_until(t)
  walk_to(camp.seated and camp.pos or camp.shade)
  while world.time().time < t - 1 do
    world.set_time(math.min(world.time().time + LAPSE, t))
    step(1)
    if game.health.down then return false end
    if s.food < 55 then press("eat") end
    if s.water < 55 then press("drink") end
  end
  return true
end

local function sleep_night()
  make_fire()
  at_fire()
  game.slept = nil
  press("sleep")
  return game.slept
end

local function report(d)
  print(string.format("day %d: food %.0f, water %.0f, rest %.0f, %.1f C, sick %.2f, health %.0f, pack %.1f kg", d + 1,
    s.food, s.water, s.rest, s.core, s.sick, game.health.hp, pack.weight(game.pack)))
end

-- A day at camp: water fetched and boiled, food found and cooked, the day
-- lived, a night by the fire. d: the day (0 the first).
local function a_day(d, evening)
  local morning = T0 + d * 86400
  if kcal() < 2500 then
    forage(function(it) return it.kind == "food" and (it.poison or 0) == 0 and it.kcal or false end, kcal, 3000)
  end
  fetch_water(2)
  expect(make_fire(), "the campfire burns on day " .. (d + 1))
  cook()
  pack.take(game.pack, "kapok_fibre", 999) -- (nothing to do with it yet)
  expect(live_until(morning + 4 * HOUR), "alive at noon on day " .. (d + 1))
  fetch_water(1)
  make_fire()
  cook()
  expect(live_until(morning + 8 * HOUR), "alive in the afternoon of day " .. (d + 1))
  fetch_water(1)
  make_fire()
  cook()
  expect(live_until(morning + 11 * HOUR), "alive in the evening of day " .. (d + 1))
  if evening then evening() end
  expect(live_until(morning + 13 * HOUR), "alive at night on day " .. (d + 1))
  expect(sleep_night(), "slept on day " .. (d + 1))
  expect(live_until(morning + 86400), "alive the morning after day " .. (d + 1))
  report(d)
end

-- One evening a breeze along the bank carries the mage's scent: a hungry wolf
-- downwind follows it up the wind; one as far upwind never notices.
local function scent_evening()
  local along = camp.river + math.pi / 2
  local function spot(h)
    for _, dh in ipairs({ 0, 0.3, -0.3, 0.6, -0.6, 0.9, -0.9 }) do
      local p = world.move(camp.pos, h + dh, 130, 0)
      if not world.water(p) then return p end
    end
  end
  local down, up = spot(along), spot(along + math.pi)
  expect(down and up, "dry ground both ways along the bank")
  local dp, mp = world.to_local(down), world.to_local(camp.pos)
  local de, dn = dp[1] - mp[1], -(dp[3] - mp[3])
  local dl = math.sqrt(de * de + dn * dn)
  local weather = sim.weather()
  sim.set_weather({ cloud = 0.2, rain = 0, snow = 0, storm = 0, wind_east = 4 * de / dl, wind_north = 4 * dn / dl,
    temperature = weather.temperature }, 3600)
  at_fire()
  step(60 * 10) -- the scent drifts downwind
  local a = sim.spawn_creature("wolf", down)
  local b = sim.spawn_creature("wolf", up)
  for _, id in ipairs({ a, b }) do sim.set_needs(id, { hunger = 1, thirst = 0, energy = 1 }) end
  local nearest_a, nearest_b, tracked = 1e9, 1e9, false
  for _ = 1, 60 * 90 do
    step(1)
    expect(game.walker, "still on foot")
    local wa, wb = sim.creature(a), sim.creature(b)
    nearest_a = math.min(nearest_a, world.distance(wa.pos, game.walker.pos))
    nearest_b = math.min(nearest_b, world.distance(wb.pos, game.walker.pos))
    tracked = tracked or wa.action == "investigate" or wa.action == "hunt"
    if tracked and nearest_a < 80 then break end
  end
  print(string.format("scent: downwind wolf came to %.0f m (tracked %s), upwind wolf to %.0f m", nearest_a,
    tostring(tracked), nearest_b))
  expect(tracked and nearest_a < 90, "the wolf downwind followed the scent up the wind")
  expect(nearest_b > 100, "the wolf upwind never came")
  sim.remove_creature(a) -- (it slinks off from the fire)
  sim.remove_creature(b)
end

-- ---- Days 1 to 3: the cloud forest ----
camp = make_camp(forest, forest_land)
fly_to(camp.pos, 30)
-- A fair week (a campfire will not light in a downpour), as warm as the place.
sim.set_weather({ cloud = 0.3, rain = 0, snow = 0, storm = 0, temperature = world.climate(camp.pos) })
step(60 * 3) -- the land's animals come out
local quarry, species = hunt(800)
expect(quarry, "a kill in the forest")
local carcass = sim.creature(quarry).pos
fly_to(carcass, 1)
step_off()
walk_to(carcass)
press("gather")
expect(pack.count(game.pack, "raw_meat") > 0, "butchered a " .. tostring(species))
pack.take(game.pack, "hide", 9) -- (left behind: too heavy to carry about)
expect(forage(function(it) return it.fuel end, fuel_kg, 4), "firewood")
a_day(0)
a_day(1, scent_evening)
a_day(2)

-- ---- Day 4: down the river to the estuary ----
-- The last forest morning: no water (it is heavy, and there is a river
-- below), nothing spoilt, food for the road cooked, and kapok bark (should a
-- remedy be needed).
for id, st in pairs(game.pack.stacks) do
  local it = items.get(id)
  if it.kind == "water" or (it.kind == "material" and not it.fuel) or st.load >= smalllife.SPOILT then
    pack.take(game.pack, id, 999)
  end
end
expect(forage(function(it) return it.brew end, herbs, 4), "kapok bark for the road")
forage(function(it) return it.kind == "food" and (it.poison or 0) == 0 and it.kcal or false end, kcal, 1800)
make_fire()
cook()
pack.take(game.pack, "coconut", math.max(pack.count(game.pack, "coconut") - 4, 0)) -- (as many as can be carried)
for id in pairs(game.pack.stacks) do
  local it = items.get(id)
  if it.kind == "material" and not it.fuel then pack.take(game.pack, id, 999) end
end
walk_to(game.carpet.pos)
press("mount")
expect(game.walker == nil, "on the carpet")
camp = make_camp(estuary, estuary_land)
fly_to(camp.pos, 30)
sim.set_weather({ cloud = 0.3, rain = 0, snow = 0, storm = 0, temperature = world.climate(camp.pos) })
step_off()
-- Thirsty, and no fire laid yet: the mage drinks from the river unboiled.
fetch_water(1)
expect(pack.count(game.pack, "swamp_water") == 4, "swamp water from the estuary")
-- The mage drinks their fill, in the morning (before the mosquitoes are out).
local fell_ill = false
for round = 1, 4 do
  if round > 1 then
    expect(live_until(world.time().time + 2 * HOUR), "alive in the estuary")
    fetch_water(1)
  end
  walk_to(camp.shade)
  for _ = 1, 4 do
    press("drink")
    if s.sick > 0 then break end
  end
  if s.sick > 0 then
    fell_ill = news("You feel sick (Swamp water)")
    break
  end
end
expect(fell_ill, "sick from the swamp water")
print(string.format("sick at %.1f h into day 4: %.2f", (world.time().time - T0 - 3 * 86400) / HOUR, s.sick))
-- The cure: a fire of reeds, the water boiled, the bark steeped.
expect(make_fire(), "a fire of reeds")
fetch_water(2)
cook()
expect(pack.count(game.pack, "bark_tea") > 0 and pack.count(game.pack, "boiled_water") > 0, "boiled water and bark tea")
for id in pairs(game.pack.stacks) do
  if items.get(id).dirty then pack.take(game.pack, id, 999) end -- (no more swamp water unboiled)
end
press("drink")
expect(s.treated and news("You drink bark tea"), "the remedy taken")
local cured_on
for d = 3, 6 do
  a_day(d)
  if s.sick == 0 and not cured_on then cured_on = d + 1 end
end
-- ---- Seven days lived ----
expect(cured_on and cured_on <= 6, "cured within two days: day " .. tostring(cured_on))
expect(world.time().time >= T0 + 7 * 86400, "seven days have passed")
expect(not game.health.down and game.health.hp > 50, "alive and well: " .. game.health.hp)
expect(s.food > 0 and s.water > 0 and s.sick == 0, "fed, watered and well")
local n = codex.counts(game.codex)
print(string.format("codex: %d seen, %d observed, %d studied, %d understood", n[1], n[2], n[3], n[4]))
expect(n[3] >= 3, "the land's plants and animals studied along the way")
