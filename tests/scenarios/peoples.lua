-- SPDX-License-Identifier: MIT
-- The Settled world peopled from the engine's world: the land in regions,
-- peoples spread from cradles, languages, faiths, realms, places and roads,
-- the reconstructed chronicle and the story woven in, the same for every
-- heir. (Generation is too long for one budgeted call, so it is tested here,
-- where scenario steps run unbudgeted.)
scenario { name = "peoples" }
local land = require("people.land")
local peopling = require("people.peopling")
local history = require("people.history")
local chronicle = require("people.chronicle")
local job = require("people.job")

local SEED = 4242
local anchor = world.position(0, 6506, 6025, 128, 128, 0)
local j = job.new(chronicle.settled_world, SEED, { anchor = anchor })
local slices = 0
while not job.run(j, 1) do slices = slices + 1 end
expect(slices > 10, "generation gives way many times")
local db, grid = j.result, j.extra
local test = { expect = expect, expect_eq = expect_eq, expect_error = expect_error }
local function case(_, fn) fn() end

-- spec: peoples-and-cultures: The land as peoples see it
case("the land in regions with climate, water and neighbours", function()
  test.expect(#grid.cells == 6 * grid.n * grid.n and grid.n == land.size(world.planet().radius))
  local landn, rivers, classes = 0, 0, {}
  for _, c in ipairs(grid.cells) do
    test.expect(#c.nb >= 3)
    if c.land then
      landn = landn + 1
      classes[c.class] = true
      test.expect(c.fertility >= 0 and c.fertility <= 1)
    end
    if c.river then rivers = rivers + 1 end
  end
  test.expect(landn > 100 and rivers > 0 and classes.desert and classes.forest and classes.steppe)
  local c = land.cell_at(grid, anchor)
  test.expect(c and land.cell_at(grid, land.point(grid, c, 0.5, 0.5)) == c)
end)

-- spec: peoples-and-cultures: Related languages
-- spec: peoples-and-cultures: Peoples from cradles
case("peoples settle cradles and spread, living off their land", function()
  test.expect_eq(#db.culture, peopling.PEOPLES)
  local families, cells = {}, {}
  for _, cu in ipairs(db.culture) do
    families[cu.family] = true
    test.expect(cu.name and cu.language and cu.subsistence and #cu.values == 2 and cu.magic and cu.hospitality)
    test.expect(cu.calendar and #cu.festivals == 1 and cu.cuisine and cu.textiles and cu.palette)
    test.expect_eq(db.cell_culture[cu.cradle], cu.id)
    test.expect(grid.cells[cu.cradle].fertility > 0.2)
  end
  for _, cu in pairs(db.cell_culture) do cells[cu] = (cells[cu] or 0) + 1 end
  local n = 0
  for _ in pairs(families) do n = n + 1 end
  test.expect(n >= 3, "several style families")
  for _, cu in ipairs(db.culture) do test.expect((cells[cu.id] or 0) >= 1) end
  -- Deserts and mountains are crossed less than rivers and coasts.
  local settled_desert, desert = 0, 0
  for _, c in ipairs(grid.cells) do
    if c.class == "desert" and not c.oasis then
      desert = desert + 1
      if db.cell_culture[c.id] then settled_desert = settled_desert + 1 end
    end
  end
  test.expect(desert == 0 or settled_desert < desert)
  local d = peopling.data()
  test.expect(peopling.family_fit(d.family.arabian, { temp = 26, rain = 80, relief = 50 })
    > peopling.family_fit(d.family.arabian, { temp = -5, rain = 900, relief = 50 }))
  test.expect_eq(peopling.env_of({ coast = true, class = "forest", relief = 0 }), "sea")
  test.expect_eq(peopling.env_of({ class = "desert", relief = 0 }), "desert")
  test.expect_eq(peopling.env_of({ class = "ice", relief = 0 }), "ice")
  test.expect_eq(peopling.env_of({ class = "grassland", relief = 0 }), "grassland")
  test.expect_eq(peopling.env_of({ class = "tundra", relief = 0 }), "steppe")
  test.expect_eq(peopling.env_of({ class = "mountain", relief = 0 }), "mountain")
  test.expect_eq(peopling.env_of({ class = "swamp", relief = 0 }), "forest")
  -- Languages: one per people plus the proto-languages and the Old Tongue.
  local protos = 0
  for _, l in ipairs(db.language) do if l.proto then protos = protos + 1 end end
  test.expect(protos >= 2 and #db.language == #db.culture + protos)
  test.expect_eq(peopling.speech(db, db.culture[1].id), peopling.speech(db, db.culture[1].id))
end)

-- spec: peoples-and-cultures: Faiths
case("faiths with doctrines from their homeland, spread with majorities and minorities", function()
  test.expect(#db.faith >= 2 and #db.faith <= 6)
  for _, f in ipairs(db.faith) do
    test.expect(f.deity and f.concept and f.imagery and f.magic and f.burial and f.prayers and f.diet)
    test.expect(f.sacred.animal and f.sacred.plant and #f.holy_sites >= 1 and f.prophet and f.founded < db.year)
    test.expect(f.concept ~= "a colour of mana" or f.colour)
    test.expect_eq(history.get(db, f.holy_sites[1]).kind, "holy")
  end
  local settled, believers, minorities = 0, 0, 0
  for id in pairs(db.cell_culture) do
    settled = settled + 1
    if db.cell_faith[id] then believers = believers + 1 end
    if db.cell_minority[id] then
      minorities = minorities + 1
      test.expect(db.cell_minority[id] ~= db.cell_faith[id])
    end
  end
  test.expect(believers == settled and minorities > 0)
end)

-- spec: peoples-and-cultures: Realms
case("realms around capitals, their kind and their law on mages", function()
  test.expect(#db.polity >= #db.culture)
  for _, p in ipairs(db.polity) do
    test.expect(p.name and p.title and p.mages and #p.laws == 2 and p.capital and p.dynasty and p.ruler)
    test.expect_eq(history.get(db, p.capital).realm, p.id)
    test.expect(p.mages == "mages are welcome" or p.mages == "mages need a licence" or p.mages == "mages are hunted"
      or p.mages == "ruled by mages")
  end
  local owned = 0
  for _ in pairs(db.cell_realm) do owned = owned + 1 end
  local settled = 0
  for _ in pairs(db.cell_culture) do settled = settled + 1 end
  test.expect_eq(owned, settled)
  test.expect(#db.borders > 0)
  for _, b in ipairs(db.borders) do
    test.expect(db.cell_realm[b.cell] == b.a and db.cell_realm[b.other] == b.b)
  end
end)

-- spec: peoples-and-cultures: Present-day places
case("settlements sited, typed and sized; roads; caravanserais a day apart", function()
  local types = {}
  for _, st in ipairs(db.settlement) do
    types[st.type] = (types[st.type] or 0) + 1
    test.expect(st.name and st.pos and st.culture and st.population >= 0 and st.buildings >= 1)
    test.expect(st.ruined or world.height(st.pos) > 0 or st.story)
    test.expect(st.founded and st.founded < db.year)
  end
  test.expect(types.city and types.village and types.caravanserai, "cities, villages, caravanserais")
  test.expect(#db.roads > 0)
  for _, r in ipairs(db.roads) do test.expect(history.get(db, r.a) and history.get(db, r.b) and r.length > 0) end
  local near = peopling.near(db, grid, db.settlement[1].pos, 50000)
  test.expect(near[1].settlement == db.settlement[1] and near[1].distance == 0)
  for i = 2, #near do test.expect(near[i].distance >= near[i - 1].distance) end
end)

-- spec: peoples-and-cultures: The history database
-- spec: peoples-and-cultures: A reconstructed chronicle
case("the chronicle agrees with the map", function()
  test.expect(#db.events > 100 and #db.figure > 20 and #db.dynasty >= #db.polity and #db.eras == 4)
  for _, wid in ipairs(db.wars) do
    local war = history.get(db, wid)
    local site = history.get(db, war.site)
    test.expect_eq(db.cell_realm[site.cell], war.winner, "the winner holds the field today")
    test.expect(history.get(db, war.battle).fallen > 0 and site.name)
    local loser = history.get(db, war.loser)
    local grudge = false
    for _, g in ipairs(loser.grudges) do if g.war == wid then grudge = true end end
    test.expect(grudge)
  end
  local ruins = 0
  for _, st in ipairs(db.settlement) do
    if st.ruined then
      ruins = ruins + 1
      test.expect(st.fall_cause and history.get(db, st.fall) and st.population == 0)
    end
  end
  test.expect(ruins > 0)
  local tombs = 0
  for _, site in ipairs(db.site) do
    if site.kind == "tomb" then
      tombs = tombs + 1
      test.expect_eq(history.get(db, site.of).burial, site.id)
    end
  end
  test.expect(tombs > 0)
  for _, ev in ipairs(db.events) do test.expect(ev.year <= db.year, ev.title or ev.kind) end
  local kinds = {}
  for _, ev in ipairs(db.events) do kinds[ev.kind] = true end
  test.expect(kinds.war and kinds.battle and kinds.revelation and kinds.fall and kinds.summary and kinds.settling)
  test.expect(#db.artifact > 0 and history.get(db, db.artifact[1].made_by))
end)

-- spec: peoples-and-cultures: The story woven in
case("Anarvan, Farrokh, the Circle of the Loom and Azhdar", function()
  local an = history.get(db, db.story.anarvan)
  test.expect(an.name == "Anarvan" and world.distance(an.pos, anchor) < 1 and an.story == "anarvan")
  local farrokh, azhdar = history.get(db, db.story.farrokh), history.get(db, db.story.azhdar)
  test.expect(farrokh.name == "Farrokh" and farrokh.home == an.id and farrokh.circle and farrokh.alive)
  test.expect(azhdar.name == "Azhdar" and azhdar.alive and history.get(db, azhdar.home).type == "city")
  local breaking = history.events(db, { kind = "breaking" })[1]
  test.expect_eq(breaking.year, db.year - chronicle.CIRCLE_YEARS_AGO)
  test.expect_eq(#breaking.participants, 7)
  test.expect(history.get(db, db.story.hall).reserved)
  local quiet = chronicle.settled_world(SEED, { story = false })
  test.expect(quiet.story == nil)
end)

-- spec: peoples-and-cultures: The same history for every heir
case("every heir's game makes the same history from the seed", function()
  local again = chronicle.settled_world(SEED, { anchor = anchor })
  local function digest(h)
    local parts = {}
    for _, st in ipairs(h.settlement) do parts[#parts + 1] = st.name .. st.type .. st.population end
    for _, ev in ipairs(h.events) do parts[#parts + 1] = ev.year .. ev.kind end
    return engine.hash_state(table.concat(parts, "|"))
  end
  test.expect_eq(digest(again), digest(db))
  local other = chronicle.settled_world(SEED + 1, { anchor = anchor })
  test.expect(digest(other) ~= digest(db))
end)
