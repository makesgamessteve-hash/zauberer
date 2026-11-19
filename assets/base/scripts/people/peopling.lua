-- SPDX-License-Identifier: MIT
-- Peopling, the "Settled world" variant (concept 10.5): peoples are seeded
-- in cradles (well-watered, fertile, mild regions) and spread over the land
-- along rivers and coasts, avoiding deserts and mountains, adapting how they
-- live to each land. Neighbouring peoples of a style family speak related
-- languages. Faiths spread from holy places along the easy roads; realms are
-- carved out around capitals with borders along rivers and ridges;
-- settlements are placed where water, soil, defence, trade and resources
-- meet, sized by what the land can feed; roads join them, with caravanserais
-- a day apart across dry land. Everything follows from the seed.
local rng = require("people.rng")
local land = require("people.land")
local language = require("people.language")
local history = require("people.history")
local job = require("people.job")

local peopling = {}

peopling.PEOPLES = 6           -- founding peoples (cradles)
peopling.SPREAD_LIMIT = 26     -- how far peoples spread (cost of the way)
peopling.DAY = 35000           -- metres: a caravan's day
peopling.SITE_SPACING = 12000  -- metres between settlements of a region

local function read(path)
  local text = assert(vfs.read(path))
  return assert(load(text, "=" .. path, "t", {}))()
end

local data
function peopling.data()
  if not data then
    data = { families = read("data/style_families.lua"), peoples = read("data/peoples.lua"),
      types = read("data/settlement_types.lua") }
    data.family = {}
    for _, f in ipairs(data.families) do data.family[f.id] = f end
    data.type = {}
    for _, t in ipairs(data.types) do data.type[t.id] = t end
  end
  return data
end

local function arc(grid, a, b) -- metres along the surface between two regions or directions
  local da, db = a.dir or a, b.dir or b
  local dot = da[1] * db[1] + da[2] * db[2] + da[3] * db[3]
  local la = math.sqrt(da[1] ^ 2 + da[2] ^ 2 + da[3] ^ 2)
  local lb = math.sqrt(db[1] ^ 2 + db[2] ^ 2 + db[3] ^ 2)
  return math.acos(math.max(-1, math.min(1, dot / (la * lb)))) * grid.radius
end
peopling.arc = arc

local function suitability(c)
  if not c.land or c.class == "ice" then return nil end
  return c.fertility + (c.river and 0.4 or 0) + (c.coast and 0.15 or 0) + (c.oasis and 0.3 or 0) - c.relief / 3000
end

-- Picks up to count regions by a score, at least spacing metres apart
-- (relaxing the spacing until enough are found).
local function spaced(grid, candidates, count, spacing)
  table.sort(candidates, function(a, b)
    if a.score ~= b.score then return a.score > b.score end
    return a.cell.id < b.cell.id
  end)
  local chosen = {}
  for _ = 1, 6 do
    chosen = {}
    for _, cand in ipairs(candidates) do
      local ok = true
      for _, o in ipairs(chosen) do
        if arc(grid, cand.cell, o) < spacing then ok = false break end
      end
      if ok then chosen[#chosen + 1] = cand.cell end
      if #chosen >= count then return chosen end
    end
    spacing = spacing * 0.7
  end
  return chosen
end

local RELIEF = { mountains = { 300, 3000 }, foothills = { 80, 700 }, plains = { 0, 200 }, lowland = { 0, 150 },
  any = { 0, 5000 } }

-- How well a style family fits a region (higher is better).
function peopling.family_fit(f, c)
  local h = f.homeland
  local function off(v, r) return v < r[1] and (r[1] - v) or (v > r[2] and (v - r[2]) or 0) end
  local rel = RELIEF[h.relief] or RELIEF.any
  return -(off(c.temp, h.temp) / 4 + off(c.rain, h.rain) / 250 + off(c.relief, rel) / 300)
end

local function choose_family(d, c, s, used)
  local choices = {}
  for _, f in ipairs(d.families) do
    if f.id ~= "far_east" or used.far_east == nil then
      choices[#choices + 1] = { f, math.exp(peopling.family_fit(f, c)) * (used[f.id] and 0.35 or 1) }
    end
  end
  return s:weighted(choices)
end

local function subsistence(d, c)
  if c.oasis then return "oasis farming" end
  if c.coast and c.fertility < 0.4 then return d.peoples.subsistence.coast end
  return d.peoples.subsistence[c.class] or "farming"
end

-- Cultures and their languages: cradles, the spread over the land, language
-- families from neighbouring peoples of a family.
local function cultures(db, grid, s, opts)
  local d = peopling.data()
  local cands = {}
  for _, c in ipairs(grid.cells) do
    local v = suitability(c)
    if v then cands[#cands + 1] = { cell = c, score = v + s:float() * 0.05 } end
  end
  local count = opts.peoples or peopling.PEOPLES
  if opts.anchor then -- the first people's cradle is where the story begins
    local a = { world.to_cartesian(opts.anchor) }
    for _, cand in ipairs(cands) do
      cand.score = cand.score + math.max(0, 3 - arc(grid, cand.cell, a) / grid.cell_m) * 2
    end
  end
  local cradles = spaced(grid, cands, count, grid.radius * 1.4 / math.sqrt(count))
  local used = {}
  for i, c in ipairs(cradles) do
    local f = choose_family(d, c, s, used)
    used[f.id] = true
    local families = { { f.id, 1 } }
    local second = choose_family(d, c, s, used)
    if second ~= f and s:chance(0.4) then families = { { f.id, 0.7 }, { second.id, 0.3 } } end
    local values = s:shuffle({ table.unpack(d.peoples.values) })
    local ley = c.ley or 1e9
    local cal = s:pick(d.peoples.calendars)
    history.add(db, "culture", { cradle = c.id, family = f.id, families = families,
      subsistence = subsistence(d, c), values = { values[1], values[2] },
      magic = s:weighted({ { "revered", ley < 150000 and 3 or 1 }, { "regulated", 2 }, { "feared", 1.5 },
        { "forbidden", 0.7 } }),
      calendar = cal, festivals = { d.peoples.festivals[cal][s:int(1, 3)] },
      cuisine = f.cuisine, textiles = f.textiles, instruments = f.instruments, clothing = f.clothing,
      palette = f.palette, motifs = f.motifs, hospitality = values[1] == "hospitality" and "sacred guest right"
        or s:pick({ "three days of guest right", "bread and salt for strangers", "guests eat first" }),
      index = i })
  end
  local cost = function(a, b)
    if not b.land or b.class == "ice" then return nil end
    local step = 1 + b.relief / 400 + ((b.class == "desert" and not b.oasis) and 2 or 0)
      + (b.class == "mountain" and 2.5 or 0) + (b.class == "tundra" and 1.5 or 0)
    if (a.river and b.river) or (a.coast and b.coast) then step = step * 0.5 end
    return step
  end
  local sources = {}
  for i, c in ipairs(cradles) do sources[i] = { cell = c.id, key = i } end
  local dist, owner = land.spread(grid, sources, cost, opts.spread or peopling.SPREAD_LIMIT)
  db.cell_culture, db.cell_settled = {}, {}
  for id, k in pairs(owner) do
    db.cell_culture[id] = db.culture[k].id
    db.cell_settled[id] = dist[id]
  end
  -- Language families: neighbouring peoples of one style family speak
  -- daughters of one proto-language; the others their own.
  local group = {}
  for i = 1, #db.culture do group[i] = i end
  local function find(i)
    while group[i] ~= i do i = group[i] end
    return i
  end
  for id, k in pairs(owner) do
    for _, nid in ipairs(grid.cells[id].nb) do
      local o = owner[nid]
      if o and o ~= k and db.culture[o].family == db.culture[k].family then
        local a, b = find(k), find(o)
        if a ~= b then group[math.max(a, b)] = math.min(a, b) end
      end
    end
  end
  local protos = {}
  for i, cu in ipairs(db.culture) do
    local root = find(i)
    if not protos[root] then
      local rec = history.add(db, "language", { seed = rng.hash(db.seed, "proto", root), family = cu.family,
        proto = true, changes = {} })
      rec.name = "Old " .. language.name(peopling.language(db, rec.id), "people", "proto")
      protos[root] = rec
    end
    local rec = language.daughter(db.seed, "culture" .. i, 1 + (i % 3))
    rec.family, rec.parent = cu.family, protos[root].id
    local lang = history.add(db, "language", rec)
    cu.language = lang.id
    local obj = peopling.language(db, lang.id)
    cu.name = language.name(obj, "people", "self")
    lang.name = cu.name
  end
end

-- The language object of a language id (made once from its record).
function peopling.language(db, id)
  db.lang_cache = db.lang_cache or {}
  local l = db.lang_cache[id]
  if l then return l end
  local rec = history.get(db, id)
  local parent = rec.parent and peopling.language(db, rec.parent) or nil
  l = language.make(rec, peopling.data().family[rec.family], parent)
  db.lang_cache[id] = l
  return l
end

-- The language a culture speaks.
function peopling.speech(db, culture_id)
  return peopling.language(db, history.get(db, culture_id).language)
end

local function env_of(c)
  if c.coast and c.class ~= "desert" then return "sea" end
  if c.river or c.lake then return "river" end
  if c.class == "desert" then return "desert" end
  if c.class == "mountain" or c.relief > 600 then return "mountain" end
  if c.class == "steppe" or c.class == "tundra" then return "steppe" end
  if c.class == "forest" or c.class == "rainforest" or c.class == "swamp" then return "forest" end
  if c.class == "ice" then return "ice" end
  return "grassland"
end
peopling.env_of = env_of

local function faiths(db, grid, s)
  local d = peopling.data()
  local F = math.max(2, math.min(6, #db.culture // 2 + 1 + s:int(0, 1)))
  local cands = {}
  for id, cu in pairs(db.cell_culture) do
    local c = grid.cells[id]
    local score = ((c.ley or 1e9) < 60000 and 1 or 0) + (c.class == "mountain" and 0.6 or 0) + (c.river and 0.3 or 0)
      + (c.oasis and 0.5 or 0) + rng.float(db.seed, "holy", id) * 0.4
    cands[#cands + 1] = { cell = c, score = score, culture = cu }
  end
  local cores = spaced(grid, cands, F, grid.radius * 1.1 / math.sqrt(F))
  for i, c in ipairs(cores) do
    local culture = history.get(db, db.cell_culture[c.id])
    local lang = peopling.speech(db, culture.id)
    local env = env_of(c)
    local image = d.peoples.faith.imagery[env]
    local concept = s:pick(d.peoples.faith.concepts)
    local near_nexus = (c.ley or 1e9) < 120000
    if near_nexus and s:chance(0.4) then concept = "a colour of mana" end
    local stance = s:weighted({ { "a sacred gift", near_nexus and 3 or 1 }, { "a power to be licensed", 2 },
      { "an abomination", culture.magic == "forbidden" and 3 or 0.8 } })
    local deity = language.name(lang, "holy", "deity" .. i)
    local faith = history.add(db, "faith", { name = "the faith of " .. deity, deity = deity, concept = concept,
      imagery = image[1], element = image[2], magic = stance, core = c.id, culture = culture.id,
      burial = d.peoples.faith.burial[env], order = s:pick(d.peoples.faith.orders),
      prayers = s:pick({ "at dawn and dusk", "three times a day", "five times a day", "at every new moon" }),
      diet = s:pick({ "no pork", "fasting in the month of mourning", "no meat on holy days", "bread before wine",
        "no blood" }),
      sacred = { animal = s:pick({ "falcon", "goat", "horse", "lion", "crane", "snake" }),
        plant = s:pick({ "pomegranate", "date palm", "cypress", "lotus", "wheat", "rose" }) },
      festival = culture.festivals[1] })
    if concept == "a colour of mana" then
      faith.colour = mana.ley(c.pos).nexus_colors[1]
    end
    local site = history.add(db, "site", { kind = "holy", name = language.name(lang, "holy", "site" .. i),
      cell = c.id, pos = land.point(grid, c, 0.5, 0.5), faith = faith.id })
    faith.holy_sites = { site.id }
    history.refer(db, site.id)
  end
  -- Spread along the easy ways, following the peoples.
  db.cell_faith, db.cell_minority = {}, {}
  local best, second = {}, {}
  for i, c in ipairs(cores) do
    local cost = function(a, b)
      if not db.cell_culture[b.id] then return nil end
      local step = 1 + b.relief / 600
      if (a.river and b.river) or (a.coast and b.coast) or b.class == "steppe" then step = step * 0.6 end
      if db.cell_culture[a.id] ~= db.cell_culture[b.id] then step = step + 1.5 end
      return step
    end
    local dist = land.spread(grid, { { cell = c.id, key = i } }, cost)
    for id, dd in pairs(dist) do
      if not best[id] or dd < best[id][1] then
        second[id] = best[id]
        best[id] = { dd, i }
      elseif not second[id] or dd < second[id][1] then
        second[id] = { dd, i }
      end
    end
  end
  for id in pairs(db.cell_culture) do -- lands no road reaches: the faith of the nearest holy place
    if not best[id] then
      local near, nd
      for i, c in ipairs(cores) do
        local dd = arc(grid, grid.cells[id], c)
        if not nd or dd < nd then near, nd = i, dd end
      end
      best[id] = { math.huge, near }
    end
  end
  for id, b in pairs(best) do
    db.cell_faith[id] = db.faith[b[2]].id
    local m = second[id]
    if m and m[1] < b[1] * 1.4 + 2 then db.cell_minority[id] = db.faith[m[2]].id end
  end
end

local function realms(db, grid, s)
  local d = peopling.data()
  local cells_of = {}
  for id, cu in pairs(db.cell_culture) do
    cells_of[cu] = cells_of[cu] or {}
    table.insert(cells_of[cu], grid.cells[id])
  end
  local capitals = {}
  for _, cu in ipairs(db.culture) do
    local cells = cells_of[cu.id] or {}
    table.sort(cells, function(a, b) return a.id < b.id end)
    local cands = {}
    for _, c in ipairs(cells) do cands[#cands + 1] = { cell = c, score = suitability(c) + s:float() * 0.1 } end
    local n = math.max(1, math.floor(#cells / 35 + 0.5))
    for _, c in ipairs(spaced(grid, cands, n, grid.cell_m * 4)) do
      capitals[#capitals + 1] = { cell = c, culture = cu }
    end
  end
  local sources = {}
  for i, cap in ipairs(capitals) do sources[i] = { cell = cap.cell.id, key = i } end
  local cost = function(a, b)
    if not db.cell_culture[b.id] then return nil end
    local step = 1 + b.relief / 500
    if b.river and not a.river then step = step + 1.5 end -- borders follow rivers
    if db.cell_culture[a.id] ~= db.cell_culture[b.id] then step = step + 3 end
    return step
  end
  local _, owner = land.spread(grid, sources, cost)
  local count, coastal = {}, {}
  for id, k in pairs(owner) do
    count[k] = (count[k] or 0) + 1
    if grid.cells[id].coast then coastal[k] = true end
  end
  db.cell_realm = {}
  for i, cap in ipairs(capitals) do
    local c, cu = cap.cell, cap.culture
    local lang = peopling.speech(db, cu.id)
    local faith = history.get(db, db.cell_faith[c.id] or db.faith[1].id)
    local cells = count[i] or 1
    local ptype
    if cu.subsistence == "herding" and (c.class == "steppe" or c.class == "tundra" or c.class == "savanna") then
      ptype = "nomad confederation"
    elseif (c.ley or 1e9) < 60000 and cu.magic == "revered" and s:chance(0.6) then
      ptype = "mage-ruled domain"
    elseif faith.core == c.id and s:chance(0.7) then
      ptype = "theocracy"
    elseif coastal[i] and (cu.values[1] == "commerce" or cu.values[2] == "commerce") and s:chance(0.6) then
      ptype = "merchant republic"
    elseif cells <= 6 then
      ptype = "city-state"
    elseif cells >= 45 then
      ptype = "empire"
    else
      ptype = "kingdom"
    end
    local stance
    local leaning = ({ ["a sacred gift"] = 1, ["a power to be licensed"] = 0, ["an abomination"] = -2 })[faith.magic]
      + ({ revered = 1, regulated = 0, feared = -1, forbidden = -2 })[cu.magic]
    if ptype == "mage-ruled domain" then stance = "ruled by mages"
    elseif leaning >= 1 then stance = "mages are welcome"
    elseif leaning >= 0 then stance = "mages need a licence"
    else stance = "mages are hunted" end
    history.add(db, "polity", { name = language.name(lang, "realm", "realm" .. i), type = ptype, culture = cu.id,
      faith = faith.id, capital_cell = c.id, cells = cells, title = d.peoples.titles[ptype], mages = stance,
      laws = { stance, s:pick({ "a tithe of the harvest", "a toll at every gate", "service in the levy",
        "a tax on looms", "guest right is law" }) } })
  end
  for id, k in pairs(owner) do db.cell_realm[id] = db.polity[k].id end
  for _, p in ipairs(db.polity) do -- neighbours (for wars and trade)
    p.neighbours = {}
  end
  local seen = {}
  for id, rid in pairs(db.cell_realm) do
    for _, nid in ipairs(grid.cells[id].nb) do
      local o = db.cell_realm[nid]
      if o and o ~= rid then
        local key = rid < o and (rid .. "|" .. o) or (o .. "|" .. rid)
        if not seen[key] then
          seen[key] = { a = rid < o and rid or o, b = rid < o and o or rid, cell = rid < o and id or nid,
            other = rid < o and nid or id }
        end
      end
    end
  end
  local keys = {}
  for k in pairs(seen) do keys[#keys + 1] = k end
  table.sort(keys)
  db.borders = {}
  for _, k in ipairs(keys) do
    local b = seen[k]
    table.insert(history.get(db, b.a).neighbours, b.b)
    table.insert(history.get(db, b.b).neighbours, b.a)
    db.borders[#db.borders + 1] = b
  end
end

local function settlement_type(db, c, nth, s, realm_capital, border)
  local cu = history.get(db, db.cell_culture[c.id])
  if nth == 1 and realm_capital then return "city" end
  if nth == 1 and c.oasis then return "oasis_town" end
  local faith = db.cell_faith[c.id] and history.get(db, db.cell_faith[c.id])
  if nth == 1 and faith and faith.core == c.id then return "temple" end
  if nth == 2 and (c.ley or 1e9) < 25000 then return "mage_tower" end
  if border and c.relief > 150 and nth == 2 and s:chance(0.5) then return "citadel" end
  if cu.subsistence == "herding" and (c.class == "steppe" or c.class == "tundra") then return "nomad_camp" end
  if c.class == "swamp" then return "stilt_village" end
  if (c.class == "desert" or c.class == "steppe") and c.relief > 300 and s:chance(0.4) then return "cliff_dwellings" end
  if c.class == "mountain" or (#c.minerals > 0 and s:chance(0.5)) then return "mining_town" end
  if c.coast and s:chance(0.6) then return "fishing_village" end
  if c.river and c.fertility > 0.45 and nth == 1 and s:chance(0.5) then return "market_town" end
  if c.fertility > 0.3 then return "village" end
  return "hamlet"
end

local function settlements(db, grid, s)
  local d = peopling.data()
  local capital_cell = {}
  for _, p in ipairs(db.polity) do capital_cell[p.capital_cell] = p end
  local border = {}
  for _, b in ipairs(db.borders) do border[b.cell], border[b.other] = true, true end
  local ids = {}
  for id in pairs(db.cell_culture) do ids[#ids + 1] = id end
  table.sort(ids)
  db.cell_settlements = {}
  for _, id in ipairs(ids) do
    job.pace(2)
    local c = grid.cells[id]
    local capacity = c.fertility * 6 + (c.river and 2 or 0) + (c.coast and 1 or 0) + (c.oasis and 2 or 0)
    local n = math.max(capital_cell[id] and 1 or 0, math.min(4, math.floor(capacity / 2.5 + s:float())))
    if n > 0 then
      local sites = {}
      local cs = rng.new(db.seed, "sites", id)
      for k = 1, 8 do
        local u, v = 0.1 + cs:float() * 0.8, 0.1 + cs:float() * 0.8
        local p = land.point(grid, c, u, v)
        local h = world.height(p)
        local kind = world.water(p)
        if h > 2 and kind ~= "ocean" and kind ~= "lake" then
          local score = (h - c.height) / 400 + (c.river and 0.3 or 0) + cs:float() * 0.3
          sites[#sites + 1] = { pos = p, h = h, score = score, k = k }
        end
      end
      table.sort(sites, function(a, b)
        if a.score ~= b.score then return a.score > b.score end
        return a.k < b.k
      end)
      local placed = {}
      for _, site in ipairs(sites) do
        if #placed >= n then break end
        local ok = true
        for _, o in ipairs(placed) do
          if world.distance(o.pos, site.pos) < peopling.SITE_SPACING then ok = false end
        end
        if ok then
          local nth = #placed + 1
          local tid = settlement_type(db, c, nth, s, capital_cell[id], border[id])
          local t = d.type[tid]
          local fill = math.min(1, c.fertility + (capital_cell[id] and nth == 1 and 0.5 or 0))
          local count = math.floor(t.buildings[1] + (t.buildings[2] - t.buildings[1]) * fill * (0.6 + cs:float() * 0.4))
          local cu = db.cell_culture[id]
          site.pos.alt = site.h
          local rec = history.add(db, "settlement", { type = tid, pos = site.pos, cell = id, culture = cu,
            faith = db.cell_faith[id], minority = db.cell_minority[id], realm = db.cell_realm[id],
            buildings = count, population = count * t.people, prosperity = 0.3 + c.fertility * 0.5 + cs:float() * 0.2,
            walled = t.walled or false })
          rec.name = language.name(peopling.speech(db, cu), "settlement", rec.id)
          placed[#placed + 1] = rec
          db.cell_settlements[id] = db.cell_settlements[id] or {}
          table.insert(db.cell_settlements[id], rec.id)
          if tid == "city" then capital_cell[id].capital = rec.id end
        end
      end
    end
  end
  for _, p in ipairs(db.polity) do -- a realm whose capital region had no site: its biggest place
    if not p.capital then
      local best
      for _, st in ipairs(db.settlement) do
        if st.realm == p.id and (not best or st.population > best.population) then best = st end
      end
      if best then p.capital = best.id end
    end
  end
end

local function lerp_pos(grid, a, b, t)
  local ax, ay, az = world.to_cartesian(a)
  local bx, by, bz = world.to_cartesian(b)
  local x, y, z = ax + (bx - ax) * t, ay + (by - ay) * t, az + (bz - az) * t
  local l = math.sqrt(x * x + y * y + z * z)
  return world.from_cartesian(x / l * grid.radius, y / l * grid.radius, z / l * grid.radius)
end

local function roads(db, grid, s)
  local d = peopling.data()
  db.roads = {}
  local by_realm = {}
  for _, st in ipairs(db.settlement) do
    if st.realm then
      by_realm[st.realm] = by_realm[st.realm] or {}
      table.insert(by_realm[st.realm], st)
    end
  end
  local function link(a, b, kind)
    db.roads[#db.roads + 1] = { a = a.id, b = b.id, length = world.distance(a.pos, b.pos), kind = kind }
  end
  for _, p in ipairs(db.polity) do
    local list = by_realm[p.id] or {}
    table.sort(list, function(a, b)
      if a.population ~= b.population then return a.population > b.population end
      return a.id < b.id
    end)
    for i = 2, #list do
      local best, bd
      for j = 1, i - 1 do
        local dd = world.distance(list[i].pos, list[j].pos)
        if not bd or dd < bd then best, bd = list[j], dd end
      end
      link(list[i], best, "road")
    end
  end
  for _, b in ipairs(db.borders) do -- trade routes between neighbouring capitals
    local pa, pb = history.get(db, b.a), history.get(db, b.b)
    if pa.capital and pb.capital then
      link(history.get(db, pa.capital), history.get(db, pb.capital), "trade route")
    end
  end
  -- Caravanserais a day apart where trade routes cross dry land, away from
  -- towns that already give shelter.
  local DRY = { desert = true, steppe = true, savanna = true }
  local roads_now = #db.roads
  for r = 1, roads_now do
    job.pace(1)
    local road = db.roads[r]
    if road.kind == "trade route" and road.length > 1.6 * peopling.DAY then
      local a, b = history.get(db, road.a), history.get(db, road.b)
      local stops = math.floor(road.length / peopling.DAY)
      for k = 1, stops do
        local p = lerp_pos(grid, a.pos, b.pos, k / (stops + 1))
        local h = world.height(p)
        local cell = land.cell_at(grid, p)
        local lonely = true
        for _, sid in ipairs(cell and db.cell_settlements[cell.id] or {}) do
          if world.distance(history.get(db, sid).pos, p) < 20000 then lonely = false end
        end
        if h > 2 and DRY[cell.class] and lonely then
          do
            p.alt = h
            local cu = db.cell_culture[cell.id] or a.culture
            local t = d.type.caravanserai
            local rec = history.add(db, "settlement", { type = "caravanserai", pos = p, cell = cell.id, culture = cu,
              faith = db.cell_faith[cell.id] or a.faith, realm = db.cell_realm[cell.id] or a.realm, buildings = 1,
              population = t.people, prosperity = 0.5 + s:float() * 0.3, walled = true, road = r })
            rec.name = "Caravanserai of " .. language.name(peopling.speech(db, cu), "settlement", rec.id)
            db.cell_settlements[cell.id] = db.cell_settlements[cell.id] or {}
            table.insert(db.cell_settlements[cell.id], rec.id)
          end
        end
      end
    end
  end
end

-- Peoples the world. opts: { peoples (cradles), year (the present), spread
-- (how far peoples go) }. Returns the history database (present state; the
-- chronicle is written by people.chronicle) and the land grid.
function peopling.settle(seed, opts)
  opts = opts or {}
  local grid = land.sample(seed)
  local s = rng.new(seed, "peopling")
  local db = history.new(opts.year or (1100 + seed % 400), "settled")
  db.seed = seed
  cultures(db, grid, s, opts)
  faiths(db, grid, s)
  realms(db, grid, s)
  settlements(db, grid, s)
  roads(db, grid, s)
  return db, grid
end

-- The settlements within radius metres of a position, nearest first.
function peopling.near(db, grid, pos, radius)
  local out = {}
  local reach = math.ceil(radius / grid.cell_m) + 1
  local c = land.cell_at(grid, pos)
  local seen, frontier = { [c.id] = true }, { c.id }
  for _ = 1, reach do
    local nextf = {}
    for _, id in ipairs(frontier) do
      for _, nid in ipairs(grid.cells[id].nb) do
        if not seen[nid] then
          seen[nid] = true
          nextf[#nextf + 1] = nid
        end
      end
    end
    frontier = nextf
  end
  for id in pairs(seen) do
    for _, sid in ipairs(db.cell_settlements[id] or {}) do
      local st = history.get(db, sid)
      local dd = world.distance(pos, st.pos)
      if dd <= radius then out[#out + 1] = { settlement = st, distance = dd } end
    end
  end
  table.sort(out, function(a, b)
    if a.distance ~= b.distance then return a.distance < b.distance end
    return a.settlement.id < b.settlement.id
  end)
  return out
end

return peopling
