-- SPDX-License-Identifier: MIT
-- Natural caves (concept 6.7): built from sparse voxel volumes (1 m cubes,
-- only the carved ones kept), from a cave graph of chambers joined by
-- passages, with a loop and a side branch, reached from the surface down a
-- sloping entrance. Karst caves have stalactites, an underground river
-- running from a spring through the passages downhill to a lake and out;
-- lava tubes are long and smooth, sea caves open to the shore, glacier
-- caves are cold. A separate 3D simulation moves water (it falls and
-- spreads, conserved but for the spring and the outflow), heat (diffusing
-- into rock at the cave's temperature) and foul air (diffusing). The walker
-- stands on floors made from the chambers and passages themselves, so they
-- are smooth. Coordinates are metres from the entrance (x east, y up, z
-- north); deterministic from the seed.
local rng = require("people.rng")

local caves = {}

caves.KINDS = {
  karst = { chambers = { 5, 8 }, radius = { 4, 9 }, passage = { 1.6, 2.4 }, river = true, formations = true,
    temp = 12, life = { "bats", "blind salamanders", "cave fungi", "cave crickets" } },
  lava_tube = { chambers = { 3, 5 }, radius = { 3, 5 }, passage = { 2.5, 3.5 }, river = false, formations = false,
    temp = 18, life = { "bats", "cave fungi" } },
  sea_cave = { chambers = { 2, 4 }, radius = { 5, 8 }, passage = { 2, 3 }, river = true, formations = false,
    temp = 14, life = { "crabs", "seabirds" } },
  glacier_cave = { chambers = { 3, 5 }, radius = { 3, 6 }, passage = { 1.6, 2.2 }, river = true, formations = false,
    temp = -2, life = { "ice worms" } },
}
caves.FLOOR = 1.2    -- metres: the floor lies this far below a chamber's or passage's middle
caves.STEP = 0.6

local OFF = 512
local function key(x, y, z) return (x + OFF) + (y + OFF) * 1024 + (z + OFF) * 1048576 end
caves.key = key

local function unkey(k)
  local x = k % 1024 - OFF
  local y = (k // 1024) % 1024 - OFF
  local z = k // 1048576 - OFF
  return x, y, z
end
caves.unkey = unkey

-- The kind of cave the land makes: karst in limestone (or wet hills), lava
-- tubes by old volcanoes, sea caves on coasts, glacier caves in the ice.
function caves.kind_for(land)
  land = land or {}
  if land.class == "ice" or land.class == "tundra" then return "glacier_cave" end
  if land.limestone or (land.rain or 0) > 500 and (land.relief or 0) > 100 then return "karst" end
  if land.volcanic then return "lava_tube" end
  if land.coast then return "sea_cave" end
  return "karst"
end

local function carve(cave, cx, cy, cz, r, ry)
  ry = ry or r
  for x = math.floor(cx - r), math.ceil(cx + r) do
    for y = math.floor(cy - ry), math.ceil(cy + ry) do
      for z = math.floor(cz - r), math.ceil(cz + r) do
        local dx, dy, dz = (x + 0.5 - cx) / r, (y + 0.5 - cy) / ry, (z + 0.5 - cz) / r
        if dx * dx + dy * dy + dz * dz <= 1 then
          local k = key(x, y, z)
          if not cave.air[k] then
            cave.air[k] = true
            cave.cells = cave.cells + 1
          end
        end
      end
    end
  end
end

-- Generates a cave. opts: { seed, kind, depth (of the first chamber) }.
function caves.generate(opts)
  local kind = caves.KINDS[opts.kind or "karst"] and (opts.kind or "karst") or "karst"
  local K = caves.KINDS[kind]
  local s = rng.new(opts.seed or 1, "cave", kind)
  local cave = { kind = kind, seed = opts.seed or 1, air = {}, water = {}, heat = {}, gas = {}, cells = 0,
    chambers = {}, passages = {}, formations = {}, life = K.life, temp = K.temp }
  -- The chambers, each lower than the last, wandering on.
  local n = s:int(K.chambers[1], K.chambers[2])
  local x, y, z = 0, -(opts.depth or (6 + s:float() * 6)), 0
  local heading = s:float() * 2 * math.pi
  local entrance = { x = 0, y = 0, z = 0 }
  for i = 1, n do
    local dist = i == 1 and (12 + s:float() * 8) or (22 + s:float() * 18)
    heading = heading + (s:float() - 0.5) * 1.6
    x, z = x + math.sin(heading) * dist, z + math.cos(heading) * dist
    if i > 1 then y = y - (2 + s:float() * 6) end
    local r = K.radius[1] + s:float() * (K.radius[2] - K.radius[1])
    cave.chambers[i] = { x = x, y = y, z = z, r = r, ry = r * 0.6, n = i }
  end
  -- A side branch off a middle chamber.
  local mid = cave.chambers[math.max(1, n // 2)]
  local ba = heading + math.pi / 2
  cave.chambers[#cave.chambers + 1] = { x = mid.x + math.sin(ba) * 25, y = mid.y - 3, z = mid.z + math.cos(ba) * 25,
    r = K.radius[1], ry = K.radius[1] * 0.6, n = #cave.chambers + 1, branch = true }
  -- Passages: the entrance down to the first chamber, chamber to chamber, a
  -- loop (the first to the third) and the branch.
  local function passage(a, b, extra)
    local p = { a = a, b = b, r = K.passage[1] + s:float() * (K.passage[2] - K.passage[1]) }
    for k2, v in pairs(extra or {}) do p[k2] = v end
    cave.passages[#cave.passages + 1] = p
    return p
  end
  passage(entrance, cave.chambers[1], { entrance = true })
  for i = 2, n do passage(cave.chambers[i - 1], cave.chambers[i]) end
  if n >= 3 then passage(cave.chambers[1], cave.chambers[3], { loop = true }) end
  passage(mid, cave.chambers[#cave.chambers], { branch = true })
  -- Carve: chambers as flattened spheres, passages as chains of spheres.
  for _, c in ipairs(cave.chambers) do carve(cave, c.x, c.y, c.z, c.r, c.ry) end
  for _, p in ipairs(cave.passages) do
    local len = math.sqrt((p.b.x - p.a.x) ^ 2 + (p.b.y - p.a.y) ^ 2 + (p.b.z - p.a.z) ^ 2)
    local steps = math.max(2, math.ceil(len / (p.r * 0.6)))
    for k = 0, steps do
      local t = k / steps
      local wob = math.sin(t * math.pi * 3 + s:float()) * (p.entrance and 0 or 1.2)
      carve(cave, p.a.x + (p.b.x - p.a.x) * t + wob, p.a.y + (p.b.y - p.a.y) * t, p.a.z + (p.b.z - p.a.z) * t + wob,
        p.r)
    end
    p.len = len
  end
  -- Formations: stalactites and stalagmites in the chambers of karst.
  if K.formations then
    for _, c in ipairs(cave.chambers) do
      for _ = 1, s:int(3, 8) do
        local a, d = s:float() * 2 * math.pi, s:float() * c.r * 0.7
        cave.formations[#cave.formations + 1] = { x = c.x + math.sin(a) * d, z = c.z + math.cos(a) * d,
          y = c.y + c.ry * 0.8, floor = c.y - caves.FLOOR, h = 0.5 + s:float() * 1.5, hanging = s:chance(0.6) }
      end
    end
  end
  -- The river: from a spring in the second chamber down to the last (a lake),
  -- and out at its far side.
  if K.river and n >= 2 then
    local deepest = cave.chambers[n]
    cave.spring = { x = cave.chambers[2].x, y = cave.chambers[2].y - caves.FLOOR + 0.5,
      z = cave.chambers[2].z, rate = 0.5 }
    cave.lake = deepest
    cave.outflow = { x = deepest.x + deepest.r * 0.7, y = deepest.y - caves.FLOOR, z = deepest.z }
    -- The lake begins half full.
    local level = deepest.y - caves.FLOOR + 1.0
    for k in pairs(cave.air) do
      local cx, cy, cz = unkey(k)
      if cy + 0.5 < level and (cx - deepest.x) ^ 2 + (cz - deepest.z) ^ 2 < deepest.r ^ 2 then cave.water[k] = 1 end
    end
  end
  cave.entrance = entrance
  cave.depth = cave.chambers[n].y
  return cave
end

-- The floor under a point (the highest within a step of y), from the shapes.
function caves.floor(cave, x, y, z)
  local best
  local function try(f)
    if f <= y + caves.STEP and (not best or f > best) then best = f end
  end
  for _, c in ipairs(cave.chambers) do
    local dx, dz = (x - c.x) / c.r, (z - c.z) / c.r
    if dx * dx + dz * dz < 0.85 then try(c.y - caves.FLOOR) end
  end
  for _, p in ipairs(cave.passages) do
    local ax, az, bx, bz = p.a.x, p.a.z, p.b.x, p.b.z
    local lx, lz = bx - ax, bz - az
    local l2 = lx * lx + lz * lz
    local t = l2 > 0 and math.max(0, math.min(1, ((x - ax) * lx + (z - az) * lz) / l2)) or 0
    local px, pz = ax + lx * t, az + lz * t
    local d = math.sqrt((x - px) ^ 2 + (z - pz) ^ 2)
    if d < p.r * 0.8 + 1.2 then -- (at the mouth the floor eases in from the ground)
      local sink = p.entrance and caves.FLOOR * math.min(1, t / 0.2) or caves.FLOOR
      try(p.a.y + (p.b.y - p.a.y) * t - sink)
    end
  end
  return best
end

-- Whether a point is in the cave's open space (or at its mouth).
function caves.open(cave, x, y, z)
  return cave.air[key(math.floor(x), math.floor(y), math.floor(z))] == true or y >= -0.5
end

-- How much water there is in all.
function caves.water_volume(cave)
  local v = 0
  for _, w in pairs(cave.water) do v = v + w end
  return v
end

local DIRS = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }

local function sorted_keys(t)
  local keys = {}
  for k in pairs(t) do keys[#keys + 1] = k end
  table.sort(keys)
  return keys
end

-- One step of the cave's water: it falls into open cells below and spreads
-- to lower neighbours; the spring adds and the outflow takes away.
function caves.flow(cave, dt)
  local water = cave.water
  if cave.spring then
    local k = key(math.floor(cave.spring.x), math.floor(cave.spring.y), math.floor(cave.spring.z))
    if cave.air[k] then water[k] = math.min(1, (water[k] or 0) + cave.spring.rate * dt) end
  end
  local keys = sorted_keys(water)
  table.sort(keys, function(a, b) -- the lowest first, so water settles from the bottom
    local _, ya = unkey(a)
    local _, yb = unkey(b)
    if ya ~= yb then return ya < yb end
    return a < b
  end)
  for _, k in ipairs(keys) do
    local w = water[k]
    if w and w > 1e-6 then
      local x, y, z = unkey(k)
      local below = key(x, y - 1, z)
      if cave.air[below] then
        local room = 1 - (water[below] or 0)
        local move = math.min(room, w)
        if move > 0 then
          water[below] = (water[below] or 0) + move
          w = w - move
        end
      end
      if w > 1e-6 then
        for _, d in ipairs(DIRS) do
          local nk = key(x + d[1], y, z + d[3])
          if cave.air[nk] then
            local diff = w - (water[nk] or 0)
            if diff > 0.01 then
              local move = diff / 5
              water[nk] = (water[nk] or 0) + move
              w = w - move
            end
          end
        end
      end
      water[k] = w > 1e-6 and w or nil
    end
  end
  if cave.outflow then
    local ok = key(math.floor(cave.outflow.x), math.floor(cave.outflow.y), math.floor(cave.outflow.z))
    for dy = 0, 2 do
      local k2 = ok + dy * 1024
      if water[k2] then water[k2] = math.max(0, water[k2] - 0.4 * dt) end
    end
  end
end

-- Heat and foul air diffuse through the open cells, flowing from each cell
-- to its open neighbours; rock pulls them back to the cave's temperature
-- (and clean air).
function caves.diffuse(cave, dt)
  local NB = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }
  local rate = math.min(1 / 7, dt * 0.5)
  local function spread(field, rest)
    local delta = {}
    for _, k in ipairs(sorted_keys(field)) do
      local v = field[k]
      local x, y, z = unkey(k)
      for _, d in ipairs(NB) do
        local nk = key(x + d[1], y + d[2], z + d[3])
        if cave.air[nk] then
          local nv = field[nk] or rest
          if v > nv then -- (each pair once: from the higher to the lower)
            local f = (v - nv) * rate
            delta[k] = (delta[k] or 0) - f
            delta[nk] = (delta[nk] or 0) + f
          elseif field[nk] == nil and v < nv then
            local f = (nv - v) * rate
            delta[k] = (delta[k] or 0) + f
          end
        else
          delta[k] = (delta[k] or 0) + (rest - v) * rate * 0.3
        end
      end
    end
    for k, dv in pairs(delta) do
      local v = (field[k] or rest) + dv
      if math.abs(v - rest) < 0.01 then field[k] = nil else field[k] = v end
    end
  end
  spread(cave.heat, cave.temp)
  spread(cave.gas, 0)
end

-- Heat or foul air released at a point.
function caves.release(cave, what, x, y, z, amount)
  local k = key(math.floor(x), math.floor(y), math.floor(z))
  if not cave.air[k] then return false end
  local field = what == "heat" and cave.heat or cave.gas
  field[k] = (field[k] or (what == "heat" and cave.temp or 0)) + amount
  return true
end

function caves.temperature(cave, x, y, z)
  return cave.heat[key(math.floor(x), math.floor(y), math.floor(z))] or cave.temp
end

function caves.foul(cave, x, y, z)
  return cave.gas[key(math.floor(x), math.floor(y), math.floor(z))] or 0
end

-- The rock cells bounding the open space within radius of a point (to draw).
function caves.walls_near(cave, x, y, z, radius)
  local out = {}
  local r = math.ceil(radius)
  local fx, fy, fz = math.floor(x), math.floor(y), math.floor(z)
  for i = fx - r, fx + r do
    for j = fy - r, fy + r do
      for k = fz - r, fz + r do
        if not cave.air[key(i, j, k)] then
          for _, d in ipairs({ { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } }) do
            if cave.air[key(i + d[1], j + d[2], k + d[3])] then
              out[#out + 1] = { i, j, k }
              break
            end
          end
        end
      end
    end
  end
  return out
end

-- Whether every chamber can be walked to from the entrance along passages.
function caves.connected(cave)
  local seen = { [cave.entrance] = true }
  local changed = true
  while changed do
    changed = false
    for _, p in ipairs(cave.passages) do
      if seen[p.a] and not seen[p.b] then seen[p.b], changed = true, true end
      if seen[p.b] and not seen[p.a] then seen[p.a], changed = true, true end
    end
  end
  for _, c in ipairs(cave.chambers) do
    if not seen[c] then return false end
  end
  return true
end

return caves
