-- SPDX-License-Identifier: MIT
-- Factions and the economy (concept 6.11, 6.12). Factions: the realms, each
-- town's guild, the faiths and their orders, free mages, the Ashen, bandits
-- and the nomads, each with a stance towards mages; the heirs' standing
-- with each faction and in each settlement goes up and down with what they
-- do. Settlements make goods by their land, rock, waters and trades; prices
-- follow supply and demand along the roads (salt cheap on the coast, dear
-- in the mountains); towns take coin, villages barter. Caravans travel the
-- trade routes as the days pass. Prosperity grows with trade and safety and
-- falls with raids and fire; what the heirs do feeds back.
local history = require("people.history")
local items = require("life.items")
local pack = require("player.pack")

local economy = {}

economy.CARAVAN_SPEED = 35000 / 86400 -- metres per second: a caravan's day
economy.COIN_TOWNS = { market_town = true, city = true, oasis_town = true, caravanserai = true, citadel = true }

-- What a settlement makes: { item = amount per day }.
function economy.production(db, st, cell)
  local out = {}
  local function add(item, n) out[item] = (out[item] or 0) + n end
  local pop = math.max(st.population or 0, 1)
  cell = cell or {}
  if cell.coast then add("salt", pop / 20); add("dried_meat", pop / 40) end
  if cell.oasis or st.type == "oasis_town" then add("dates", pop * 2) end
  if cell.class == "mountain" or st.type == "mining_town" then
    add("copper_ore", pop / 10)
    add("iron_ingot", pop / 40)
  end
  if cell.class == "steppe" or st.type == "nomad_camp" then add("wool", pop / 8); add("dried_meat", pop / 20) end
  if (cell.fertility or 0) > 0.3 then add("flatbread", pop / 2); add("pomegranate", pop / 4) end
  local cu = st.culture and history.get(db, st.culture)
  if cu and (cu.family == "persianate" or cu.family == "steppe") then add("rug", pop / 200) end
  if cu and cu.family == "subcontinent" then add("spices", pop / 30); add("dyed_cloth", pop / 40) end
  if cu and cu.family == "far_east" then add("silk", pop / 60) end
  if st.trade == "weaving" then add("rug", 1); add("dyed_cloth", 2) end
  add("lamp_oil", pop / 50)
  return out
end

-- The goods within reach of a settlement (its own and those of the places
-- its roads lead to, less by distance): { item = supply }.
function economy.supply(db, st, cell_of)
  local sup = {}
  local function take(other, factor)
    for item, n in pairs(economy.production(db, other, cell_of and cell_of(other))) do
      sup[item] = (sup[item] or 0) + n * factor
    end
  end
  take(st, 1)
  for _, r in ipairs(db.roads or {}) do
    local other = (r.a == st.id and r.b) or (r.b == st.id and r.a)
    if other then take(history.get(db, other), math.max(0.05, 1 - r.length / 300000)) end
  end
  return sup
end

-- The price of an item in a settlement: dear where it is scarce, cheap where
-- it is plenty. buying: the heirs buy (sellers keep a margin).
function economy.price(db, st, item, buying, cell_of)
  local it = items.get(item)
  local base = it and it.price or 1
  if item == "coin" then return 1 end
  local supply = economy.supply(db, st, cell_of)[item] or 0
  local demand = math.max(1, (st.population or 10) / 20)
  local factor = math.max(0.3, math.min(4, math.sqrt(demand / math.max(supply, 0.05))))
  local p = base * factor
  if buying then p = p * 1.25 end
  return math.max(1, math.floor(p + 0.5))
end

-- The factions of the world and their stance towards mages.
function economy.factions(db)
  if db.factions then return db.factions end
  local out = {}
  local function add(f) out[#out + 1] = f; f.n = #out end
  for _, p in ipairs(db.polity) do
    add({ id = p.id, kind = "realm", name = p.name, attitude = ({ ["mages are welcome"] = 10,
      ["mages need a licence"] = 0, ["mages are hunted"] = -30, ["ruled by mages"] = 20 })[p.mages] or 0 })
  end
  for _, f in ipairs(db.faith) do
    add({ id = f.id, kind = "faith", name = f.name, attitude = ({ ["a sacred gift"] = 15,
      ["a power to be licensed"] = 0, ["an abomination"] = -40 })[f.magic] or 0 })
  end
  add({ id = "guilds", kind = "guilds", name = "the guilds", attitude = 0 })
  add({ id = "free mages", kind = "free mages", name = "the free mages", attitude = -5 })
  add({ id = "the Ashen", kind = "the Ashen", name = "the Ashen", attitude = -100 })
  add({ id = "bandits", kind = "bandits", name = "the bandits", attitude = -60 })
  add({ id = "nomads", kind = "nomads", name = "the nomads and forest folk", attitude = 0 })
  db.factions = out
  return out
end

-- The heirs' standing: with factions and in settlements (-100 .. 100).
function economy.new(saved)
  saved = type(saved) == "table" and saved or {}
  local e = { standing = {}, place = {}, coins_given = 0, caravans = nil, events = {} }
  for k, v in pairs(type(saved.standing) == "table" and saved.standing or {}) do
    if type(k) == "string" and type(v) == "number" then e.standing[k] = v end
  end
  for k, v in pairs(type(saved.place) == "table" and saved.place or {}) do
    if type(k) == "string" and type(v) == "number" then e.place[k] = v end
  end
  return e
end

function economy.snapshot(e)
  return { standing = e.standing, place = e.place }
end

local function clamp(v) return math.max(-100, math.min(100, v)) end

-- What a faction thinks of the heirs: its stance plus their standing.
function economy.regard(e, db, faction_id)
  for _, f in ipairs(economy.factions(db)) do
    if f.id == faction_id then return clamp(f.attitude + (e.standing[faction_id] or 0)) end
  end
  return clamp(e.standing[faction_id] or 0)
end

-- Something the heirs did: { kind = "saved from fire", "stole", "broke guest
-- right", "cleared bandits", "cast before them", "traded", "helped" }, at a
-- settlement. Moves standing in the settlement, with its realm and faith.
economy.DEEDS = { ["saved from fire"] = 25, ["cleared bandits"] = 20, helped = 10, traded = 1, stole = -15,
  ["broke guest right"] = -40, ["cast before them"] = 0 }

function economy.deed(e, db, st, kind)
  local amount = economy.DEEDS[kind] or 0
  if kind == "cast before them" then -- awe, fear or hostility, by the faith and the realm's law
    local f = st.faith and history.get(db, st.faith)
    amount = f and (({ ["a sacred gift"] = 5, ["a power to be licensed"] = -2, ["an abomination"] = -20 })[f.magic]
      or 0) or 0
  end
  e.place[st.id] = clamp((e.place[st.id] or 0) + amount)
  if st.realm then e.standing[st.realm] = clamp((e.standing[st.realm] or 0) + amount / 2) end
  if st.faith then e.standing[st.faith] = clamp((e.standing[st.faith] or 0) + amount / 3) end
  if kind == "cleared bandits" then e.standing.bandits = clamp((e.standing.bandits or 0) - 30) end
  if kind == "saved from fire" or kind == "cleared bandits" then
    st.prosperity = math.min(1, (st.prosperity or 0.5) + 0.1)
  end
  e.events[#e.events + 1] = { kind = kind, at = st.id }
  return e.place[st.id]
end

-- Buys or sells: coins in towns, barter (goods of the same worth) in
-- villages. Returns true, or nil and why.
function economy.trade(e, db, st, p, item, count, buying, cell_of)
  local price = economy.price(db, st, item, buying, cell_of) * count
  if buying then
    if economy.COIN_TOWNS[st.type] then
      if pack.count(p, "coin") < price then return nil, "not enough coins: " .. price .. " needed" end
      pack.take(p, "coin", price)
    else -- barter: give goods worth as much
      local paid = 0
      local ids = {}
      for id in pairs(p.stacks) do ids[#ids + 1] = id end
      table.sort(ids)
      local plan = {}
      for _, id in ipairs(ids) do
        if id ~= item and paid < price then
          local worth = economy.price(db, st, id, false, cell_of)
          local n = math.min(p.stacks[id].count, math.ceil((price - paid) / worth))
          plan[#plan + 1] = { id, n }
          paid = paid + n * worth
        end
      end
      if paid < price then return nil, "nothing worth enough to barter" end
      for _, it in ipairs(plan) do pack.take(p, it[1], it[2]) end
    end
    pack.add(p, item, count, 0)
  else
    if pack.count(p, item) < count then return nil, "you have no " .. items.name(item):lower() end
    pack.take(p, item, count)
    if economy.COIN_TOWNS[st.type] then pack.add(p, "coin", price, 0) else pack.add(p, "flatbread", price, 0) end
  end
  economy.deed(e, db, st, "traded")
  return true
end

-- The caravans on the trade routes: each goes back and forth along its road
-- at a caravan's pace, from the world's time.
function economy.caravans(db)
  if db.caravans then return db.caravans end
  local out = {}
  for i, r in ipairs(db.roads or {}) do
    if r.kind == "trade route" then
      out[#out + 1] = { road = i, phase = (i * 0.37) % 1, goods = { "spices", "silk", "salt", "rug" } }
    end
  end
  db.caravans = out
  return out
end

-- Where a caravan is at a time: its position and heading.
function economy.caravan_at(db, c, time)
  local r = db.roads[c.road]
  local a, b = history.get(db, r.a), history.get(db, r.b)
  local trip = r.length / economy.CARAVAN_SPEED
  local t = ((time / trip) + c.phase) % 2
  if t > 1 then a, b, t = b, a, 2 - t end
  local ax, ay, az = world.to_cartesian(a.pos)
  local bx, by, bz = world.to_cartesian(b.pos)
  local la, lb = math.sqrt(ax * ax + ay * ay + az * az), math.sqrt(bx * bx + by * by + bz * bz)
  ax, ay, az, bx, by, bz = ax / la, ay / la, az / la, bx / lb, by / lb, bz / lb
  local theta = math.acos(math.max(-1, math.min(1, ax * bx + ay * by + az * bz)))
  local wa, wb = 1 - t, t -- along the great circle at an even pace
  if theta > 1e-9 then wa, wb = math.sin((1 - t) * theta) / math.sin(theta), math.sin(t * theta) / math.sin(theta) end
  local R = world.planet().radius
  local pos = world.from_cartesian((ax * wa + bx * wb) * R, (ay * wa + by * wb) * R, (az * wa + bz * wb) * R)
  pos.alt = world.height(pos)
  return pos, b
end

return economy
