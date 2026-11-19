-- SPDX-License-Identifier: MIT
-- People (concept 6.10): generated when their settlement comes near, a
-- household at a time - names in their people's language, age, family,
-- profession, wealth, faith, traits - and remembered once the heirs have
-- dealt with them. They live by the day: sleep, meals, work at their trade,
-- prayers at their faith's hours, the bazaar, the teahouse, festivals of
-- their calendar; they go indoors in the rain and run with buckets to a
-- fire. They see the heirs by their standing, their cloak and the magic
-- they have seen, and keep guest right. They talk by topics (rumours,
-- directions, the history of their land told their way, faith, songs,
-- grudges, prices, work) and offer services (trade, lodging, healing,
-- teaching, guiding). Requests come from the state of the world.
local rng = require("people.rng")
local history = require("people.history")
local language = require("people.language")
local peopling = require("people.peopling")
local lore = require("people.lore")
local economy = require("people.economy")

local npcs = {}

npcs.WALK = 1.3 -- m/s
npcs.TRAITS = { "kind", "proud", "curious", "pious", "greedy", "brave", "timid", "merry", "stern", "wise" }
npcs.DWELLINGS = { home = true, farmhouse = true, weaver = true, tent = true, tower = true, palace = true }
npcs.WORK = { smith = "smithy", merchant = "bazaar", scholar = "library", herbalist = "bazaar", potter = "home",
  weaver = "home", farmer = "fields", herder = "pasture", fisher = "dock", innkeeper = "caravanserai",
  priest = "temple", guard = "citadel" }

local talk_data
function npcs.data()
  if not talk_data then
    local text = assert(vfs.read("data/dialogue.lua"))
    talk_data = assert(load(text, "=data/dialogue.lua", "t", {}))()
  end
  return talk_data
end

-- The people of a laid-out settlement.
function npcs.populate(db, plan)
  local st = plan.settlement
  local out = {}
  local lang = peopling.speech(db, st.culture)
  local has = {}
  for _, b in ipairs(plan.buildings) do has[b.type] = has[b.type] or b end
  for _, b in ipairs(plan.buildings) do
    if npcs.DWELLINGS[b.type] and not b.ruin then
      local s = rng.new(b.seed, "household")
      local hh = b.household or {}
      local n = math.max(1, math.min(8, hh.people or 3))
      for i = 1, n do
        local role = (i <= 2 and "parent") or (i == n and n > 3 and "elder") or "child"
        local age = role == "parent" and s:int(22, 50) or (role == "elder" and s:int(60, 85) or s:int(3, 16))
        local trade = role == "parent" and (hh.trade or "farmer") or (role == "elder" and "elder" or "child")
        if b.type == "tower" and i == 1 then trade = "hedge mage" end
        local id = b.id .. "/" .. i
        local faith = st.faith
        if st.minority and s:chance(0.2) then faith = st.minority end
        local name = language.name(lang, "person", id)
        if b.role == "farrokh" then name, age, role, trade = "Farrokh", 59, "parent", "weaver" end
        out[#out + 1] = { id = id, home = b.id, settlement = st.id, culture = st.culture, faith = faith,
          name = name, sex = s:chance(0.5) and "woman" or "man", age = age, role = role,
          profession = trade, wealth = hh.wealth or 0.5, traits = { s:pick(npcs.TRAITS), s:pick(npcs.TRAITS) },
          robe = s:int(0, 3), family = b.id, festival_day = rng.hash(db.seed, st.culture) % 30 }
      end
    end
  end
  -- Those whose work keeps a place: the innkeeper, the priest, a merchant.
  local staff = { caravanserai = "innkeeper", temple = "priest", shrine = "priest", bazaar = "merchant",
    teahouse = "merchant" }
  for _, b in ipairs(plan.buildings) do
    local job = staff[b.type]
    if job then
      local id = b.id .. "/keeper"
      out[#out + 1] = { id = id, home = b.id, work = b.id, settlement = st.id, culture = st.culture, faith = st.faith,
        name = language.name(lang, "person", id), sex = rng.hash(b.seed, "sex") % 2 == 0 and "woman" or "man",
        age = 30 + rng.hash(b.seed, "age") % 30, role = "parent", profession = job, wealth = 0.6,
        traits = { npcs.TRAITS[1 + rng.hash(b.seed, "t") % #npcs.TRAITS] }, robe = rng.hash(b.seed, "robe") % 4,
        festival_day = rng.hash(db.seed, st.culture) % 30 }
    end
  end
  for _, p in ipairs(out) do -- where each works
    if not p.work then
      local kind = npcs.WORK[p.profession]
      local place = kind and has[kind]
      p.work = (place and place.id) or (kind == "fields" or kind == "pasture") and "fields" or p.home
    end
    p.temple = (has.temple or has.shrine or {}).id
    p.teahouse = (has.teahouse or {}).id
    p.bazaar = (has.bazaar or {}).id
  end
  return out
end

local PRAYERS = { ["at dawn and dusk"] = { 6, 18 }, ["three times a day"] = { 6, 12, 18 },
  ["five times a day"] = { 5, 8, 12, 15, 18 }, ["at every new moon"] = {} }

-- What someone is doing at an hour of a day: { what, at } where at is a
-- building id, "fields", "plaza" or "fire". world: { rain, fire (a burning
-- building id), raid }.
function npcs.activity(db, p, hour, day, world_state)
  world_state = world_state or {}
  local h = math.floor(hour)
  local adult = p.role ~= "child"
  if world_state.raid then return { what = adult and p.traits[1] == "brave" and "defend" or "flee", at = p.home } end
  if world_state.fire and adult and h >= 5 and h < 23 then return { what = "fight fire", at = "fire" } end
  if h >= 22 or h < 5 then return { what = "sleep", at = p.home } end
  if day % 30 == p.festival_day and h >= 10 then return { what = "festival", at = "plaza" } end
  local faith = p.faith and history.get(db, p.faith)
  for _, ph in ipairs(faith and PRAYERS[faith.prayers] or {}) do
    if h == ph then return { what = "pray", at = p.temple or p.home } end
  end
  if h == 7 or h == 12 or h == 19 then return { what = "eat", at = p.home } end
  local a
  if h >= 8 and h < 18 then
    if p.role == "child" then a = { what = "play", at = "plaza" }
    elseif p.role == "elder" then a = { what = "talk", at = p.teahouse or "plaza" }
    elseif h < 10 and p.bazaar and p.profession ~= "merchant" and day % 3 == 0 then a = { what = "shop", at = p.bazaar }
    else a = { what = "work", at = p.work } end
  else
    a = { what = "rest", at = (h >= 20 and p.teahouse and p.role ~= "child") and p.teahouse or p.home }
  end
  if world_state.rain and (a.at == "plaza" or a.at == "fields") then a = { what = "shelter", at = p.home } end
  return a
end

-- How someone feels about the heirs: "warm", "neutral", "wary", "afraid" or
-- "hostile". seen: { standing (the settlement's), cloak ("drab" or "vivid"),
-- magic (they saw a spell cast), guest (the heirs broke guest right) }.
function npcs.attitude(db, p, seen)
  seen = seen or {}
  local score = seen.standing or 0
  local cu = history.get(db, p.culture)
  local f = p.faith and history.get(db, p.faith)
  if seen.cloak == "vivid" then -- they see a mage
    score = score + ((cu.magic == "revered" and 10) or (cu.magic == "feared" and -15) or (cu.magic == "forbidden"
      and -30) or -5)
  end
  if seen.magic then
    score = score + ((f and f.magic == "a sacred gift" and 15) or (f and f.magic == "an abomination" and -40) or -10)
  end
  if seen.guest then score = score - 60 end
  for _, t in ipairs(p.traits) do
    if t == "kind" then score = score + 10 elseif t == "stern" then score = score - 5 end
  end
  if cu.values[1] == "hospitality" then score = score + 10 end
  if score >= 15 then return "warm" end
  if score >= -10 then return "neutral" end
  if score >= -30 then return "wary" end
  if seen.magic or seen.cloak == "vivid" then return score >= -60 and "afraid" or "hostile" end
  return "hostile"
end

-- A greeting by their mood.
function npcs.greet(p, mood)
  local list = npcs.data().greet[mood] or npcs.data().greet.neutral
  return list[1 + rng.hash(0, p.id, mood) % #list]
end

local DIRS = { "north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west" }

-- Directions from a place to another: "two days' ride to the north-east".
function npcs.directions(from, to)
  local a, b = world.to_local(from), world.to_local(to)
  local dx, dz = b[1] - a[1], -(b[3] - a[3])
  local bearing = math.atan(dx, dz) % (2 * math.pi)
  local dir = DIRS[math.floor(bearing / (math.pi / 4) + 0.5) % 8 + 1]
  local d = world.distance(from, to)
  local how
  if d < 1500 then how = "a short walk"
  elseif d < 30000 then how = string.format("%d hours' walk", math.max(1, math.floor(d / 5000 + 0.5)))
  else how = string.format("%d days' ride", math.max(1, math.floor(d / 40000 + 0.5))) end
  return how .. " to the " .. dir, dir, d
end

-- What someone says on a topic. ctx: { pos (where they stand), to (a
-- settlement asked about), economy (for prices), requests }.
function npcs.talk(db, p, topic, ctx)
  ctx = ctx or {}
  local st = history.get(db, p.settlement)
  local know = lore.knowledge(db, { culture = p.culture, faith = p.faith, realm = st and st.realm })
  local d = npcs.data()
  local intro = d.topic_intro[topic] or ""
  if topic == "directions" then
    local to = ctx.to
    if not to then return "Where to? I know only these hills." end
    local how = npcs.directions(ctx.pos or st.pos, to.pos)
    return string.format(intro, to.name) .. "it is " .. how .. "."
  elseif topic == "prices" then
    local items = { "salt", "spices", "flatbread", "rug" }
    local parts = {}
    for _, it in ipairs(items) do parts[#parts + 1] = it .. " " .. economy.price(db, st, it, true) end
    return intro .. " " .. table.concat(parts, ", ") .. " coins."
  elseif topic == "work" then
    local req = (ctx.requests or {})[1]
    return req and (intro .. " " .. req.text) or "There is nothing that needs doing, thank the gods."
  end
  local lines = know[topic] or {}
  if #lines == 0 then return "I know nothing of that." end
  return intro .. " " .. lines[1 + rng.hash(0, p.id, topic) % #lines]
end

-- The requests a settlement has, from the state of the world: w = {
-- predators, fire_dir, dry, bandits_near (a settlement name), sick_well }.
function npcs.requests(db, st, w)
  local d = npcs.data().requests
  local out = {}
  if w.predators then out[#out + 1] = { kind = "predator", text = d.predator } end
  if w.fire_dir then out[#out + 1] = { kind = "wildfire", text = string.format(d.wildfire, w.fire_dir) } end
  if w.dry then out[#out + 1] = { kind = "drought", text = d.drought, ritual = "call_rain" } end
  if w.bandits_near then out[#out + 1] = { kind = "bandits", text = string.format(d.bandits, w.bandits_near) } end
  if w.sick_well then out[#out + 1] = { kind = "poisoned_well", text = d.poisoned_well, clue = "bitter almonds" } end
  if rng.hash(db.seed, st.id, w.week or 0) % 7 == 0 then
    out[#out + 1] = { kind = "lost_child", text = string.format(d.lost_child,
      language.name(peopling.speech(db, st.culture), "person", st.id .. "child")) }
  end
  return out
end

-- The services someone offers by their profession: { kind, price }.
function npcs.services(p)
  local list = {}
  if p.profession == "merchant" then list[#list + 1] = { kind = "trade" } end
  if p.profession == "innkeeper" then list[#list + 1] = { kind = "lodging", price = 2 } end
  if p.profession == "priest" or p.profession == "herbalist" then list[#list + 1] = { kind = "healing", price = 3 } end
  if p.profession == "herbalist" then list[#list + 1] = { kind = "teaching", discipline = "alchemy", price = 5 } end
  if p.profession == "hedge mage" then
    list[#list + 1] = { kind = "teaching", discipline = "spellcasting", price = 8 }
  end
  if p.profession == "herder" or p.profession == "merchant" then list[#list + 1] = { kind = "guide", price = 4 } end
  return list
end

-- The heirs' memory of people they met (kept with the save).
function npcs.new(saved)
  local m = { met = {} }
  for id, v in pairs(type(saved) == "table" and type(saved.met) == "table" and saved.met or {}) do
    if type(id) == "string" and type(v) == "table" then m.met[id] = v end
  end
  return m
end

function npcs.snapshot(m)
  return { met = m.met }
end

-- Meeting someone makes them persistent: remembered with what passed.
function npcs.meet(m, p, what)
  local r = m.met[p.id] or { name = p.name, times = 0 }
  r.times = r.times + 1
  r.last = what
  m.met[p.id] = r
  return r
end

return npcs
