-- SPDX-License-Identifier: MIT
-- The people of Anarvan and the economy: households of named people with
-- their trades and faiths; their day (sleep, prayer, work, the square,
-- shelter from rain, buckets to a fire); how they see the heirs; what they
-- say by topic and their services; prices by supply and demand, factions
-- and standing, caravans on the trade routes.
scenario { name = "people" }
local game = demo.game
local places = demo.places
local npcs = require("people.npcs")
local folk = require("people.folk")
local economy = require("people.economy")
local history = require("people.history")
local pack = require("player.pack")
local wp = game.world_places
local P = wp.places
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if P.ready and world.chunk_stage(here) == 3 then break end
  sleep(5)
end
step(3)
local anarvan = places.find(P, here, "Anarvan")
local plan = P.plans[anarvan.id]
local db = P.db

-- spec: npcs: People of a place
do
  expect(plan.people and #plan.people > 20, "Anarvan's households")
  local roles, trades = {}, {}
  for _, p in ipairs(plan.people) do
    roles[p.role] = true
    trades[p.profession] = true
    expect(p.name:match("^%u%l+$") and p.age and p.faith and #p.traits >= 1 and p.home, p.id)
    expect(p.role ~= "child" or p.age < 17)
  end
  expect(roles.parent and roles.child, "families")
  expect(trades.weaver, "weavers in Anarvan")
  local again = npcs.populate(db, plan)
  expect_eq(again[1].name, plan.people[1].name, "the same people every time")
end

-- spec: npcs: The day of a place
do
  local p
  for _, q in ipairs(plan.people) do if q.role == "parent" and not p then p = q end end
  expect_eq(npcs.activity(db, p, 2, 1, {}).what, "sleep")
  expect_eq(npcs.activity(db, p, 7, 1, {}).what, "eat")
  local work = npcs.activity(db, p, 10, 1, {})
  expect(work.what == "work" or work.what == "shop" or work.what == "pray")
  local faith = history.get(db, p.faith)
  local hours = ({ ["at dawn and dusk"] = 18, ["three times a day"] = 18, ["five times a day"] = 15 })[faith.prayers]
  if hours then expect_eq(npcs.activity(db, p, hours, 1, {}).what, "pray") end
  expect_eq(npcs.activity(db, p, 10, p.festival_day, {}).what, "festival")
  expect_eq(npcs.activity(db, p, 10, 1, { fire = "x" }).what, "fight fire")
  expect(npcs.activity(db, p, 10, 1, { raid = true }).at == p.home)
  local child
  for _, q in ipairs(plan.people) do if q.role == "child" and not child then child = q end end
  expect_eq(npcs.activity(db, child, 10, 1, {}).at, "plaza")
  expect_eq(npcs.activity(db, child, 10, 1, { rain = true }).what, "shelter")
  -- In play they walk there.
  world.set_time(math.floor(world.time().time / 86400) * 86400 + 10 * 3600)
  for _ = 1, 60 * 20 do step(1) end
  local moved = 0
  for _, q in ipairs(plan.people) do if q.pos and q.goal then moved = moved + 1 end end
  expect(moved == #plan.people, "everyone is somewhere")
  local on_square = 0
  for _, q in ipairs(plan.people) do
    if q.at == "plaza" and world.distance(q.pos, anarvan.pos) < plan.plaza.r + 2 then on_square = on_square + 1 end
  end
  expect(on_square >= 1, "children play on the square")
end

-- spec: npcs: How people see the heirs
do
  local p = plan.people[1]
  local base = npcs.attitude(db, p, {})
  local order = { hostile = 1, afraid = 2, wary = 3, neutral = 4, warm = 5 }
  expect(order[npcs.attitude(db, p, { standing = 60 })] >= order[base])
  expect(order[npcs.attitude(db, p, { guest = true })] < order[base], "breaking guest right is not forgiven")
  local f = history.get(db, p.faith)
  if f.magic == "an abomination" then
    expect(order[npcs.attitude(db, p, { magic = true })] <= 2, "magic seen where it is an abomination")
  end
  expect(type(npcs.greet(p, "warm")) == "string" and npcs.greet(p, "nonsense"))
end

-- spec: npcs: Talking by topics
do
  local p = plan.people[2]
  local seen = {}
  local first = folk.talk(wp.folk, P, p, seen)
  expect(first:find(p.name, 1, true) and wp.folk.memory.met[p.id], "met and remembered")
  local said = {}
  for _ = 1, 8 do
    local line, _, topic = folk.talk(wp.folk, P, p, seen)
    said[topic or "?"] = line
  end
  expect(said.directions and said.directions:find("to the", 1, true), "directions: " .. tostring(said.directions))
  expect(said.prices and said.prices:find("coins", 1, true))
  expect(said.history and said.faith and said.work)
  local how, dir, d = npcs.directions(anarvan.pos, world.move(anarvan.pos, math.pi / 2, 50000, 0, 0))
  expect(dir == "east" and d > 49000 and how:find("days' ride", 1, true), how)
  expect(npcs.directions(anarvan.pos, world.move(anarvan.pos, 0, 900, 0, 0)):find("short walk", 1, true))
  expect(npcs.directions(anarvan.pos, world.move(anarvan.pos, 0, 9000, 0, 0)):find("hours' walk", 1, true))
  expect_eq(npcs.talk(db, p, "directions", {}), "Where to? I know only these hills.")
  expect(npcs.talk(db, p, "nothing", {}) == "I know nothing of that.")
  local back = npcs.new(npcs.snapshot(wp.folk.memory))
  expect(back.met[p.id].times >= 9)
  expect(next(npcs.new({ met = { [1] = {} } }).met) == nil)
end

-- spec: npcs: Requests from the world
do
  local reqs = npcs.requests(db, anarvan, { predators = true, fire_dir = "west", dry = true, bandits_near = "Kand",
    sick_well = true })
  local kinds = {}
  for _, r in ipairs(reqs) do kinds[r.kind] = r end
  expect(kinds.predator and kinds.wildfire and kinds.drought and kinds.bandits and kinds.poisoned_well)
  expect(kinds.drought.ritual == "call_rain" and kinds.poisoned_well.clue and kinds.wildfire.text:find("west"))
  local lost = false
  for week = 0, 30 do
    for _, r in ipairs(npcs.requests(db, anarvan, { week = week })) do lost = lost or r.kind == "lost_child" end
  end
  expect(lost, "a lost child, some week")
end

-- spec: npcs: Services
do
  local p = { id = "test:inn", name = "Rahim", profession = "innkeeper", settlement = anarvan.id,
    culture = anarvan.culture, traits = { "kind" }, pos = anarvan.pos }
  local rested, healed, learnt = 0, 0, nil
  local heir = { pack = pack.new(), rest = function(h) rested = h end, heal = function(n) healed = n end,
    learn = function(d) learnt = d end }
  expect(folk.serve(wp.folk, P, p, heir):find("2 coins", 1, true), "lodging costs")
  pack.add(heir.pack, "coin", 30, 0)
  expect(folk.serve(wp.folk, P, p, heir):find("rest the night", 1, true) and rested == 8)
  p.profession = "priest"
  expect(folk.serve(wp.folk, P, p, heir):find("tends", 1, true) and healed == 30)
  p.profession = "herbalist"
  local offers = {}
  for _, sv in ipairs(npcs.services(p)) do offers[sv.kind] = sv end
  expect(offers.healing and offers.teaching and offers.teaching.discipline == "alchemy")
  p.profession = "hedge mage"
  folk.serve(wp.folk, P, p, heir)
  expect_eq(learnt, "spellcasting")
  p.profession = "herder"
  expect(folk.serve(wp.folk, P, p, heir):find("way to", 1, true))
  p.profession = "child"
  expect(folk.serve(wp.folk, P, p, heir):find("nothing to offer", 1, true))
  p.profession = "merchant"
  local before = pack.count(heir.pack, "coin")
  local said = folk.serve(wp.folk, P, p, heir)
  expect(said:find("Bought", 1, true) or said:find("barter", 1, true), said)
  expect(pack.count(heir.pack, "coin") <= before)
end

-- spec: factions-and-economy: Prices by supply and demand
do
  local coast = { id = "test:coast", population = 300, type = "market_town", culture = anarvan.culture }
  local hills = { id = "test:hills", population = 300, type = "market_town", culture = anarvan.culture }
  db.by_id[coast.id], db.by_id[hills.id] = coast, hills
  local cells = { [coast.id] = { coast = true, fertility = 0.5 },
    [hills.id] = { class = "mountain", fertility = 0.1 } }
  local function cell_of(st) return cells[st.id] or {} end
  local salt_coast = economy.price(db, coast, "salt", true, cell_of)
  local salt_hills = economy.price(db, hills, "salt", true, cell_of)
  expect(salt_coast < salt_hills, "salt cheap on the coast (" .. salt_coast .. "), dear in the hills ("
    .. salt_hills .. ")")
  expect(economy.price(db, hills, "copper_ore", false, cell_of)
    <= economy.price(db, coast, "copper_ore", false, cell_of))
  expect(economy.price(db, coast, "salt", true, cell_of) >= economy.price(db, coast, "salt", false, cell_of))
  expect_eq(economy.price(db, coast, "coin", true), 1)
  db.by_id[coast.id], db.by_id[hills.id] = nil, nil
end

-- spec: factions-and-economy: Coins in towns, barter in villages
do
  local e = economy.new()
  local p = pack.new()
  local ok, why = economy.trade(e, db, { id = "t", type = "city", population = 500, culture = anarvan.culture },
    p, "salt", 1, true)
  expect(not ok and why:find("coins", 1, true))
  pack.add(p, "coin", 100, 0)
  expect(economy.trade(e, db, { id = "t", type = "city", population = 500, culture = anarvan.culture }, p, "salt", 1,
    true) and pack.count(p, "salt") == 1)
  local v = { id = "v", type = "village", population = 60, culture = anarvan.culture }
  local q = pack.new()
  expect(not economy.trade(e, db, v, q, "rug", 1, true), "nothing to barter")
  pack.add(q, "dates", 400, 0)
  expect(economy.trade(e, db, v, q, "salt", 1, true) and pack.count(q, "dates") < 400, "bartered with dates")
  expect(economy.trade(e, db, v, q, "salt", 1, false) and pack.count(q, "flatbread") > 0, "sold for bread")
  expect(not economy.trade(e, db, v, q, "silk", 1, false))
  expect(e.place.v > 0, "trading is remembered kindly")
end

-- spec: factions-and-economy: Factions and standing
do
  local fs = economy.factions(db)
  local kinds = {}
  for _, f in ipairs(fs) do kinds[f.kind] = true end
  expect(kinds.realm and kinds.faith and kinds.guilds and kinds["the Ashen"] and kinds.bandits and kinds.nomads)
  local e = economy.new()
  expect_eq(economy.regard(e, db, "the Ashen"), -100)
  local before = economy.regard(e, db, anarvan.realm)
  economy.deed(e, db, anarvan, "saved from fire")
  expect(economy.regard(e, db, anarvan.realm) > before and e.place[anarvan.id] == 25)
  local prosper = anarvan.prosperity
  economy.deed(e, db, anarvan, "cleared bandits")
  expect(anarvan.prosperity > prosper - 1e-9 and e.standing.bandits < 0)
  economy.deed(e, db, anarvan, "broke guest right")
  expect(e.place[anarvan.id] < 25)
  economy.deed(e, db, anarvan, "cast before them")
  expect_eq(economy.regard(e, db, "nobody"), 0)
  local back = economy.new(economy.snapshot(e))
  expect_eq(back.place[anarvan.id], e.place[anarvan.id])
  expect(next(economy.new({ standing = { [1] = 2 } }).standing) == nil)
end

-- spec: factions-and-economy: Caravans on the trade routes
do
  local list = economy.caravans(db)
  expect(#list > 0)
  local c = list[1]
  local p1 = economy.caravan_at(db, c, 0)
  local p2 = economy.caravan_at(db, c, 3600 * 6)
  local d = world.distance(p1, p2)
  expect(d > 0 and d <= economy.CARAVAN_SPEED * 3600 * 6 * 1.02, "a caravan's pace")
  local road = db.roads[c.road]
  local trip = road.length / economy.CARAVAN_SPEED
  local there = economy.caravan_at(db, c, trip * (1 - c.phase) - 1)
  local b = history.get(db, road.b)
  expect(world.distance(there, b.pos) < 100, "it reaches the end of its road")
  expect_eq(economy.production(db, { population = 0 }, nil).lamp_oil ~= nil, true)
end
