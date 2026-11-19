-- SPDX-License-Identifier: MIT
-- Buildings, generated in layers (concept 6.5), each step deterministic from
-- the building's seed:
--  1. program: the type's rooms (data/buildings.lua) for this household,
--  2. massing: footprint, storeys, roof, courtyard, iwan, windcatcher and
--     wall material from the people's style family and the land,
--  3. floor plan: rooms by constrained subdivision of each storey (a ring
--     round a courtyard, or two rows along a corridor in big buildings),
--     doors along the room graph from the entrance, windows on outer walls,
--     stairs between storeys,
--  4. construction: modular kit pieces on a 0.5 m grid (wall segments,
--     floors, lintels, roofs, doors, shutters, stairs, plinth or stilts),
--     each of a material, so fire, force and decay act on single pieces,
--  5. furnishing: pieces against walls with clear floor before them, in the
--     middle with room around, in groups, with lights and containers,
--     matched to the household,
--  6. contents: what containers hold, made when they are opened.
-- Coordinates are the building's own: x to the right, y up, z towards the
-- front (the street), the origin at the middle of the footprint at floor
-- level.
local rng = require("people.rng")

local buildings = {}

buildings.GRID = 0.5     -- metres
buildings.STOREY = 3.0   -- metres from floor to floor
buildings.WALL = 0.4     -- wall thickness
buildings.SEGMENT = 2.0  -- metres: the longest wall piece
buildings.DOOR = { 1.0, 2.1 }
buildings.WINDOW = { 0.8, 0.9, 1.0 } -- width, sill height, height

local data
function buildings.data()
  if not data then
    local text = assert(vfs.read("data/buildings.lua"))
    data = assert(load(text, "=data/buildings.lua", "t", {}))()
  end
  return data
end

local function snap(v) return math.floor(v / buildings.GRID + 0.5) * buildings.GRID end

-- 1. Program: the rooms this building will have.
function buildings.program(btype, household, s)
  local t = assert(buildings.data().types[btype], "unknown building type " .. tostring(btype))
  local rooms = {}
  for _, r in ipairs(t.rooms) do
    local take = r.required
    if not take and r.trade then take = household.trade ~= nil end
    if not take and r.wealth then take = (household.wealth or 0) >= r.wealth end
    if not take and not r.trade and not r.wealth then take = s:chance(0.5) end
    if take then
      local kind = r.id
      if r.trade and household.trade == "weaver" then kind = "loomroom" end
      rooms[#rooms + 1] = { kind = kind, share = r.share, off = r.off, upstairs = r.upstairs }
    end
  end
  for _ = 1, t.guests or 0 do -- caravanserais: a ring of guest rooms round the court
    rooms[#rooms + 1] = { kind = "guest", share = 1, off = "gatehall" }
  end
  return rooms
end

local WARM_ROOFS = { flat = true, dome = true, vault = true }
local WET_ROOFS = { gable = true, thatch = true, tile = true, pavilion = true }

-- 2. Massing. b: { type, seed, style (style family), household, land
-- (class, temp, rain), plot = { w, d } }. Fills in the building's form.
function buildings.mass(b, family)
  local t = buildings.data().types[b.type]
  local s = rng.new(b.seed, "mass")
  local land = b.land or {}
  local lo, hi = t.size[1], t.size[2]
  local wealth = b.household and b.household.wealth or 0.5
  b.w = snap(math.min(b.plot and b.plot.w or hi, lo + (hi - lo) * (0.4 + wealth * 0.6 * s:float())))
  b.d = snap(math.min(b.plot and b.plot.d or hi, lo + (hi - lo) * (0.3 + 0.7 * s:float())))
  if t.round then b.d = b.w end
  local st = family.storeys
  b.storeys = st[1] + math.floor((st[2] - st[1] + 1) * wealth * s:float())
  if b.type == "tower" then b.storeys = 3 + s:int(0, 1) end
  if t.open or b.type == "tent" or b.type == "shrine" then b.storeys = 1 end
  local program_up = false
  for _, r in ipairs(b.program or {}) do program_up = program_up or r.upstairs end
  if program_up and b.storeys < 2 then b.storeys = 2 end
  -- Roof by family and climate.
  local roofs = {}
  for _, r in ipairs(family.roofs) do
    local wgt = 1
    if (land.rain or 500) > 900 and WET_ROOFS[r] then wgt = 3 end
    if (land.rain or 500) < 400 and WARM_ROOFS[r] then wgt = 3 end
    roofs[#roofs + 1] = { r, wgt }
  end
  b.roof = s:weighted(roofs)
  if b.type == "tent" then b.roof = "yurt" end
  if t.round and b.type ~= "tent" then b.roof = "dome" end
  -- Walls by family and the land at hand.
  local walls = {}
  for _, m in ipairs(family.walls) do
    local wgt = 1
    if land.class == "forest" and (m == "timber" or m == "bamboo") then wgt = 4 end
    if land.class == "mountain" and m == "stone" then wgt = 4 end
    if (land.class == "desert" or land.class == "steppe") and (m == "mudbrick" or m == "rammed_earth") then wgt = 4 end
    walls[#walls + 1] = { m, wgt }
  end
  b.material = s:weighted(walls)
  if b.type == "tent" then b.material = "felt" end
  b.courtyard = t.courtyard or (b.w >= 12 and b.d >= 12 and s:chance(0.5) and family.features[3] == "courtyard")
  local has = {}
  for _, f in ipairs(family.features) do has[f] = true end
  b.iwan = t.iwan or (has.iwan and b.courtyard and s:chance(0.7)) or false
  b.windcatcher = has.windcatcher and (land.temp or 15) > 22 and b.storeys >= 1 and s:chance(0.5) or false
  return b
end

local function split(rect, rooms, horizontal_first)
  -- Guillotine subdivision of rect {x0, z0, x1, z1} among rooms by share.
  if #rooms == 1 then
    rooms[1].x0, rooms[1].z0, rooms[1].x1, rooms[1].z1 = rect[1], rect[2], rect[3], rect[4]
    return
  end
  local total = 0
  for _, r in ipairs(rooms) do total = total + r.share end
  local half, acc, k = total / 2, 0, 1
  for i, r in ipairs(rooms) do
    acc = acc + r.share
    k = i
    if acc >= half then break end
  end
  k = math.min(k, #rooms - 1)
  local a, b = {}, {}
  local sa = 0
  for i, r in ipairs(rooms) do
    if i <= k then a[#a + 1] = r; sa = sa + r.share else b[#b + 1] = r end
  end
  local w, d = rect[3] - rect[1], rect[4] - rect[2]
  local along_x = w > d or (w == d and horizontal_first)
  if along_x then
    local cut = snap(rect[1] + w * sa / total)
    cut = math.max(rect[1] + 1.5, math.min(rect[3] - 1.5, cut))
    split({ rect[1], rect[2], cut, rect[4] }, a, not horizontal_first)
    split({ cut, rect[2], rect[3], rect[4] }, b, not horizontal_first)
  else
    local cut = snap(rect[2] + d * sa / total)
    cut = math.max(rect[2] + 1.5, math.min(rect[4] - 1.5, cut))
    split({ rect[1], rect[2], rect[3], cut }, a, not horizontal_first)
    split({ rect[1], cut, rect[3], rect[4] }, b, not horizontal_first)
  end
end

-- The length two rooms' rectangles share along a wall (and where).
local function shared(a, b)
  local eps = 0.01
  if math.abs(a.x1 - b.x0) < eps or math.abs(b.x1 - a.x0) < eps then
    local x = math.abs(a.x1 - b.x0) < eps and a.x1 or a.x0
    local lo, hi = math.max(a.z0, b.z0), math.min(a.z1, b.z1)
    if hi - lo > eps then return hi - lo, "x", x, lo, hi end
  end
  if math.abs(a.z1 - b.z0) < eps or math.abs(b.z1 - a.z0) < eps then
    local z = math.abs(a.z1 - b.z0) < eps and a.z1 or a.z0
    local lo, hi = math.max(a.x0, b.x0), math.min(a.x1, b.x1)
    if hi - lo > eps then return hi - lo, "z", z, lo, hi end
  end
  return 0
end
buildings.shared = shared

-- 3. Floor plan: rooms on each storey, doors, windows and stairs.
function buildings.plan(b)
  local s = rng.new(b.seed, "plan")
  local hw, hd = b.w / 2, b.d / 2
  local rooms, by_storey = {}, {}
  for k = 1, b.storeys do by_storey[k] = {} end
  for _, r in ipairs(b.program) do
    local storey = r.upstairs and math.min(2, b.storeys) or 1
    table.insert(by_storey[storey], { kind = r.kind, share = r.share, off = r.off, storey = storey })
  end
  for k = 3, b.storeys do -- towers: the upper rooms spread over the storeys
    if #by_storey[k] == 0 and #by_storey[2] > 1 then
      local r = table.remove(by_storey[2])
      r.storey = k
      table.insert(by_storey[k], r)
    end
  end
  for k = 1, b.storeys do
    if #by_storey[k] == 0 then
      table.insert(by_storey[k], { kind = k == 1 and "hall" or "store", share = 1, storey = k })
    end
  end
  b.court = nil
  for k = 1, b.storeys do
    local list = by_storey[k]
    for i, r in ipairs(list) do r.share = r.share + i * 1e-6 end
    if b.courtyard and k == 1 then
      local cw, cd = snap(b.w * 0.5), snap(b.d * 0.45)
      b.court = { x0 = -cw / 2, z0 = -cd / 2, x1 = cw / 2, z1 = cd / 2, kind = "courtyard", storey = 1 }
      -- A ring: the front band (the entrance), back, left and right.
      local bands = {
        { -hw, cd / 2, hw, hd }, { -hw, -hd, hw, -cd / 2 }, { -hw, -cd / 2, -cw / 2, cd / 2 },
        { cw / 2, -cd / 2, hw, cd / 2 },
      }
      local per = {}
      for i = 1, 4 do per[i] = {} end
      for i, r in ipairs(list) do table.insert(per[(i - 1) % 4 + 1], r) end
      for i = 1, 4 do
        if #per[i] > 0 then split(bands[i], per[i], i <= 2) end
      end
    elseif #list > 4 and k == 1 then
      local cz = 1.5
      b.corridor = { x0 = -hw, z0 = -cz / 2, x1 = hw, z1 = cz / 2, kind = "corridor", storey = 1 }
      if b.d < b.w then
        local front, back = {}, {}
        for i, r in ipairs(list) do table.insert(i % 2 == 1 and front or back, r) end
        split({ -hw, cz / 2, hw, hd }, front, true)
        split({ -hw, -hd, hw, -cz / 2 }, back, true)
      else
        b.corridor = { x0 = -0.75, z0 = -hd, x1 = 0.75, z1 = hd, kind = "corridor", storey = 1 }
        local left, right = {}, {}
        for i, r in ipairs(list) do table.insert(i % 2 == 1 and left or right, r) end
        split({ -hw, -hd, -0.75, hd }, left, false)
        split({ 0.75, -hd, hw, hd }, right, false)
      end
    else
      split({ -hw, -hd, hw, hd }, list, s:chance(0.5))
    end
    for _, r in ipairs(list) do
      rooms[#rooms + 1] = r
      r.index = #rooms
      r.doors, r.windows = {}, {}
    end
  end
  -- Doors: from the entrance through the room graph, each room opening off
  -- the room it wants where they meet.
  local spaces = {}
  for _, r in ipairs(rooms) do spaces[#spaces + 1] = r end
  if b.court then b.court.doors, b.court.windows = {}, {}; spaces[#spaces + 1] = b.court end
  if b.corridor then b.corridor.doors, b.corridor.windows = {}, {}; spaces[#spaces + 1] = b.corridor end
  local entry
  if b.court then entry = b.court elseif b.corridor then entry = b.corridor end
  local front_rooms = {}
  for _, r in ipairs(rooms) do
    if r.storey == 1 and math.abs(r.z1 - hd) < 0.01 then front_rooms[#front_rooms + 1] = r end
  end
  table.sort(front_rooms, function(a, c) return math.abs((a.x0 + a.x1) / 2) < math.abs((c.x0 + c.x1) / 2) end)
  local gate = front_rooms[1]
  b.doors = {}
  local function add_door(a, c, axis, at, lo, hi, kind)
    local mid = snap((lo + hi) / 2)
    local door = { a = a, c = c, axis = axis, at = at, pos = mid, storey = a and a.storey or 1, kind = kind or "door" }
    b.doors[#b.doors + 1] = door
    if a then table.insert(a.doors, #b.doors) end
    if c then table.insert(c.doors, #b.doors) end
    return door
  end
  -- The front door: through the front room nearest the middle (or into the court).
  if gate then
    add_door(gate, nil, "z", hd, gate.x0, gate.x1, "front")
  elseif entry then
    add_door(entry, nil, "z", hd, entry.x0, entry.x1, "front")
  end
  local connected = { [gate or entry] = true }
  if gate and entry and shared(gate, entry) > 0 then
    local _, axis, at, lo, hi = shared(gate, entry)
    add_door(gate, entry, axis, at, lo, hi)
    connected[entry] = true
  end
  for _ = 1, #spaces do
    for _, r in ipairs(spaces) do
      if not connected[r] and r.storey == 1 then
        local best, blen
        for _, o in ipairs(spaces) do
          if connected[o] and o.storey == 1 then
            local len = shared(r, o)
            if len >= 1.2 then
              local want = (r.off == o.kind) and 100 or 0
              if o == b.court or o == b.corridor then want = want + 50 end
              if not best or len + want > blen then best, blen = o, len + want end
            end
          end
        end
        if best then
          local _, axis, at, lo, hi = shared(r, best)
          add_door(r, best, axis, at, lo, hi)
          connected[r] = true
        end
      end
    end
  end
  -- Upstairs: stairs from the ground floor's first room; rooms above open
  -- off the stair room.
  b.stairs = {}
  if b.storeys > 1 then
    local base = gate or rooms[1]
    for k = 2, b.storeys do
      local above
      for _, r in ipairs(rooms) do
        if r.storey == k and r.x0 <= base.x0 + 1 and r.x1 >= base.x0 + 1 and r.z0 <= base.z0 + 1 then above = r end
      end
      above = above or by_storey[k][1]
      b.stairs[#b.stairs + 1] = { storey = k - 1, x = base.x0 + 0.5 + buildings.WALL, z = base.z0 + buildings.WALL,
        w = 1.0, d = 2.5, room = base.index, to = above.index }
      connected[above] = true
      for _ = 1, #rooms do
        for _, r in ipairs(rooms) do
          if r.storey == k and not connected[r] then
            for _, o in ipairs(rooms) do
              if o.storey == k and connected[o] and shared(r, o) >= 1.2 and not connected[r] then
                local _, axis, at, lo, hi = shared(r, o)
                add_door(r, o, axis, at, lo, hi)
                connected[r] = true
              end
            end
          end
        end
      end
      base = above
    end
  end
  -- Windows on outer walls: few in hot or cold lands, more in mild ones.
  local land = b.land or {}
  local per_room = ((land.temp or 15) > 24 or (land.temp or 15) < 2) and 1 or 2
  for _, r in ipairs(rooms) do
    local sides = {}
    if math.abs(r.x0 + hw) < 0.01 then sides[#sides + 1] = { "x", -hw, r.z0, r.z1 } end
    if math.abs(r.x1 - hw) < 0.01 then sides[#sides + 1] = { "x", hw, r.z0, r.z1 } end
    if math.abs(r.z0 + hd) < 0.01 then sides[#sides + 1] = { "z", -hd, r.x0, r.x1 } end
    if math.abs(r.z1 - hd) < 0.01 and r ~= gate then sides[#sides + 1] = { "z", hd, r.x0, r.x1 } end
    for i = 1, math.min(per_room, #sides) do
      local sd = sides[i]
      if sd[4] - sd[3] >= 2 and r.kind ~= "store" and r.kind ~= "cistern" then
        r.windows[#r.windows + 1] = { axis = sd[1], at = sd[2], pos = snap((sd[3] + sd[4]) / 2), storey = r.storey }
      end
    end
  end
  b.rooms = rooms
  return b
end

local function piece(b, p)
  b.pieces[#b.pieces + 1] = p
  p.index = #b.pieces
  return p
end

-- A wall along an axis line from lo to hi at a storey, broken by the doors
-- and windows that lie on it, built of segments no longer than SEGMENT.
local function wall_line(b, axis, at, lo, hi, storey, openings, mat, outer)
  local H, T = buildings.STOREY, buildings.WALL
  local y0 = (storey - 1) * H
  table.sort(openings, function(a, c) return a.pos < c.pos end)
  local cuts = {}
  for _, o in ipairs(openings) do
    local half = (o.kind == "window" and buildings.WINDOW[1] or buildings.DOOR[1]) / 2
    cuts[#cuts + 1] = { o.pos - half, o.pos + half, o }
  end
  local function seg(a, c, y, h, kind, extra)
    local x, z, w, d
    if axis == "x" then x, z, w, d = at, (a + c) / 2, T, c - a else x, z, w, d = (a + c) / 2, at, c - a, T end
    local p = piece(b, { kind = kind or "wall", mat = mat, x = x, y = y, z = z, w = w, h = h, d = d, storey = storey,
      outer = outer })
    for k, v in pairs(extra or {}) do p[k] = v end
    return p
  end
  local function solid(a, c)
    local n = math.max(1, math.ceil((c - a) / buildings.SEGMENT - 1e-9))
    for i = 0, n - 1 do
      local s0, s1 = a + (c - a) * i / n, a + (c - a) * (i + 1) / n
      if s1 - s0 > 0.05 then seg(s0, s1, y0, H) end
    end
  end
  local x = lo
  for _, cut in ipairs(cuts) do
    local a, c, o = math.max(lo, cut[1]), math.min(hi, cut[2]), cut[3]
    if a > x then solid(x, a) end
    if o.kind == "window" then
      local _, sill, wh = buildings.WINDOW[1], buildings.WINDOW[2], buildings.WINDOW[3]
      seg(a, c, y0, sill)
      seg(a, c, y0 + sill + wh, H - sill - wh)
      seg(a, c, y0 + sill, wh, "window", { mat = "timber" })
    else
      local dh = buildings.DOOR[2]
      seg(a, c, y0 + dh, H - dh)
      local door = seg(a, c, y0, dh, "door", { mat = "timber", door = o.id, open = false })
      door.w, door.d = axis == "x" and 0.08 or door.w, axis == "x" and door.d or 0.08
    end
    x = c
  end
  if hi > x then solid(x, hi) end
end

-- Open structures: a well head or a jetty, no rooms.
local function construct_open(b)
  if b.type == "well" then
    piece(b, { kind = "well", mat = "stone", x = 0, y = 0, z = 0, w = 1.6, h = 0.8, d = 1.6 })
    piece(b, { kind = "post", mat = "timber", x = -0.7, y = 0.8, z = 0, w = 0.15, h = 1.4, d = 0.15 })
    piece(b, { kind = "post", mat = "timber", x = 0.7, y = 0.8, z = 0, w = 0.15, h = 1.4, d = 0.15 })
  else
    piece(b, { kind = "floor", mat = "timber", x = 0, y = -0.2, z = 0, w = b.w, h = 0.2, d = b.d })
    for _, cz in ipairs({ -b.d / 2 + 0.3, 0, b.d / 2 - 0.3 }) do
      for _, cx in ipairs({ -b.w / 2 + 0.3, b.w / 2 - 0.3 }) do
        piece(b, { kind = "post", mat = "timber", x = cx, y = -2, z = cz, w = 0.25, h = 1.8, d = 0.25 })
      end
    end
  end
  b.rooms, b.doors, b.stairs = {}, {}, {}
  b.stage = math.max(b.stage or 0, 2)
  return b
end

-- 4. Construction: the kit pieces.
function buildings.construct(b)
  b.pieces = {}
  b.iwan_done = nil
  if buildings.data().types[b.type].open then return construct_open(b) end
  local H = buildings.STOREY
  local hw, hd = b.w / 2, b.d / 2
  local mat = b.material
  -- Plinth or stilts under the floor.
  if (b.stilts or 0) > 0 then
    for _, cx in ipairs({ -hw + 0.2, 0, hw - 0.2 }) do
      for _, cz in ipairs({ -hd + 0.2, hd - 0.2 }) do
        piece(b, { kind = "post", mat = "timber", x = cx, y = -b.stilts, z = cz, w = 0.3, h = b.stilts, d = 0.3 })
      end
    end
  elseif (b.plinth or 0) > 0.05 then
    piece(b, { kind = "plinth", mat = "stone", x = 0, y = -b.plinth, z = 0, w = b.w + 0.4, h = b.plinth,
      d = b.d + 0.4 })
  end
  for i, d in ipairs(b.doors) do d.id = i end
  -- Floors of rooms, the courtyard paving and the corridor.
  local function floor(r, y, m)
    for _, st in ipairs(b.stairs) do
      if st.storey + 1 == r.storey and st.x >= r.x0 and st.x <= r.x1 and st.z >= r.z0 and st.z <= r.z1 then
        -- leave the stairwell open: the floor around it in two parts
        local cut = st.z + st.d
        if cut < r.z1 then
          piece(b, { kind = "floor", mat = m, x = (r.x0 + r.x1) / 2, y = y - 0.2, z = (cut + r.z1) / 2, w = r.x1 - r.x0,
            h = 0.2, d = r.z1 - cut, room = r.index, storey = r.storey })
        end
        local x1, x2 = st.x - st.w / 2, st.x + st.w / 2
        if x1 > r.x0 then
          piece(b, { kind = "floor", mat = m, x = (r.x0 + x1) / 2, y = y - 0.2, z = (r.z0 + cut) / 2, w = x1 - r.x0,
            h = 0.2, d = cut - r.z0, room = r.index, storey = r.storey })
        end
        if x2 < r.x1 then
          piece(b, { kind = "floor", mat = m, x = (x2 + r.x1) / 2, y = y - 0.2, z = (r.z0 + cut) / 2, w = r.x1 - x2,
            h = 0.2, d = cut - r.z0, room = r.index, storey = r.storey })
        end
        return
      end
    end
    piece(b, { kind = "floor", mat = m, x = (r.x0 + r.x1) / 2, y = y - 0.2, z = (r.z0 + r.z1) / 2, w = r.x1 - r.x0,
      h = 0.2, d = r.z1 - r.z0, room = r.index, storey = r.storey })
  end
  for _, r in ipairs(b.rooms) do floor(r, (r.storey - 1) * H, r.storey == 1 and "tile" or "timber") end
  if b.court then floor(b.court, 0, "stone") end
  if b.corridor then floor(b.corridor, 0, "tile") end
  -- Walls: the outer walls of each storey, then inner walls once per shared edge.
  for k = 1, b.storeys do
    local function openings(axis, at)
      local list = {}
      for _, d in ipairs(b.doors) do
        if d.storey == k and d.axis == axis and math.abs(d.at - at) < 0.01 then list[#list + 1] = d end
      end
      for _, r in ipairs(b.rooms) do
        for _, w in ipairs(r.windows) do
          if w.storey == k and w.axis == axis and math.abs(w.at - at) < 0.01 then
            list[#list + 1] = { kind = "window", pos = w.pos }
          end
        end
      end
      return list
    end
    wall_line(b, "z", hd, -hw, hw, k, openings("z", hd), mat, true)
    wall_line(b, "z", -hd, -hw, hw, k, openings("z", -hd), mat, true)
    wall_line(b, "x", -hw, -hd + buildings.WALL / 2, hd - buildings.WALL / 2, k, openings("x", -hw), mat, true)
    wall_line(b, "x", hw, -hd + buildings.WALL / 2, hd - buildings.WALL / 2, k, openings("x", hw), mat, true)
    local spaces = {}
    for _, r in ipairs(b.rooms) do if r.storey == k then spaces[#spaces + 1] = r end end
    if k == 1 and b.court then spaces[#spaces + 1] = b.court end
    if k == 1 and b.corridor then spaces[#spaces + 1] = b.corridor end
    for i = 1, #spaces do
      for j = i + 1, #spaces do
        local len, axis, at, lo, hi = shared(spaces[i], spaces[j])
        local open_side = b.iwan and (spaces[j] == b.court or spaces[i] == b.court)
          and (spaces[i].kind == "hall" or spaces[j].kind == "hall")
        if len > 0.05 and not (open_side and not b.iwan_done) then
          local list = {}
          for _, d in ipairs(b.doors) do
            if d.storey == k and d.axis == axis and math.abs(d.at - at) < 0.01 and d.pos >= lo and d.pos <= hi then
              list[#list + 1] = d
            end
          end
          local imat = (spaces[i] == b.court or spaces[j] == b.court) and mat or "plaster"
          wall_line(b, axis, at, lo, hi, k, list, imat, false)
        elseif open_side then
          b.iwan_done = true -- the iwan: a vaulted hall open to the court
          piece(b, { kind = "arch", mat = mat, x = axis == "x" and at or (lo + hi) / 2, y = H - 0.6,
            z = axis == "z" and at or (lo + hi) / 2, w = axis == "x" and buildings.WALL or (hi - lo), h = 0.6,
            d = axis == "x" and (hi - lo) or buildings.WALL, storey = 1 })
        end
      end
    end
  end
  -- Stairs.
  for _, st in ipairs(b.stairs) do
    piece(b, { kind = "stair", mat = "timber", x = st.x, y = (st.storey - 1) * H, z = st.z + st.d / 2, w = st.w,
      h = H, d = st.d, storey = st.storey })
  end
  -- Roofs.
  local top = b.storeys * H
  local function roof_over(r)
    local w, d = r.x1 - r.x0 + 0.4, r.z1 - r.z0 + 0.4
    local x, z = (r.x0 + r.x1) / 2, (r.z0 + r.z1) / 2
    local kind = b.roof
    if kind == "dome" then
      piece(b, { kind = "roof", mat = "plaster", x = x, y = top, z = z, w = w, h = 0.25, d = d, shape = "flat" })
      local dd = math.min(w, d) * 0.8
      piece(b, { kind = "roof", mat = "tile", x = x, y = top + 0.25, z = z, w = dd, h = dd / 2, d = dd,
        shape = "dome" })
    elseif kind == "flat" then
      piece(b, { kind = "roof", mat = mat == "felt" and "felt" or "plaster", x = x, y = top, z = z, w = w, h = 0.3,
        d = d,
        shape = "flat" })
    else
      local m = ({ thatch = "thatch", tile = "tile", gable = "tile", vault = mat, pavilion = "tile",
        yurt = "felt" })[kind]
      piece(b, { kind = "roof", mat = m or "tile", x = x, y = top, z = z, w = w, h = math.min(w, d) * 0.45, d = d,
        shape = kind == "vault" and "vault" or (kind == "yurt" and "cone" or "gable") })
    end
  end
  if b.court then
    local hw2, hd2 = hw, hd
    local c = b.court
    for _, band in ipairs({ { -hw2, c.z1, hw2, hd2 }, { -hw2, -hd2, hw2, c.z0 }, { -hw2, c.z0, c.x0, c.z1 },
      { c.x1, c.z0, hw2, c.z1 } }) do
      roof_over({ x0 = band[1], z0 = band[2], x1 = band[3], z1 = band[4] })
    end
  else
    roof_over({ x0 = -hw, z0 = -hd, x1 = hw, z1 = hd })
  end
  if b.windcatcher then
    piece(b, { kind = "windcatcher", mat = mat, x = hw - 1, y = top, z = -hd + 1, w = 1.5, h = 3, d = 1.5 })
  end
  b.stage = math.max(b.stage or 0, 2)
  return b
end

local function overlaps(a, list)
  for _, o in ipairs(list) do
    if a.x0 < o.x1 and o.x0 < a.x1 and a.z0 < o.z1 and o.z0 < a.z1 then return true end
  end
  return false
end

-- 5. Furnishing, matched to the household.
function buildings.furnish(b)
  if (b.stage or 0) >= 3 then return b end
  local d = buildings.data()
  local s = rng.new(b.seed, "furnish")
  local H = buildings.STOREY
  b.containers = {}
  local hh = b.household or {}
  for _, r in ipairs(b.rooms) do
    local rule = d.rooms[r.kind]
    if rule then
      local taken = {}
      local function blocked(rect)
        if overlaps(rect, taken) then return true end
        for _, di in ipairs(r.doors) do -- clear floor before doors
          local door = b.doors[di]
          local dx = door.axis == "x" and door.at or door.pos
          local dz = door.axis == "z" and door.at or door.pos
          if rect.x0 < dx + 1 and dx - 1 < rect.x1 and rect.z0 < dz + 1 and dz - 1 < rect.z1 then return true end
        end
        for _, st in ipairs(b.stairs) do -- the stairs and the stairwell above them
          if (st.storey == r.storey or st.storey + 1 == r.storey) and rect.x0 < st.x + st.w and st.x - st.w < rect.x1
            and rect.z0 < st.z + st.d + 0.5 and st.z - 0.5 < rect.z1 then return true end
        end
        return false
      end
      local function place(id, x, z, yaw, clearance)
        local f = d.furniture[id]
        local w, dd = f[1], f[2]
        if yaw % 180 ~= 0 then w, dd = dd, w end
        local rect = { x0 = x - w / 2, z0 = z - dd / 2, x1 = x + w / 2, z1 = z + dd / 2 }
        local need = { x0 = rect.x0, z0 = rect.z0, x1 = rect.x1, z1 = rect.z1 }
        if clearance then -- clear floor before it
          if yaw == 0 then need.z1 = need.z1 + clearance elseif yaw == 180 then need.z0 = need.z0 - clearance
          elseif yaw == 90 then need.x1 = need.x1 + clearance else need.x0 = need.x0 - clearance end
        end
        local m = buildings.WALL / 2
        if rect.x0 < r.x0 + m - 1e-6 or rect.x1 > r.x1 - m + 1e-6 or rect.z0 < r.z0 + m - 1e-6
          or rect.z1 > r.z1 - m + 1e-6 then return nil end
        if blocked(need) then return nil end
        taken[#taken + 1] = rect
        local p = piece(b, { kind = "furniture", item = id, mat = f[4], x = x, y = (r.storey - 1) * H, z = z, w = w,
          h = f[3], d = dd, yaw = yaw, room = r.index, storey = r.storey })
        return p
      end
      local function along_walls(id)
        local f = d.furniture[id]
        local m = buildings.WALL / 2
        local tries = {}
        for t = 0, 7 do
          local u = (t + 0.5) / 8
          tries[#tries + 1] = { r.x0 + m + f[2] / 2, r.z0 + (r.z1 - r.z0) * u, 90 }
          tries[#tries + 1] = { r.x1 - m - f[2] / 2, r.z0 + (r.z1 - r.z0) * u, 270 }
          tries[#tries + 1] = { r.x0 + (r.x1 - r.x0) * u, r.z0 + m + f[2] / 2, 0 }
          tries[#tries + 1] = { r.x0 + (r.x1 - r.x0) * u, r.z1 - m - f[2] / 2, 180 }
        end
        local start = s:int(0, #tries - 1)
        for k = 1, #tries do
          local t = tries[(start + k - 1) % #tries + 1]
          local p = place(id, snap(t[1] * 2) / 2, snap(t[2] * 2) / 2, t[3], 0.8)
          if p then return p end
        end
      end
      local function middle(id)
        local cx, cz = (r.x0 + r.x1) / 2, (r.z0 + r.z1) / 2
        for k = 0, 6 do
          local p = place(id, cx + (k % 3 - 1) * 1.2, cz + (k // 3 - 1) * 1.2, 0, nil)
          if p then return p end
        end
      end
      local wall = { table.unpack(rule.wall or {}) }
      if r.kind == "workshop" and hh.trade == "smith" then wall = { "forge", "anvil", "rack" } end
      if r.kind == "workshop" and hh.trade == "scholar" then wall = { "bookcase", "reading_desk" } end
      local placed = {}
      for _, id in ipairs(wall) do
        local p = along_walls(id)
        if p then placed[id] = p end
      end
      for _, g in ipairs(rule.groups or {}) do -- the second of a group beside the first
        local first = placed[g[1]]
        if first then
          for _, off in ipairs({ { 1.1, 0 }, { -1.1, 0 }, { 0, 1.1 }, { 0, -1.1 } }) do
            local p = place(g[2], first.x + off[1], first.z + off[2], first.yaw, nil)
            if p then placed[g[2]] = p break end
          end
        end
      end
      local mids = { table.unpack(rule.middle or {}) }
      if (hh.wealth or 0) > 0.6 then mids[#mids + 1] = "cushion" end
      for _, id in ipairs(mids) do middle(id) end
      if rule.light == "brazier" then
        local p = middle("brazier")
        if p then p.light = true end
      elseif rule.light then -- a lamp stands on a shelf, table or chest, or else by a wall
        local on
        for _, p in ipairs(b.pieces) do
          if p.kind == "furniture" and p.room == r.index and not on and (p.item == "shelf" or p.item == "low_table"
            or p.item == "chest" or p.item == "counter" or p.item == "reading_desk" or p.item == "workbench") then
            on = p
          end
        end
        local f = d.furniture.oil_lamp
        if on then
          piece(b, { kind = "furniture", item = "oil_lamp", mat = f[4], x = on.x, y = on.y + on.h, z = on.z, w = f[1],
            h = f[3], d = f[2], yaw = 0, room = r.index, storey = r.storey, light = true, on = on.index })
        else
          local p = along_walls("oil_lamp")
          if p then p.light = true end
        end
      end
      for _, p in ipairs(b.pieces) do
        if p.kind == "furniture" and p.room == r.index and not p.container then
          for _, c in ipairs(rule.containers or {}) do
            if p.item == c then
              b.containers[#b.containers + 1] = p.index
              p.container = #b.containers
            end
          end
        end
      end
    end
  end
  b.stage = 3
  return b
end

-- 6. What a container holds: { {item, count} } for the household. books(kind)
-- makes the text of a book when one is found.
function buildings.contents(b, n)
  local d = buildings.data()
  local s = rng.new(b.seed, "contents", n)
  local hh = b.household or {}
  local out = {}
  local p = b.pieces[b.containers[n]]
  if p and p.item == "bookcase" then
    out[#out + 1] = { "book", 1 + s:int(0, 2) }
    return out
  end
  for _ = 1, s:int(0, 2) do
    out[#out + 1] = { s:pick(d.contents.common), s:int(1, 4) }
  end
  local trade = d.contents[hh.trade or ""]
  if trade and s:chance(0.6) then out[#out + 1] = { s:pick(trade), s:int(1, 3) } end
  if (hh.wealth or 0) > 0.5 and s:chance(hh.wealth) then out[#out + 1] = { "coin", s:int(1, 12) } end
  local merged, at = {}, {}
  for _, it in ipairs(out) do -- one entry per thing
    if at[it[1]] then merged[at[it[1]]][2] = merged[at[it[1]]][2] + it[2]
    else merged[#merged + 1] = { it[1], it[2] }; at[it[1]] = #merged end
  end
  return merged
end

-- Rooms of a building by kind (for scripts and tests).
function buildings.rooms_of(b, kind)
  local out = {}
  for _, r in ipairs(b.rooms or {}) do
    if r.kind == kind then out[#out + 1] = r end
  end
  return out
end

-- Generates a building through its stages. b: { type, seed, style
-- (family id), household, land, plot, stilts, plinth }. stage: 1 massing, 2
-- plan and construction, 3 furnishing.
function buildings.generate(b, family, stage)
  if not b.program then b.program = buildings.program(b.type, b.household or {}, rng.new(b.seed, "program")) end
  if not b.w then buildings.mass(b, family) end
  b.stage = b.stage or 1
  if (stage or 1) >= 2 and b.stage < 2 then
    if not buildings.data().types[b.type].open then buildings.plan(b) end
    buildings.construct(b)
  end
  if (stage or 1) >= 3 then buildings.furnish(b) end
  return b
end

return buildings
