-- SPDX-License-Identifier: MIT
-- Places in play (concept 6.1, 6.5): the peopled world's settlements come
-- to be as the heirs approach. Within a few kilometres a settlement is laid
-- out and its buildings massed (their shells drawn from afar); within about
-- 100 m a building gets its floor plan and kit pieces; within about 30 m,
-- or when its door is opened, its furniture; a container's contents when it
-- is opened. Interiors are seamless: the walker stands on floors and stairs
-- and is stopped by walls and closed doors. Only changes are kept (taken
-- things, opened or broken doors, pieces burnt or smashed), with the save;
-- the host's changes are the truth in co-op and travel as these deltas.
local job = require("people.job")
local chronicle = require("people.chronicle")
local peopling = require("people.peopling")
local history = require("people.history")
local land = require("people.land")
local buildings = require("places.buildings")
local layout = require("places.layout")
local castles = require("places.castles")
local integrity = require("places.integrity")
local rng = require("people.rng")
local underground = require("places.underground")
local setpieces = require("places.setpieces")

local places = {}

places.NEAR = 3000     -- metres within which settlements are laid out
places.PLAN = 120      -- metres within which buildings get floor plans and pieces
places.FURNISH = 35    -- metres within which they are furnished
places.FORGET = 5000   -- metres beyond which laid-out settlements are dropped
places.SLICES = 6      -- generation slices per tick while the world is peopled
places.PER_TICK = 6    -- buildings planned per tick
places.REACH = 2.5     -- metres to a door or container to use it
places.STEP = 0.6      -- metres a walker steps up

function places.new(saved)
  saved = type(saved) == "table" and saved or {}
  local P = { plans = {}, deltas = {}, ready = false, under = underground.new() }
  for id, d in pairs(type(saved.deltas) == "table" and saved.deltas or {}) do
    if type(id) == "string" and type(d) == "table" then P.deltas[id] = d end
  end
  return P
end

function places.snapshot(P)
  return { deltas = P.deltas }
end

-- Begins peopling the world (in slices, see places.tick): the Settled world
-- (variant "settled") or Rise and fall ("rise"); a history kept with the save
-- (saved) is taken as it is.
function places.begin(P, seed, anchor, variant, saved)
  P.seed, P.variant = seed, variant or "settled"
  if saved then
    P.job = job.new(function() return saved, land.sample(seed) end)
  else
    P.job = job.new(chronicle.settled_world, seed, { anchor = anchor, variant = P.variant })
  end
  P.ready = false
end

-- Finishes peopling at once (scenarios and tests; not inside a budgeted call).
function places.finish(P)
  if P.job and not P.ready then
    job.run(P.job)
    P.db, P.grid, P.ready = P.job.result, P.job.extra, true
  end
  return P.db
end

local function delta(P, id)
  P.deltas[id] = P.deltas[id] or {}
  return P.deltas[id]
end

-- The context a settlement is laid out in.
function places.context(P, st)
  local cu = history.get(P.db, st.culture) or P.db.culture[1]
  local family = peopling.data().family[cu.family]
  local cell = P.grid.cells[st.cell] or land.cell_at(P.grid, st.pos)
  local roads = {}
  for _, r in ipairs(P.db.roads) do
    local other = (r.a == st.id and r.b) or (r.b == st.id and r.a)
    if other then
      local o = history.get(P.db, other)
      local x, z = places.local_xz(st.pos, o.pos)
      roads[#roads + 1] = math.atan(x, z)
    end
  end
  local t = peopling.data().type[st.type]
  return { family = family, land = { class = cell.class, temp = cell.temp, rain = cell.rain, coast = cell.coast,
    river = cell.river, relief = cell.relief },
    height = function(x, z) return world.height(world.move(st.pos, 0, z, x, 0)) end,
    water = function(x, z)
      local kind, surface = world.water(world.move(st.pos, 0, z, x, 0))
      return kind ~= nil and surface ~= nil and surface > world.height(world.move(st.pos, 0, z, x, 0)) + 0.3
    end,
    roads = roads, castle = castles, roles = t and t.roles or {} }
end

-- Metres east and north of a centre to a position (near: a flat frame).
function places.local_xz(centre, pos)
  local a, b = world.to_local(centre), world.to_local(pos)
  return b[1] - a[1], -(b[3] - a[3])
end

-- Lays out a settlement (once) and masses its buildings.
function places.plan(P, st)
  local plan = P.plans[st.id]
  if plan then return plan end
  st.seed = st.seed or rng.hash(P.db.seed, st.id)
  local ctx = places.context(P, st)
  plan = layout.plan(st, ctx)
  if st.story == "anarvan" then setpieces.anarvan(plan, st, ctx) end
  plan.settlement = st
  plan.family = ctx.family
  for _, b in ipairs(plan.buildings) do
    b.settlement = st.id
    buildings.generate(b, ctx.family, 1)
  end
  P.plans[st.id] = plan
  if plan.farrokh and not P.under.dungeons["sanctum:farrokh"] then -- the cellar under the loom
    places.realise(P, plan.farrokh, 2)
    local e, n = setpieces.hatch(plan.farrokh)
    local g = setpieces.sanctum(places.origin(P, plan.farrokh), e, n)
    g.closed = true -- (hidden until Farrokh opens it)
    P.under.dungeons[g.id] = g
  end
  return plan
end

-- Where a building stands: its origin (the floor's middle) and heading.
function places.origin(P, b)
  local o = b.origin
  if not o then
    local st = history.get(P.db, b.settlement)
    o = world.move(st.pos, 0, b.z, b.x, 0)
    o.alt = (b.ground or world.height(o)) + (b.stilts or 0)
    b.origin = o
  end
  return world.move(o, 0, 0, 0, 0), b.yaw
end

local function basis(yaw)
  return { math.cos(yaw), 0, math.sin(yaw) }, { math.sin(yaw), 0, -math.cos(yaw) }
end

-- A world position in a building's own frame (x right, y up, z front).
function places.to_building(P, b, pos)
  local o = places.origin(P, b)
  local a, c = world.to_local(o), world.to_local(pos)
  local d = { c[1] - a[1], c[2] - a[2], c[3] - a[3] }
  local right, front = basis(b.yaw)
  return d[1] * right[1] + d[3] * right[3], (pos.alt or 0) - o.alt, d[1] * front[1] + d[3] * front[3]
end

-- A point of a building's frame as a world position.
function places.to_world(P, b, x, y, z)
  local o = places.origin(P, b)
  local p = world.move(o, b.yaw, z, x, 0)
  p.alt = o.alt + y
  return p
end

-- Decay of a ruin: roofs and upper floors gone, walls fallen by its age,
-- the rest settled by integrity; overgrown, and buried in sand, silt or snow.
local function decay(b)
  local s = rng.new(b.ruin.seed, "decay")
  local share = math.min(0.6, b.ruin.age / 400)
  for _, p in ipairs(b.pieces) do
    if p.kind == "roof" or p.kind == "windcatcher" or (p.storey or 1) > 1 or p.kind == "door" or p.kind == "window" then
      p.gone = true
    elseif p.kind == "wall" and s:chance(share) then
      p.gone = true
    elseif p.kind == "furniture" and s:chance(0.7) then
      p.gone = true
    end
  end
  integrity.settle(b.pieces)
  local cls = (b.land or {}).class
  b.burial = (cls == "desert" and 0.8) or ((cls == "swamp") and 0.5) or ((cls == "tundra" or cls == "ice") and 0.6) or 0
  b.overgrown = cls ~= "desert" and cls ~= "ice"
end

-- Brings a building to a stage, applying what was changed in it.
function places.realise(P, b, stage)
  local plan = P.plans[b.settlement]
  if (b.stage or 1) < 2 and stage >= 2 then
    buildings.generate(b, plan.family, 2)
    if b.ruin then decay(b) end
    local d = P.deltas[b.id]
    if d then
      for _, i in ipairs(d.gone or {}) do
        if b.pieces[i] then b.pieces[i].gone = true end
      end
      for i, state in pairs(d.doors or {}) do
        for _, p in ipairs(b.pieces) do
          if p.kind == "door" and p.door == tonumber(i) then p.open = state == "open"; p.gone = state == "broken" end
        end
      end
      integrity.settle(b.pieces)
    end
  end
  if stage >= 3 and (b.stage or 1) < 3 then
    buildings.furnish(b)
    if b.ruin then
      local s = rng.new(b.ruin.seed, "loot")
      for _, p in ipairs(b.pieces) do
        if p.kind == "furniture" and s:chance(0.7) then p.gone = true end
      end
    end
  end
  return b
end

local function each_building(P, fn)
  for _, plan in pairs(P.plans) do
    for _, b in ipairs(plan.buildings) do
      if fn(b, plan) then return b end
    end
  end
end

-- Moves generation on and brings near places into being. viewers: the
-- positions of the heirs present.
function places.tick(P, viewers)
  if not P.ready then
    if not P.job then return end
    if job.run(P.job, places.SLICES) then
      P.db, P.grid, P.ready = P.job.result, P.job.extra, true
    end
    return
  end
  for _, v in ipairs(viewers) do
    for _, n in ipairs(peopling.near(P.db, P.grid, v, places.NEAR)) do
      places.plan(P, n.settlement)
    end
  end
  underground.tick(P.under, P.db, P.grid, viewers)
  for id, plan in pairs(P.plans) do -- forget far places (their changes are kept)
    local near = false
    for _, v in ipairs(viewers) do
      if world.distance(v, plan.settlement.pos) < places.FORGET then near = true end
    end
    if not near then P.plans[id] = nil end
  end
  local budget = places.PER_TICK
  each_building(P, function(b)
    local o = places.origin(P, b)
    local dist = math.huge
    for _, v in ipairs(viewers) do dist = math.min(dist, world.distance(v, o)) end
    local want = (dist < places.FURNISH and 3) or (dist < places.PLAN and 2) or 1
    if want > (b.stage or 1) then
      places.realise(P, b, want)
      budget = budget - 1
    end
    return budget <= 0
  end)
end

-- The building whose footprint holds a position (planned ones only).
function places.at(P, pos)
  local found, fx, fy, fz
  each_building(P, function(b)
    if (b.stage or 1) >= 2 then
      local x, y, z = places.to_building(P, b, pos)
      if math.abs(x) <= b.w / 2 + 0.3 and math.abs(z) <= b.d / 2 + 0.3 and y > -3 and y < b.storeys * 3 + 6 then
        found, fx, fy, fz = b, x, y, z
        return true
      end
    end
  end)
  return found, fx, fy, fz
end

-- The floor under a position no higher than a step above alt: the top of a
-- floor, stair, plinth or rubble in a building (world altitude), or nil;
-- under ground, the floor of a cave or vault, and true (it is the ground).
function places.floor(P, pos, alt)
  local under = underground.floor(P.under, pos, alt)
  if under then return under, true end
  local b, x, _, z = places.at(P, pos)
  if not b then return nil end
  local o = places.origin(P, b)
  local limit = alt - o.alt + places.STEP
  local best
  for _, p in ipairs(b.pieces) do
    if not p.gone and (p.kind == "floor" or p.kind == "stair" or p.kind == "plinth")
      and math.abs(x - p.x) <= p.w / 2 and math.abs(z - p.z) <= p.d / 2 then
      local top = p.y + p.h
      if p.kind == "stair" then top = p.y + p.h * math.max(0, math.min(1, (p.z + p.d / 2 - z) / p.d)) end
      if top <= limit and (not best or top > best) then best = top end
    end
  end
  return best and (o.alt + best) or nil
end

local BLOCKS = { wall = true, door = true, window = true, plinth = false, post = true, well = true, windcatcher = true }

-- Whether walking from a to b (feet at alt) runs into a wall, a closed door
-- or a window of a building.
function places.blocked(P, a, b, alt)
  if underground.blocked(P.under, a, b, alt) then return true end
  local found = false
  each_building(P, function(bd)
    if (bd.stage or 1) < 2 then return false end
    local ax, ay, az = places.to_building(P, bd, { face = a.face, x = a.x, y = a.y, lx = a.lx, ly = a.ly, alt = alt })
    local bx, _, bz = places.to_building(P, bd, b)
    if math.max(math.abs(ax), math.abs(bx)) > bd.w / 2 + 2 or math.max(math.abs(az), math.abs(bz)) > bd.d / 2 + 2 then
      return false
    end
    for _, p in ipairs(bd.pieces) do
      if BLOCKS[p.kind] and not p.gone and not (p.kind == "door" and p.open)
        and ay + 1.6 > p.y and ay + 0.3 < p.y + p.h then
        local x0, x1 = p.x - p.w / 2 - 0.2, p.x + p.w / 2 + 0.2
        local z0, z1 = p.z - p.d / 2 - 0.2, p.z + p.d / 2 + 0.2
        if bx >= x0 and bx <= x1 and bz >= z0 and bz <= z1 then found = true return true end
      end
    end
    return false
  end)
  return found
end

-- The door or container nearest in front of someone at pos facing heading
-- (within reach): returns the building, the piece and its kind.
function places.nearby(P, pos, heading)
  local hand = world.move(pos, heading, 0.8, 0, 0)
  hand.alt = pos.alt + 1
  local best, bp, bd
  each_building(P, function(b)
    if (b.stage or 1) < 2 then return false end
    local x, y, z = places.to_building(P, b, hand)
    if math.abs(x) > b.w / 2 + 3 or math.abs(z) > b.d / 2 + 3 then return false end
    for _, p in ipairs(b.pieces) do
      if not p.gone and (p.kind == "door" or p.container) then
        local dx, dz = x - p.x, z - p.z
        local dy = y - (p.y + math.min(p.h, 1.2))
        local d = math.sqrt(dx * dx + dz * dz + dy * dy * 0.25)
        if d <= places.REACH and (not bd or d < bd) then best, bp, bd = b, p, d end
      end
    end
    return false
  end)
  return best, bp, bp and (bp.kind == "door" and "door" or "container")
end

-- Opens or closes a door; returns whether it is open now.
function places.toggle_door(P, b, p)
  p.open = not p.open
  local d = delta(P, b.id)
  d.doors = d.doors or {}
  d.doors[tostring(p.door)] = p.open and "open" or "closed"
  if p.open and (b.stage or 1) < 3 then places.realise(P, b, 3) end -- opening the door furnishes the house
  return p.open
end

-- What a container holds now: its contents less what was taken.
function places.container(P, b, n)
  local d = P.deltas[b.id]
  local taken = d and d.taken and d.taken[tostring(n)] or {}
  local out = {}
  for _, it in ipairs(buildings.contents(b, n)) do
    local left = it[2] - (taken[it[1]] or 0)
    if left > 0 then out[#out + 1] = { it[1], left } end
  end
  return out
end

-- Takes things out of a container; returns how many were taken.
function places.take(P, b, n, item, count)
  local have = 0
  for _, it in ipairs(places.container(P, b, n)) do
    if it[1] == item then have = it[2] end
  end
  local got = math.min(have, count)
  if got > 0 then
    local d = delta(P, b.id)
    d.taken = d.taken or {}
    local t = d.taken[tostring(n)] or {}
    t[item] = (t[item] or 0) + got
    d.taken[tostring(n)] = t
  end
  return got
end

-- Smashes or burns a piece of a building; what it held up falls. Returns
-- the pieces lost.
function places.damage(P, b, i)
  local lost = integrity.remove(b.pieces, i)
  local d = delta(P, b.id)
  d.gone = d.gone or {}
  local have = {}
  for _, k in ipairs(d.gone) do have[k] = true end
  for _, k in ipairs(lost) do
    if not have[k] then d.gone[#d.gone + 1] = k end
  end
  return lost
end

-- Fire eats timber: burning cells under a building's timber pieces burn
-- them away (a few pieces looked at per call).
function places.burn(P, b)
  if (b.stage or 1) < 2 then return {} end
  local lost = {}
  b.burn_at = (b.burn_at or 0)
  b.burn_left = math.max(0, (b.burn_left or 0) - 1)
  for _ = 1, 8 do
    b.burn_at = b.burn_at % #b.pieces + 1
    local p = b.pieces[b.burn_at]
    if not p.gone and (p.mat == "timber" or p.mat == "thatch" or p.mat == "felt" or p.mat == "bamboo") then
      local cell = sim.fire_at(places.to_world(P, b, p.x, p.y, p.z))
      if cell.loaded and cell.burning then
        b.burn_left = 300 -- (ticks it is known to burn)
        for _, k in ipairs(places.damage(P, b, b.burn_at)) do lost[#lost + 1] = k end
      end
    end
  end
  b.burning = b.burn_left > 0
  return lost
end

-- A settlement by name (the nearest of that name), or the nearest of a type.
function places.find(P, pos, what)
  local best, bd
  for _, st in ipairs(P.db.settlement) do
    if st.name == what or st.type == what then
      local d = world.distance(pos, st.pos)
      if not bd or d < bd then best, bd = st, d end
    end
  end
  return best, bd
end

-- How many rooms a laid-out settlement's buildings (and cellars) hold.
function places.rooms(plan)
  local n = 0
  for _, b in ipairs(plan.buildings) do
    n = n + #(b.program or buildings.program(b.type, b.household or {}, rng.new(b.seed, "program")))
  end
  return n + (plan.castle and #plan.castle.cellars or 0)
end

return places
