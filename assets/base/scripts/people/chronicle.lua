-- SPDX-License-Identifier: MIT
-- The reconstructed chronicle (concept 10.5 step 6, 10.10, 10.12): written
-- backwards from the present so that it agrees with the map. Each people
-- settled its cradle first and spread outward; settlements were founded as
-- their people reached them; realms had dynasties whose rulers reigned,
-- fought and were buried; wars were fought along today's borders and won by
-- the side that holds the field today; mountains and rivers brought
-- eruptions and floods; some places fell to ruin, each for a cause. Into it
-- the story is woven: Anarvan and its weaver Farrokh, the Circle of the Loom
-- broken some forty years ago, and the Ash Weaver Azhdar's origin in a real
-- city. Deterministic from the seed.
local rng = require("people.rng")
local land = require("people.land")
local language = require("people.language")
local history = require("people.history")
local peopling = require("people.peopling")
local job = require("people.job")

local chronicle = {}

chronicle.CIRCLE_YEARS_AGO = 40
chronicle.RUINS = 0.08 -- share of places that lie in ruins today

local function person(db, culture_id, key, s, born, role, extra)
  local lang = peopling.speech(db, culture_id)
  local rec = { name = language.name(lang, "person", key), culture = culture_id, born = born, role = role }
  for k, v in pairs(extra or {}) do rec[k] = v end
  if not rec.alive and not rec.died then rec.died = born + s:int(35, 75) end
  return history.add(db, "figure", rec)
end

local function battle_name(db, culture_id, s)
  local patterns = {
    { "Field of the %s Banners", { "seven", "banner" }, { "Seven", "Three", "Hundred" } },
    { "the Weeping %s", { "weep", "bridge" }, { "Bridge", "Ford", "Gate" } },
    { "the %s Plain", { "red", "field" }, { "Red", "Ashen", "Bitter" } },
    { "the Pass of %s", { "dark", "wind" }, { "Wolves", "Ravens", "Sorrow" } },
  }
  local p = s:pick(patterns)
  local name = string.format(p[1], s:pick(p[3]))
  local native = language.phrase(peopling.speech(db, culture_id), p[2])
  return name, native
end

-- Writes the chronicle into a peopled database. opts: { anchor (PlanetPos:
-- where Anarvan lies), story (false: no story threads) }.
function chronicle.write(db, grid, opts)
  opts = opts or {}
  local s = rng.new(db.seed, "chronicle")
  local d = peopling.data()
  local now = db.year
  local depth = 1000 + s:int(0, 300) -- years since the first cities
  -- Eras.
  db.eras = {
    { name = "the Age of the Elders", from = now - depth - 3000, to = now - depth },
    { name = "the Age of the First Cities", from = now - depth, to = now - depth // 2 },
    { name = "the Age of Kingdoms", from = now - depth // 2, to = now - 150 },
    { name = "the Present Age", from = now - 150, to = now },
  }
  -- Peoples settle their cradles and spread; places are founded as they come.
  for _, cu in ipairs(db.culture) do
    cu.settled = now - depth - s:int(0, 200)
    history.event(db, { year = cu.settled, kind = "settling", participants = { cu.id }, place = cu.cradle,
      importance = 0.9, title = "the " .. cu.name .. " settle their homeland" })
  end
  local far = 0
  for _, cost in pairs(db.cell_settled) do far = math.max(far, cost) end
  for _, st in ipairs(db.settlement) do
    job.pace(0.5)
    local cu = history.get(db, st.culture)
    local cost = db.cell_settled[st.cell] or far
    st.founded = math.min(now - 5, cu.settled + math.floor(cost / math.max(far, 1) * depth * 0.8) + s:int(0, 60))
    history.event(db, { year = st.founded, kind = "founding", participants = { st.culture, st.id },
      place = st.id, importance = st.type == "city" and 0.8 or 0.2, title = st.name .. " is founded" })
    if cost > far * 0.6 and s:chance(0.15) then
      history.event(db, { year = st.founded - s:int(1, 20), kind = "migration", participants = { st.culture },
        place = st.cell, importance = 0.4, title = "the " .. cu.name .. " move into new lands" })
    end
  end
  -- Elder ruins at the strongest nexuses: the first mage culture, in the Old Tongue.
  local elder = {}
  for _, c in ipairs(grid.cells) do
    if c.land and c.ley and c.ley < grid.cell_m * 0.7 then elder[#elder + 1] = c end
  end
  table.sort(elder, function(a, b) return a.ley < b.ley or (a.ley == b.ley and a.id < b.id) end)
  db.old_tongue = history.add(db, "language", { seed = rng.hash(db.seed, "old tongue"), family = "persianate",
    proto = true, changes = {}, name = "the Old Tongue" }).id
  for i = 1, math.min(6, #elder) do
    local c = elder[i]
    local site = history.add(db, "site", { kind = "elder ruin", cell = c.id, pos = land.point(grid, c, 0.5, 0.5),
      name = language.name(peopling.language(db, db.old_tongue), "holy", "elder" .. i), language = db.old_tongue,
      built = now - depth - s:int(1000, 2900), cause = "the waning of the ley" })
    local ev = history.event(db, { year = site.built + s:int(400, 900), kind = "fall", participants = { site.id },
      place = site.id, importance = 0.7, title = site.name .. " is abandoned as the ley wanes" })
    site.fall = ev.id
    history.refer(db, site.id)
    history.refer(db, ev.id)
  end
  -- Realms: dynasties, rulers, deeds and tombs.
  for _, p in ipairs(db.polity) do
    job.pace(5)
    local capital = p.capital and history.get(db, p.capital)
    local founded = capital and (capital.founded + s:int(10, 300)) or (now - s:int(200, 600))
    founded = math.min(founded, now - 60)
    p.founded = founded
    history.event(db, { year = founded, kind = "rise", participants = { p.id, p.culture }, place = p.capital,
      importance = 0.8, title = "the realm of " .. p.name .. " is founded" })
    local year, dyn = founded, nil
    local n = 0
    while year < now do
      if not dyn or (s:chance(0.1) and year < now - 30) then
        local lang = peopling.speech(db, p.culture)
        dyn = history.add(db, "dynasty", { name = "House of " .. language.name(lang, "person", "house" .. p.id .. year),
          polity = p.id, from = year })
        if #db.dynasty > 1 and n > 0 then
          history.event(db, { year = year, kind = "succession", participants = { p.id, dyn.id }, place = p.capital,
            importance = 0.6, title = dyn.name .. " takes the throne of " .. p.name })
        end
      end
      n = n + 1
      local reign = s:int(8, 38)
      local good = s:chance(0.6)
      local ruler = person(db, p.culture, "ruler" .. p.id .. n, s, year - s:int(18, 40), p.title,
        { polity = p.id, dynasty = dyn.id, reign_from = year, reign_to = math.min(year + reign, now),
          epithet = good and s:pick(d.peoples.epithets.good) or s:pick(d.peoples.epithets.bad), good = good,
          alive = year + reign >= now })
      dyn.rulers = dyn.rulers or {}
      table.insert(dyn.rulers, ruler.id)
      history.event(db, { year = year, kind = "reign", participants = { p.id, ruler.id }, place = p.capital,
        importance = 0.25, title = ruler.name .. " " .. ruler.epithet .. " rules " .. p.name })
      if ruler.died and ruler.died <= now and capital then
        local tomb = history.add(db, "site", { kind = "tomb", of = ruler.id, cell = capital.cell,
          pos = world.move(capital.pos, s:float() * 6.28, 400 + s:float() * 1500, 0, 0),
          name = "the tomb of " .. ruler.name .. " " .. ruler.epithet })
        ruler.burial = tomb.id
      end
      if s:chance(0.12) then
        local work = s:pick({ "a great library", "an observatory", "a stone bridge", "a canal",
          "a garden of four parts" })
        history.event(db, { year = math.min(now - 1, year + s:int(1, reign)), kind = "great work",
          participants = { p.id, ruler.id }, place = p.capital, importance = 0.5,
          title = ruler.name .. " builds " .. work, work = work })
      end
      year = year + reign
    end
    p.dynasty = dyn.id
    p.ruler = db.figure[#db.figure].id
  end
  -- Wars along today's borders; the side holding the field today won.
  db.wars = {}
  for _, b in ipairs(db.borders) do
    local pa, pb = history.get(db, b.a), history.get(db, b.b)
    local wars = s:int(0, 2)
    for k = 1, wars do
      local start = math.max(pa.founded, pb.founded) + s:int(5, math.max(6, now - math.max(pa.founded, pb.founded) - 5))
      local winner = s:chance(0.5) and pa or pb
      local loser = winner == pa and pb or pa
      local field = grid.cells[winner == pa and b.cell or b.other]
      local name, native = battle_name(db, winner.culture, s)
      local cause = s:pick(d.peoples.war_causes)
      local war = history.event(db, { year = start, kind = "war", participants = { winner.id, loser.id },
        place = field.id, importance = 0.75, cause_text = cause, winner = winner.id, loser = loser.id,
        title = "the war of " .. pa.name .. " and " .. pb.name })
      local fallen = s:int(3, 60) * 100
      local site = history.add(db, "site", { kind = "battlefield", name = name, native = native, cell = field.id,
        pos = land.point(grid, field, 0.3 + s:float() * 0.4, 0.3 + s:float() * 0.4), war = war.id })
      local battle = history.event(db, { year = start + s:int(0, 3), kind = "battle", cause = war.id,
        participants = { winner.id, loser.id }, place = site.id, importance = 0.7, winner = winner.id,
        loser = loser.id, fallen = fallen, title = "the battle of " .. name })
      war.battle, war.site = battle.id, site.id
      site.battle = battle.id
      history.refer(db, site.id)
      loser.grudges = loser.grudges or {}
      table.insert(loser.grudges, { against = winner.id, war = war.id })
      history.refer(db, war.id)
      db.wars[#db.wars + 1] = war.id
      for _ = 1, s:int(1, 4) do
        history.event(db, { year = start + s:int(-40, 40), kind = "skirmish", participants = { pa.id, pb.id },
          place = field.id, importance = 0.1, title = "a skirmish on the border" })
      end
      if k == 1 and s:chance(0.4) then -- a general of the winners, remembered
        person(db, winner.culture, "general" .. war.id, s, start - s:int(25, 50), "general",
          { polity = winner.id, deed = war.id })
      end
    end
  end
  -- The land's own violence: eruptions, earthquakes and floods.
  for _, c in ipairs(grid.cells) do
    if db.cell_culture[c.id] then
      if c.relief > 900 and s:chance(0.25) then
        history.event(db, { year = now - s:int(20, depth), kind = s:chance(0.5) and "eruption" or "earthquake",
          participants = { db.cell_culture[c.id] }, place = c.id, importance = 0.55,
          title = "the mountain wakes in fire" })
      elseif c.river and c.flow > 6 and s:chance(0.2) then
        history.event(db, { year = now - s:int(5, depth), kind = "flood", participants = { db.cell_culture[c.id] },
          place = c.id, importance = 0.3, title = "the river floods" })
      elseif c.class == "desert" and s:chance(0.05) then
        history.event(db, { year = now - s:int(5, depth), kind = "famine", participants = { db.cell_culture[c.id] },
          place = c.id, importance = 0.2, title = "the wells run dry" })
      end
    end
  end
  -- Faiths: founded by prophets at their holy places.
  for _, f in ipairs(db.faith) do
    local cu = history.get(db, f.culture)
    local prophet = person(db, f.culture, "prophet" .. f.id, s, cu.settled + s:int(100, depth // 2), "prophet",
      { faith = f.id })
    f.prophet = prophet.id
    f.founded = prophet.born + s:int(25, 40)
    history.event(db, { year = f.founded, kind = "revelation", participants = { f.id, prophet.id },
      place = f.holy_sites[1], importance = 0.85, title = prophet.name .. " hears " .. f.deity })
    if s:chance(0.35) then
      history.event(db, { year = f.founded + s:int(150, 500), kind = "schism", participants = { f.id },
        place = f.holy_sites[1], importance = 0.5, title = "the faith of " .. f.deity .. " splits" })
    end
  end
  -- Ruins: places that fell, each for a cause.
  for _, st in ipairs(db.settlement) do
    if st.type ~= "city" and st.type ~= "caravanserai" and s:chance(chronicle.RUINS) then
      local cause = s:pick(d.peoples.ruin_causes)
      st.ruined = true
      st.population = 0
      local fell = st.founded + s:int(10, math.max(11, now - st.founded - 1))
      local ev = history.event(db, { year = fell, kind = "fall", participants = { st.id, st.culture }, place = st.id,
        importance = 0.6, cause_text = cause, title = st.name .. " falls to " .. cause })
      st.fall, st.fall_cause = ev.id, cause
      history.refer(db, st.id)
      history.refer(db, ev.id)
    end
  end
  -- Artifacts with a provenance.
  for i = 1, math.min(8, #db.polity) do
    local p = db.polity[i]
    local maker = person(db, p.culture, "maker" .. i, s, p.founded + s:int(0, 100), "master weaver")
    local art = history.add(db, "artifact", { name = s:pick({ "the Star Carpet", "the Horn of Dawn", "the Lapis Seal",
      "the Ember Crown", "the Loom of Hours", "the Silver Falcon" }) .. " of " .. p.name, made_by = maker.id,
      made = maker.born + s:int(25, 50), owners = { p.ruler } })
    local lost = db.wars[1 + (i % math.max(1, #db.wars))]
    if lost and s:chance(0.5) then
      art.lost_at = history.get(db, lost).site
      art.lost = history.get(db, lost).year
    end
  end
  if opts.story ~= false then chronicle.weave_story(db, grid, s, opts.anchor) end
  history.prune(db, 0.3)
  return db
end

-- The authored backstory woven into the generated history (concept 10.12).
function chronicle.weave_story(db, grid, s, anchor)
  local now = db.year
  -- Anarvan: the weavers' village where the story begins.
  local cell = anchor and land.cell_at(grid, anchor) or grid.cells[db.culture[1].cradle]
  if not db.cell_realm[cell.id] then -- (a shore no people reached: it belongs to the nearest settled land)
    local seen, frontier, found = { [cell.id] = true }, { cell.id }, nil
    while not found and #frontier > 0 do
      local nextf = {}
      for _, id in ipairs(frontier) do
        for _, nid in ipairs(grid.cells[id].nb) do
          if not seen[nid] then
            seen[nid] = true
            if db.cell_realm[nid] and not found then found = nid end
            nextf[#nextf + 1] = nid
          end
        end
      end
      frontier = nextf
    end
    if found then
      db.cell_culture[cell.id], db.cell_faith[cell.id] = db.cell_culture[found], db.cell_faith[found]
      db.cell_realm[cell.id] = db.cell_realm[found]
    end
  end
  local culture = db.cell_culture[cell.id] or db.culture[1].id
  local pos = anchor and world.move(anchor, 0, 0, 0, 0) or land.point(grid, cell, 0.5, 0.5)
  pos.alt = world.height(pos)
  local anarvan = history.add(db, "settlement", { name = "Anarvan", type = "village", pos = pos, cell = cell.id,
    culture = culture, faith = db.cell_faith[cell.id] or db.faith[1].id, realm = db.cell_realm[cell.id],
    buildings = 18, population = 90, prosperity = 0.6, founded = now - s:int(200, 400), story = "anarvan",
    trade = "weaving" })
  db.cell_settlements[cell.id] = db.cell_settlements[cell.id] or {}
  table.insert(db.cell_settlements[cell.id], anarvan.id)
  history.event(db, { year = anarvan.founded, kind = "founding", participants = { culture, anarvan.id },
    place = anarvan.id, importance = 0.9, title = "Anarvan is founded by weavers" })
  history.refer(db, anarvan.id)
  db.story = { anarvan = anarvan.id }
  local nearest, nd -- a road to the nearest place
  for _, st in ipairs(db.settlement) do
    if st ~= anarvan and not st.ruined then
      local d = world.distance(st.pos, pos)
      if not nd or d < nd then nearest, nd = st, d end
    end
  end
  if nearest then db.roads[#db.roads + 1] = { a = anarvan.id, b = nearest.id, length = nd, kind = "road" } end
  -- The Circle of the Loom: seven mages from many peoples, broken forty years ago.
  local circle_year = now - chronicle.CIRCLE_YEARS_AGO
  local members = {}
  for i = 1, 7 do
    local cu = db.culture[1 + (i - 1) % #db.culture]
    members[i] = person(db, cu.id, "circle" .. i, s, circle_year - s:int(30, 60), "mage of the Circle",
      { circle = true, alive = i == 7 })
  end
  local farrokh = members[7]
  farrokh.name, farrokh.born, farrokh.culture = "Farrokh", circle_year - 19, culture
  farrokh.role, farrokh.home = "master weaver of Anarvan", anarvan.id
  local azhdar = members[6]
  azhdar.name, azhdar.alive, azhdar.died = "Azhdar", true, nil
  azhdar.role = "the Ash Weaver"
  local hall_cell = grid.cells[db.culture[1].cradle]
  local hall = history.add(db, "site", { kind = "circle hall", name = "the Hall of the Loom", cell = hall_cell.id,
    pos = land.point(grid, hall_cell, 0.6, 0.4), reserved = true })
  local weaves = history.event(db, { year = circle_year - 12, kind = "great work", participants = {},
    place = hall.id, importance = 0.95, title = "the Circle of the Loom weaves the Great Weaves" })
  local breaking = history.event(db, { year = circle_year, kind = "breaking", participants = {}, place = hall.id,
    importance = 0.95, cause = weaves.id, title = "the Circle of the Loom is broken" })
  for _, m in ipairs(members) do
    table.insert(weaves.participants, m.id)
    table.insert(breaking.participants, m.id)
  end
  history.event(db, { year = circle_year + 1, kind = "hiding", participants = { farrokh.id }, place = anarvan.id,
    cause = breaking.id, importance = 0.9, title = "the Great Weaves are hidden" })
  history.event(db, { year = circle_year + 2, kind = "settling", participants = { farrokh.id }, place = anarvan.id,
    importance = 0.6, title = "a young weaver named Farrokh settles in Anarvan" })
  -- Azhdar's origin in a real city of a real realm.
  local origin
  for _, st in ipairs(db.settlement) do
    if st.type == "city" and st.id ~= anarvan.id and (not origin
      or world.distance(st.pos, pos) < world.distance(origin.pos, pos)) then origin = st end
  end
  if origin then
    azhdar.born, azhdar.home, azhdar.culture = circle_year - 30, origin.id, origin.culture
    history.event(db, { year = azhdar.born, kind = "birth", participants = { azhdar.id }, place = origin.id,
      importance = 0.8, title = "Azhdar is born in " .. origin.name })
    history.event(db, { year = circle_year + 15, kind = "crime", participants = { azhdar.id }, place = origin.id,
      importance = 0.8, title = "the library of " .. origin.name .. " burns" })
  end
  db.story.farrokh, db.story.azhdar, db.story.hall = farrokh.id, azhdar.id, hall.id
  for _, id in ipairs({ farrokh.id, azhdar.id, hall.id, weaves.id, breaking.id }) do history.refer(db, id) end
  return anarvan
end

-- Peoples a world and writes its chronicle: the Settled world, ready to play.
function chronicle.settled_world(seed, opts)
  local db, grid = peopling.settle(seed, opts)
  chronicle.write(db, grid, opts)
  return db, grid
end

return chronicle
