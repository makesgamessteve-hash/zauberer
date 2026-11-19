-- SPDX-License-Identifier: MIT
-- Settlement layout (concept 6.4): main streets run from a centre (square,
-- market or crossing) out towards the places the roads lead to, ring
-- streets follow round it; plots line the streets, sized by wealth and
-- density; buildings are assigned by role (temples, bazaar, caravanserai,
-- bathhouse and teahouse near the centre, wells on the square, smithies at
-- the edge, docks on the shore); walls with gates and towers ring the walled
-- places; fields, pastures, orchards, woodlots and fishing grounds lie
-- around. Buildings stand on plinths on slopes and on stilts over water;
-- steep streets get steps. Special places have their own plans: a single
-- caravanserai, a nomad camp of tents round a fire, a mage's tower, cliff
-- dwellings along a slope. Ruins decay. Coordinates are metres east (x) and
-- north (z) of the settlement's centre; deterministic from its seed.
local rng = require("people.rng")

local layout = {}

layout.MAX_BUILDINGS = 400 -- buildings actually laid out (the rest of a big city is suggested)
layout.STREET = 4          -- metres wide (main streets 6)
layout.STEEP = 0.25        -- slope from which a street has steps

local TRADES = { "farmer", "farmer", "herder", "weaver", "smith", "potter", "herbalist", "merchant", "scholar" }

local function add_building(plan, rec)
  plan.buildings[#plan.buildings + 1] = rec
  rec.n = #plan.buildings
  rec.id = plan.id .. "/" .. rec.n
  return rec
end

-- A grid of what is taken, for keeping plots apart.
local function free(plan, x, z, r)
  local cell = 16
  local gx, gz = math.floor(x / cell), math.floor(z / cell)
  for i = gx - 2, gx + 2 do
    for j = gz - 2, gz + 2 do
      for _, o in ipairs(plan.taken[i * 100003 + j] or {}) do
        local dx, dz = o[1] - x, o[2] - z
        if dx * dx + dz * dz < (o[3] + r) ^ 2 then return false end
      end
    end
  end
  return true
end

local function take(plan, x, z, r)
  local key = math.floor(x / 16) * 100003 + math.floor(z / 16)
  plan.taken[key] = plan.taken[key] or {}
  table.insert(plan.taken[key], { x, z, r })
end

-- Fits a building to the ground: a plinth on slopes (terraced), stilts over
-- water. ctx.height(x, z) and ctx.water(x, z).
local function fit(b, ctx)
  local r = math.max(b.plot.w, b.plot.d) / 2
  local lo, hi = math.huge, -math.huge
  for _, o in ipairs({ { -r, -r }, { r, -r }, { -r, r }, { r, r }, { 0, 0 } }) do
    local h = ctx.height(b.x + o[1], b.z + o[2])
    lo, hi = math.min(lo, h), math.max(hi, h)
  end
  b.ground = hi
  b.plinth = hi - lo
  b.terraced = b.plinth > 3
  if ctx.water and ctx.water(b.x, b.z) then b.stilts = 2.0 end
end

local function role_type(role)
  if role == "qanat_outlet" or role == "cistern" then return role end
  return role
end

-- Plans a settlement. st: the settlement record; ctx: { family (style
-- family), land (class, temp, rain, coast, river), height(x, z),
-- water(x, z), roads (headings in radians to the places roads lead to),
-- castle (the castles module, for citadels) }.
function layout.plan(st, ctx)
  local s = rng.new(rng.hash(st.seed or 0, st.id), "layout")
  local plan = { id = st.id, buildings = {}, streets = {}, walls = {}, towers = {}, gates = {}, fields = {},
    taken = {}, type = st.type }
  local land = ctx.land or {}
  local want = math.min(st.buildings or 1, layout.MAX_BUILDINGS)
  local function household(rank)
    local wealth = math.max(0, math.min(1, (st.prosperity or 0.5) * (1.3 - rank * 0.8) + s:float() * 0.2))
    local local_trade = ({ weaving = "weaver", smithing = "smith", trade = "merchant" })[st.trade or ""]
    local trade = local_trade or s:pick(TRADES)
    if local_trade and s:chance(0.4) then trade = s:pick(TRADES) end
    return { wealth = wealth, trade = trade, people = 2 + s:int(0, 5) }
  end
  local function building(btype, x, z, yaw, rank, size)
    local rec = { type = btype, x = x, z = z, yaw = yaw, seed = rng.hash(st.seed or 0, st.id, #plan.buildings + 1),
      household = household(rank or 0.5), plot = { w = size or 12, d = size or 12 }, style = ctx.family.id,
      land = land }
    fit(rec, ctx)
    return add_building(plan, rec)
  end
  -- Special plans.
  if st.type == "caravanserai" then
    building("caravanserai", 0, 0, s:float() * 2 * math.pi, 0.2, 40)
    building("well", 26, 0, 0, 0.5, 3)
    plan.radius = 30
    return layout.finish(plan, st, ctx)
  elseif st.type == "citadel" then
    plan.castle = ctx.castle.plan(st, ctx, s)
    plan.radius = plan.castle.radius
    for _, b in ipairs(plan.castle.buildings) do add_building(plan, b) end
    return layout.finish(plan, st, ctx)
  elseif st.type == "nomad_camp" then
    local n = math.max(5, math.min(30, want))
    local ring = 6 + n * 1.3
    for i = 1, n do
      local a = (i - 1) / n * 2 * math.pi
      building("tent", math.sin(a) * ring, math.cos(a) * ring, a, 0.5, 7)
    end
    plan.fire = { x = 0, z = 0 }
    plan.radius = ring + 6
    plan.fields[1] = { kind = "pasture", r0 = ring + 10, r1 = ring + 300, from = 0, to = 2 * math.pi }
    return layout.finish(plan, st, ctx)
  elseif st.type == "mage_tower" then
    building("tower", 0, 0, s:float() * 6.28, 0.1, 9)
    for i = 1, math.max(1, math.min(3, want - 1)) do
      local a = i * 2.1
      building(i == 1 and "storehouse" or "home", math.sin(a) * 16, math.cos(a) * 16, a + math.pi, 0.6, 10)
    end
    plan.radius = 25
    return layout.finish(plan, st, ctx)
  elseif st.type == "cliff_dwellings" then
    local best, slope = 0, -1
    for k = 0, 7 do -- the steepest direction
      local a = k * math.pi / 4
      local sl = math.abs(ctx.height(math.sin(a) * 40, math.cos(a) * 40) - ctx.height(0, 0))
      if sl > slope then best, slope = a, sl end
    end
    local along = best + math.pi / 2
    for i = 1, math.max(6, math.min(60, want)) do
      local row = (i - 1) // 12
      local t = ((i - 1) % 12 - 5.5) * 8
      local x = math.sin(along) * t + math.sin(best) * row * 6
      local z = math.cos(along) * t + math.cos(best) * row * 6
      local b = building("home", x, z, best + math.pi, 0.5, 7)
      b.carved = true
    end
    plan.radius = 60
    return layout.finish(plan, st, ctx)
  end
  -- The ordinary plan: a centre and streets.
  local plaza = 8 + math.sqrt(want) * 1.2
  plan.plaza = { x = 0, z = 0, r = plaza, kind = (st.type == "fishing_village" and "harbour")
    or ((st.type == "market_town" or st.type == "city" or st.type == "oasis_town") and "market") or "square" }
  local headings = {}
  for _, h in ipairs(ctx.roads or {}) do headings[#headings + 1] = h end
  local mains = want > 120 and 6 or (want > 40 and 4 or (want > 10 and 3 or 2))
  local base = s:float() * 2 * math.pi
  while #headings < mains do headings[#headings + 1] = base + #headings * 2 * math.pi / mains end
  local radius = plaza + 14 * math.sqrt(want) + 20
  for i, h in ipairs(headings) do
    local x1, z1 = math.sin(h) * radius, math.cos(h) * radius
    plan.streets[#plan.streets + 1] = { x0 = math.sin(h) * plaza, z0 = math.cos(h) * plaza, x1 = x1, z1 = z1,
      w = 6, main = true, heading = h, index = i }
  end
  local rings = math.floor((radius - plaza) / 28)
  for k = 1, rings do
    plan.streets[#plan.streets + 1] = { ring = plaza + 28 * k - 6, w = layout.STREET }
  end
  take(plan, 0, 0, plaza)
  for _, sr in ipairs(plan.streets) do -- streets are kept clear
    if sr.main then
      for t = 0, 1, 0.05 do take(plan, sr.x0 + (sr.x1 - sr.x0) * t, sr.z0 + (sr.z1 - sr.z0) * t, 3) end
    end
  end
  -- Plots: along main streets outwards, then along the rings.
  local slots = {}
  local spacing = 9 + (1 - (st.prosperity or 0.5)) * 2
  for _, sr in ipairs(plan.streets) do
    if sr.main then
      local len = math.sqrt((sr.x1 - sr.x0) ^ 2 + (sr.z1 - sr.z0) ^ 2)
      local dx, dz = (sr.x1 - sr.x0) / len, (sr.z1 - sr.z0) / len
      for t = spacing / 2, len - 10, spacing do
        for _, side in ipairs({ -1, 1 }) do
          local off = sr.w / 2 + 6.5
          local x = sr.x0 + dx * t + dz * off * side
          local z = sr.z0 + dz * t - dx * off * side
          slots[#slots + 1] = { x = x, z = z, r = math.sqrt(x * x + z * z), face = { -dz * side, dx * side } }
        end
      end
    elseif sr.ring and sr.ring + sr.w / 2 + 13 < radius then
      local n = math.floor(2 * math.pi * sr.ring / spacing)
      for i = 1, n do
        local a = i / n * 2 * math.pi
        local x, z = math.sin(a) * (sr.ring + sr.w / 2 + 6.5), math.cos(a) * (sr.ring + sr.w / 2 + 6.5)
        slots[#slots + 1] = { x = x, z = z, r = sr.ring, face = { -math.sin(a), -math.cos(a) } }
      end
    end
  end
  for _, sl in ipairs(slots) do -- a building's front faces the street: its yaw is the heading of the face
    sl.yaw = math.atan(sl.face[1], sl.face[2])
  end
  table.sort(slots, function(a, b)
    if math.abs(a.r - b.r) > 0.01 then return a.r < b.r end
    if a.x ~= b.x then return a.x < b.x end
    return a.z < b.z
  end)
  -- Roles first, nearest the centre; then homes.
  local roles = {}
  for _, r in ipairs(ctx.roles or {}) do roles[#roles + 1] = r end
  local placed = 0
  local wells = 0
  for _, role in ipairs(roles) do
    if role == "well" or role == "cistern" or role == "qanat_outlet" then
      wells = wells + 1
      local a = wells * 2.4
      building(role_type(role), math.sin(a) * plaza * 0.5, math.cos(a) * plaza * 0.5, a, 0.5, role == "well" and 3 or 7)
      placed = placed + 1
    end
  end
  local edge = {}
  for _, role in ipairs(roles) do
    if role == "smithy" then edge[#edge + 1] = role
    elseif role == "dock" then
      if land.coast or land.river then edge[#edge + 1] = role end
    elseif role == "citadel" then
      plan.citadel = true
    elseif role ~= "well" and role ~= "cistern" and role ~= "qanat_outlet" then
      for _, sl in ipairs(slots) do
        if not sl.used and free(plan, sl.x, sl.z, 9) then
          sl.used = true
          take(plan, sl.x, sl.z, 9)
          building(role, sl.x, sl.z, sl.yaw, 0.2, 18)
          placed = placed + 1
          break
        end
      end
    end
  end
  local homes_from = #plan.buildings
  for _, sl in ipairs(slots) do
    if placed >= want then break end
    if not sl.used and free(plan, sl.x, sl.z, 5.5) and not (ctx.water and ctx.water(sl.x, sl.z) and not st.stilts) then
      sl.used = true
      take(plan, sl.x, sl.z, 5.5)
      local rank = sl.r / radius
      local btype = "home"
      if (st.type == "village" or st.type == "hamlet") and rank > 0.5 then btype = "farmhouse" end
      local b = building(btype, sl.x, sl.z, sl.yaw, rank, 12)
      if b.household.trade == "weaver" and st.trade == "weaving" then b.type = "weaver" end -- (a weavers' village)
      placed = placed + 1
    end
  end
  for _, role in ipairs(edge) do -- at the edge, downwind or on the shore
    for k = #slots, 1, -1 do
      local sl = slots[k]
      if not sl.used and free(plan, sl.x, sl.z, 6) then
        sl.used = true
        take(plan, sl.x, sl.z, 6)
        building(role, sl.x, sl.z, sl.yaw, 0.5, 11)
        break
      end
    end
  end
  plan.homes_from = homes_from
  plan.radius = radius
  if st.stilts then
    for _, b in ipairs(plan.buildings) do b.stilts = 2.0 end
  end
  -- Walls with gates where the main streets leave, and towers.
  if st.walled then
    local wr = radius + 8
    local n = 24
    for i = 0, n - 1 do
      local a0, a1 = i / n * 2 * math.pi, (i + 1) / n * 2 * math.pi
      plan.walls[#plan.walls + 1] = { x0 = math.sin(a0) * wr, z0 = math.cos(a0) * wr, x1 = math.sin(a1) * wr,
        z1 = math.cos(a1) * wr }
      if i % 3 == 0 then plan.towers[#plan.towers + 1] = { x = math.sin(a0) * wr, z = math.cos(a0) * wr } end
    end
    for _, sr in ipairs(plan.streets) do
      if sr.main then
        plan.gates[#plan.gates + 1] = { x = math.sin(sr.heading) * wr, z = math.cos(sr.heading) * wr,
          heading = sr.heading }
      end
    end
    plan.wall_radius = wr
  end
  -- The citadel of a city stands on the highest ground near the centre.
  if plan.citadel and ctx.castle then
    local bx, bz, bh = 0, 0, -math.huge
    for k = 0, 11 do
      local a = k * math.pi / 6
      local x, z = math.sin(a) * radius * 0.6, math.cos(a) * radius * 0.6
      local h = ctx.height(x, z)
      if h > bh then bx, bz, bh = x, z, h end
    end
    local castle = ctx.castle.plan({ id = st.id .. "/citadel", seed = st.seed, buildings = 1, type = "citadel",
      culture = st.culture, prosperity = st.prosperity }, ctx, s, { x = bx, z = bz, rooms = 100 })
    plan.castle = castle
    for _, b in ipairs(castle.buildings) do add_building(plan, b) end
  end
  -- Land use round about.
  local crops
  if land.class == "desert" or st.type == "oasis_town" then crops = { "date palms", "barley" }
  elseif (land.rain or 500) > 1300 and (land.temp or 15) > 20 then crops = { "rice", "sugar cane" }
  elseif (land.temp or 15) < 6 then crops = { "barley", "rye" }
  else crops = { "wheat", "barley", "pomegranate orchards" } end
  local uses = { { "fields", 3 }, { "pasture", (land.class == "steppe" and 4 or 1) },
    { "orchards", ctx.family.id == "persianate" and 2 or 0.5 }, { "woodlot", land.class == "forest" and 2 or 0.3 },
    { "fishing grounds", (land.coast or land.river) and 2 or 0 }, { "quarry", (land.relief or 0) > 200 and 1 or 0 } }
  local sectors = 6
  for k = 1, sectors do
    local kind = s:weighted(uses)
    plan.fields[#plan.fields + 1] = { kind = kind, crop = kind == "fields" and s:pick(crops) or nil,
      from = (k - 1) / sectors * 2 * math.pi, to = k / sectors * 2 * math.pi, r0 = radius + 15,
      r1 = radius + 150 + s:float() * 250 }
  end
  return layout.finish(plan, st, ctx)
end

-- Steps on steep streets, and ruins.
function layout.finish(plan, st, ctx)
  for _, sr in ipairs(plan.streets) do
    if sr.main then
      local len = math.sqrt((sr.x1 - sr.x0) ^ 2 + (sr.z1 - sr.z0) ^ 2)
      local rise = math.abs(ctx.height(sr.x1, sr.z1) - ctx.height(sr.x0, sr.z0))
      sr.steps = rise / math.max(len, 1) > layout.STEEP
    end
  end
  if st.ruined then
    local age = st.ruin_age or 100
    for _, b in ipairs(plan.buildings) do
      b.ruin = { age = age, cause = st.fall_cause or "abandonment", seed = rng.hash(b.seed, "ruin") }
    end
    plan.ruined = true
  end
  plan.taken = nil
  return plan
end

return layout
