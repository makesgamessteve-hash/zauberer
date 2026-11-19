-- SPDX-License-Identifier: MIT
-- Settlements, buildings, castles and integrity in the peopled world: the
-- places near the heir come to be in layers; their layouts (streets, plots,
-- roles, walls, land use, terrain fit, special places, ruins); a city's
-- citadel of 100 rooms and more; walking into a house through its door,
-- stopped by walls, on its floors; doors and containers used and the
-- changes kept; fire burning timber.
scenario { name = "settlements" }
local game = demo.game
local places = demo.places
local layout = require("places.layout")
local castles = require("places.castles")
local wp = game.world_places
local P = wp.places
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if P.ready and world.chunk_stage(here) == 3 and sim.fire_at(here).loaded then break end
  sleep(5)
end
expect(P.ready, "the world is peopled")
step(2)

-- spec: settlements: Places come to be near the heirs
do
  local anarvan = places.find(P, here, "Anarvan")
  local plan = P.plans[anarvan.id]
  expect(plan and #plan.buildings >= 10, "Anarvan is laid out round the carpet")
  for _, b in ipairs(plan.buildings) do expect(b.w and b.storeys and b.roof, "every building is massed") end
  -- Stepping close plans and furnishes a building; going far forgets the place.
  local b = plan.buildings[#plan.buildings]
  local o = places.origin(P, b)
  for _ = 1, 20 do places.tick(P, { o }) end
  expect((b.stage or 1) >= 3, "a building in reach is furnished")
  local far = world.move(here, 0, 9000, 0, 0)
  local keep = P.plans
  places.tick(P, { far })
  expect(P.plans[anarvan.id] == nil, "far places are forgotten")
  P.plans = keep
  places.tick(P, { here })
end

local anarvan = places.find(P, here, "Anarvan")
local plan = P.plans[anarvan.id]

-- spec: settlements: Streets, plots and roles
do
  local mains, rings = 0, 0
  for _, s in ipairs(plan.streets) do
    if s.main then mains = mains + 1 elseif s.ring then rings = rings + 1 end
  end
  expect(mains >= 2 and plan.plaza.r > 8)
  local wells, homes = 0, 0
  for _, b in ipairs(plan.buildings) do
    local r = math.sqrt(b.x * b.x + b.z * b.z)
    if b.type == "well" then wells = wells + 1; expect(r < plan.plaza.r, "the well on the square") end
    if b.type == "home" or b.type == "farmhouse" or b.type == "weaver" then homes = homes + 1 end
    expect(r > plan.plaza.r or b.type == "well", "nothing built on the square")
  end
  expect(wells >= 1 and homes >= 5)
  for i, a in ipairs(plan.buildings) do -- plots do not overlap
    for j = i + 1, #plan.buildings do
      local c = plan.buildings[j]
      local d = math.sqrt((a.x - c.x) ^ 2 + (a.z - c.z) ^ 2)
      expect(d > 4, "buildings stand apart")
    end
  end
  local weavers = 0
  for _, b in ipairs(plan.buildings) do if b.type == "weaver" then weavers = weavers + 1 end end
  expect(weavers >= 1, "Anarvan is a village of weavers")
  -- A town's roles near its centre, its smithy at the edge.
  local st = { id = "test:town", seed = 5, type = "market_town", buildings = 120, prosperity = 0.6,
    culture = anarvan.culture,
    pos = anarvan.pos }
  local ctx = places.context(P, anarvan)
  ctx.roles = demo.places and require("people.peopling").data().type.market_town.roles
  local town = layout.plan(st, ctx)
  local by = {}
  for _, b in ipairs(town.buildings) do by[b.type] = b end
  expect(by.temple and by.bazaar and by.caravanserai and by.bathhouse and by.smithy, "a market town's roles")
  local function r(b) return math.sqrt(b.x * b.x + b.z * b.z) end
  expect(r(by.bazaar) < r(by.smithy), "the bazaar nearer the centre than the smithy")
  expect(#town.streets >= 6 and #town.fields == 6)
end

-- spec: settlements: Walls, gates and land use
do
  local ctx = places.context(P, anarvan)
  ctx.roles = require("people.peopling").data().type.city.roles
  local city = layout.plan({ id = "test:city", seed = 9, type = "city", buildings = 300, prosperity = 0.8,
    walled = true,
    culture = anarvan.culture, pos = anarvan.pos }, ctx)
  expect(#city.walls == 24 and #city.towers == 8 and #city.gates >= 4, "walls with towers and gates")
  for _, g in ipairs(city.gates) do
    expect(math.abs(math.sqrt(g.x * g.x + g.z * g.z) - city.wall_radius) < 0.01, "gates in the wall")
  end
  for _, b in ipairs(city.buildings) do
    if not b.castle then expect(math.sqrt(b.x * b.x + b.z * b.z) < city.wall_radius, "inside the walls") end
  end
  local kinds = {}
  for _, f in ipairs(city.fields) do
    kinds[f.kind] = true
    expect(f.r0 > city.radius and f.r1 > f.r0)
    if f.kind == "fields" then expect(f.crop) end
  end
  expect(kinds.fields or kinds.pasture or kinds.orchards, "land use round about")
  -- The citadel: on the high ground, 100 rooms and more.
  expect(city.castle and city.castle.rooms >= 100, "a citadel of 100 rooms and more")
  expect(places.rooms(city) >= 100)
end

-- spec: castles: Siting and the curtain wall
do
  local ctx = places.context(P, anarvan)
  local s = require("people.rng").new(3)
  local c = castles.plan({ id = "test:castle", seed = 3, prosperity = 0.5 }, ctx, s)
  local core = ctx.height(c.x, c.z)
  for r = 0, 300, 100 do
    for k = 0, 15 do
      local a = k * math.pi / 8
      expect(ctx.height(math.sin(a) * r, math.cos(a) * r) <= core + 1e-6, "on the highest ground about")
    end
  end
  expect(#c.walls == 16 and #c.towers == 8 and c.gate.portcullis and c.gate.murder_holes)
  for _, w in ipairs(c.walls) do expect(w.walk and w.crenels and w.h >= 6) end
  local types = {}
  for _, b in ipairs(c.buildings) do types[b.type] = true end
  expect(types.gatehouse and types.palace and types.barracks and types.stables and types.armoury and types.kitchens)
  expect(#c.cellars == 6 and c.secret.to, "cellars, a dungeon and a secret way out")
  expect(c.rooms >= 50 and c.rooms <= 160)
end

-- spec: castles: States and styles
do
  local ctx = places.context(P, anarvan)
  local seen = {}
  for k = 1, 40 do
    local c = castles.plan({ id = "test:c" .. k, seed = k }, ctx, require("people.rng").new(k))
    seen[c.state] = true
    if c.state == "abandoned" then expect_eq(c.garrison, 0) end
    if c.state == "held" then expect(c.garrison > 0) end
    if c.state == "ruined" then expect(c.buildings[1].ruin) end
  end
  expect(seen.held and seen.abandoned and seen.ruined)
  local held = castles.plan({ id = "x", seed = 1, castle_state = "the Ashen" }, ctx, require("people.rng").new(1))
  expect_eq(held.state, "the Ashen")
  for style, st in pairs(castles.STYLES) do expect(st.wall and st.towers and st.height > 0, style) end
end

-- spec: settlements: Terrain fit
do
  local flat = { type = "home", x = 0, z = 0, plot = { w = 10, d = 10 } }
  local ctx = { height = function(x) return x * 0.5 end, water = function() return true end }
  local st = { id = "test:slope", seed = 1, type = "stilt_village", buildings = 12, stilts = true,
    culture = anarvan.culture }
  local fam = require("people.peopling").data().family.far_east
  local plan2 = layout.plan(st, { family = fam, land = {}, height = ctx.height, water = function() return false end,
    roles = {} })
  for _, b in ipairs(plan2.buildings) do expect(b.stilts == 2, "a stilt village on stilts") end
  local steep = layout.plan({ id = "test:steep", seed = 2, type = "village", buildings = 20 },
    { family = fam, land = {}, height = function(x, z) return x + z end, water = function() return false end,
      roles = {} })
  local terraced, stepped = false, false
  for _, b in ipairs(steep.buildings) do terraced = terraced or b.terraced end
  for _, s in ipairs(steep.streets) do stepped = stepped or s.steps end
  expect(terraced and stepped, "plinths on slopes and steps in steep streets")
  local _ = flat
end

-- spec: settlements: Special places
do
  local ctx = places.context(P, anarvan)
  local function make(t, n) return layout.plan({ id = "test:" .. t, seed = 4, type = t, buildings = n or 10 }, ctx) end
  local serai = make("caravanserai", 1)
  expect(serai.buildings[1].type == "caravanserai" and serai.buildings[2].type == "well")
  local camp = make("nomad_camp", 12)
  for _, b in ipairs(camp.buildings) do expect_eq(b.type, "tent") end
  expect(camp.fire and camp.fields[1].kind == "pasture")
  local tower = make("mage_tower", 3)
  expect(tower.buildings[1].type == "tower" and #tower.buildings == 3)
  local cliff = make("cliff_dwellings", 20)
  expect(#cliff.buildings == 20 and cliff.buildings[1].carved)
  local cit = make("citadel")
  expect(cit.castle and #cit.buildings >= 5)
end

-- spec: settlements: Ruins
do
  local ctx = places.context(P, anarvan)
  local ruin = layout.plan({ id = "test:ruin", seed = 6, type = "village", buildings = 15, ruined = true,
    ruin_age = 300,
    fall_cause = "war" }, ctx)
  expect(ruin.ruined)
  P.plans["test:ruin"] = ruin
  ruin.settlement = { id = "test:ruin", pos = anarvan.pos }
  P.db.by_id["test:ruin"] = ruin.settlement
  ruin.family = ctx.family
  local b = ruin.buildings[#ruin.buildings]
  b.settlement = "test:ruin"
  require("places.buildings").generate(b, ctx.family, 1)
  places.realise(P, b, 3)
  local roofs, gone = 0, 0
  for _, p in ipairs(b.pieces) do
    if p.kind == "roof" and not p.gone then roofs = roofs + 1 end
    if p.gone then gone = gone + 1 end
  end
  expect(roofs == 0 and gone > 10 and b.ruin.cause == "war", "roofless, fallen and looted")
  expect(b.burial ~= nil and b.overgrown ~= nil)
  P.plans["test:ruin"] = nil
  P.db.by_id["test:ruin"] = nil
end

-- spec: building-generation: Seamless interiors
do
  local house
  for _, b in ipairs(plan.buildings) do
    if not house and (b.type == "home" or b.type == "weaver") then house = b end
  end
  places.realise(P, house, 3)
  local door
  for _, d in ipairs(house.doors) do if d.kind == "front" then door = d end end
  local outside = places.to_world(P, house, door.pos, 0, house.d / 2 + 2.5)
  local w = demo.walker.new(outside, house.yaw + math.pi, { move = world.move, height = world.ground_height,
    water = world.water })
  w.pos.alt = places.origin(P, house).alt
  game.walker = w
  game.carpet.pos = world.move(outside, house.yaw, 6, 6, 0) -- (out of the way)
  local function walk(seconds)
    for _ = 1, seconds * 60 do
      input.inject("steer", 0, 1, 1)
      step(1)
    end
  end
  walk(2)
  local _, x, _, z = places.at(P, w.pos)
  expect(z == nil or z > house.d / 2 - 0.5, "a closed door stops the walker")
  expect(w.bumped or (z and z > house.d / 2 - 0.5))
  -- Opening the door: the walker goes in and stands on the floor.
  local b, piece, kind = places.nearby(P, w.pos, w.heading)
  expect(b == house and kind == "door", "the door is in reach")
  places.toggle_door(P, house, piece)
  walk(2)
  local inb, ix, iy, iz = places.at(P, w.pos)
  expect(inb == house and math.abs(iz) < house.d / 2, "inside the house")
  expect(math.abs(iy) < 0.7, "standing on its floor")
  expect_near(places.floor(P, w.pos, w.pos.alt), places.origin(P, house).alt, 0.01)
  local _ = x .. ix
  -- Walls stop the walker inside too: walk on till stopped.
  walk(6)
  local _, _, _, z2 = places.at(P, w.pos)
  expect(z2 and z2 > -house.d / 2, "the back wall holds")
  -- Use: a container's things into the pack, once.
  local box
  for n, i in ipairs(house.containers) do
    if #places.container(P, house, n) > 0 and not box then box = { n = n, piece = house.pieces[i] } end
  end
  if box then
    local first = places.container(P, house, box.n)[1]
    local got = places.take(P, house, box.n, first[1], first[2])
    expect(got == first[2] and #places.container(P, house, box.n) < #places.container(P, house, box.n) + 1)
    expect_eq(places.take(P, house, box.n, first[1], 1), 0)
  end
  -- The changes are kept with the save and come back.
  local saved = places.snapshot(P)
  expect(saved.deltas[house.id].doors and saved.deltas[house.id].doors[tostring(door.id)] == "open")
  local back = places.new(saved)
  expect(back.deltas[house.id].doors[tostring(door.id)] == "open")
  expect(places.new({ deltas = { [3] = {} } }).deltas[3] == nil)
  game.walker = nil
end

-- spec: structural-integrity: Fire burns timber
do
  local wood, best
  for _, b in ipairs(plan.buildings) do
    local d = world.distance(places.origin(P, b), game.carpet.pos)
    if not best or d < best then best, wood = d, { b = b } end
  end
  places.realise(P, wood.b, 3)
  for i, p in ipairs(wood.b.pieces) do -- something wooden on the ground floor (or the house's door)
    if not wood.i and not p.gone and p.mat == "timber" and p.y < 1 then wood.i = i end
  end
  expect(wood.i, "a timber piece")
  local p = wood.b.pieces[wood.i]
  local at = places.to_world(P, wood.b, p.x, p.y, p.z)
  expect(sim.ignite(at, 2) > 0, "it catches")
  local burnt = false
  for _ = 1, 600 do
    places.burn(P, wood.b)
    step(1)
    if p.gone then burnt = true break end
  end
  expect(burnt, "the fire burns the timber away")
  expect(P.deltas[wood.b.id].gone and #P.deltas[wood.b.id].gone >= 1)
  -- A smashed wall brings down what it held, and it stays down when the place is made again.
  local lost = places.damage(P, wood.b, wood.i)
  expect(#lost == 0, "already gone")
end
