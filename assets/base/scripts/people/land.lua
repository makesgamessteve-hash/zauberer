-- SPDX-License-Identifier: MIT
-- The land as peoples see it (concept 10.4-10.5): the planet divided into
-- regions of about 100 km, each with its height, relief, climate, water,
-- fertility, minerals and ley, and its neighbours across the cube faces.
-- A created world gives its climate, biomes and minerals; the noise world
-- gets a climate from latitude, height and a seeded noise. Rivers come from
-- the rain running downhill from region to region. Deterministic.
local rng = require("people.rng")
local job = require("people.job")

local land = {}

land.CELL = 100000 -- metres: the size of a region

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

-- Regions along one edge of a cube face for a planet radius.
function land.size(radius)
  return math.max(4, math.floor(math.pi * radius / 2 / land.CELL + 0.5))
end

-- The class of a region's land from its climate and relief.
function land.class(temp, rain, height, relief, biome)
  if biome then
    local b = biome:lower()
    for _, k in ipairs({ "ice", "tundra", "desert", "steppe", "swamp", "rainforest", "forest", "savanna",
      "grassland", "shrub", "mountain", "taiga", "meadow" }) do
      if b:find(k, 1, true) then
        if k == "taiga" then return "forest" end
        if k == "meadow" or k == "shrub" then return "grassland" end
        return k
      end
    end
  end
  if temp < -8 then return "ice" end
  if height > 2600 or relief > 700 then return "mountain" end
  if temp < 0 then return "tundra" end
  if rain < 200 then return "desert" end
  if rain < 450 then return "steppe" end
  if rain > 1900 and temp > 21 then return "rainforest" end
  if rain > 1300 and height < 150 and relief < 60 then return "swamp" end
  if rain > 750 then return "forest" end
  if temp > 21 then return "savanna" end
  return "grassland"
end

-- The noise world's climate: latitude bands and height, then a seeded
-- wetness and warmth per region smoothed over its neighbours.
local function noise_climate(grid)
  local cells, seed = grid.cells, grid.seed
  local wet, warm = {}, {}
  for _, c in ipairs(cells) do
    wet[c.id], warm[c.id] = rng.float(seed, "wet", c.id), rng.float(seed, "warm", c.id)
  end
  for _ = 1, 3 do
    local w2, t2 = {}, {}
    for _, c in ipairs(cells) do
      local sw, st = wet[c.id] * 2, warm[c.id] * 2
      for _, id in ipairs(c.nb) do sw, st = sw + wet[id], st + warm[id] end
      w2[c.id], t2[c.id] = sw / (2 + #c.nb), st / (2 + #c.nb)
    end
    wet, warm = w2, t2
  end
  for _, c in ipairs(cells) do
    if c.noise then
      local x, y, z = c.dir[1], c.dir[2], c.dir[3]
      local lat = math.asin(clamp(z / math.sqrt(x * x + y * y + z * z), -1, 1))
      c.temp = 28 - 40 * math.abs(math.sin(lat)) ^ 1.5 - 6.5 * math.max(c.height, 0) / 1000 + (warm[c.id] - 0.5) * 16
      local band = 0.6 + 0.4 * math.cos(6 * lat)
      c.rain = 2000 * band * (0.1 + (wet[c.id] - 0.3) * 2.6) ^ 1.3
      if c.rain ~= c.rain or c.rain < 0 then c.rain = 0 end
    end
    if c.land then c.class = land.class(c.temp, c.rain, c.height, c.relief, c.biome) end
  end
end

-- Samples the planet: returns the grid { n, cells, radius, created } where each
-- cell has id, face, i, j, pos (its centre), height, relief, land, temp,
-- rain, class, river, lake, coast, flow, fertility, minerals, ley (metres to
-- the nearest nexus) and nb (neighbour ids).
function land.sample(seed)
  local planet = world.planet()
  local info = world.info() or {}
  local created = info.created and true or false
  local n = land.size(planet.radius)
  local cpe = planet.chunks_per_edge
  local cells = {}
  local cell_m = math.pi * planet.radius / 2 / n
  for f = 0, 5 do
    for j = 0, n - 1 do
      for i = 0, n - 1 do
        local function at(u, v)
          local cx, cy = (i + u) * cpe / n, (j + v) * cpe / n
          local ix, iy = math.floor(cx), math.floor(cy)
          return world.position(f, ix, iy, (cx - ix) * planet.chunk_size, (cy - iy) * planet.chunk_size, 0)
        end
        job.pace(1)
        local pos = at(0.5, 0.5)
        local h = world.height(pos)
        local kind = world.water(pos)
        local c = { id = #cells + 1, face = f, i = i, j = j, pos = pos, height = h, relief = 0,
          land = h > 0 and kind ~= "ocean", lake = kind == "lake", river = kind == "river", nb = {} }
        c.dir = { world.to_cartesian(pos) }
        if c.land then
          local a, b = world.height(at(0.2, 0.25)), world.height(at(0.8, 0.75))
          c.relief = math.max(h, a, b) - math.min(h, a, b)
          c.ley = mana.ley(pos).nexus_distance
          c.minerals = (created and world.minerals(pos)) or {}
          c.biome = created and world.biome(pos) or nil
          c.temp, c.rain = world.climate(pos)
        end
        c.noise = c.temp == nil
        c.temp, c.rain = c.temp or 0, c.rain or 0
        c.minerals = c.minerals or {}
        c.class = "sea"
        cells[#cells + 1] = c
      end
    end
  end
  local grid = { n = n, cells = cells, radius = planet.radius, created = created, cpe = cpe, cell_m = cell_m,
    seed = seed }
  -- Neighbours: a region's width away in the four directions (across faces too).
  for _, c in ipairs(cells) do
    job.pace(0.5)
    local seen = {}
    for k = 0, 3 do
      local p = world.move(c.pos, k * math.pi / 2, cell_m, 0, 0)
      local o = land.cell_at(grid, p)
      if o and o ~= c and not seen[o] then
        seen[o] = true
        c.nb[#c.nb + 1] = o.id
      end
    end
  end
  noise_climate(grid)
  for _, c in ipairs(cells) do
    if c.land then
      for _, id in ipairs(c.nb) do
        if not cells[id].land then c.coast = true end
      end
    end
  end
  land.rivers(grid)
  for _, c in ipairs(cells) do
    if c.land then
      local t = clamp(1 - math.abs(c.temp - 17) / 16, 0, 1)
      local r = clamp((c.rain - 150) / 700, 0, 1)
      if c.river or c.lake then r = math.max(r, 0.8) end
      if c.oasis then r = math.max(r, 0.7) end
      c.fertility = clamp(t * r * (1 - clamp(c.relief / 900, 0, 0.7)), 0, 1)
      if c.class == "ice" then c.fertility = 0 end
    else
      c.fertility = 0
    end
  end
  return grid
end

-- Rain runs from each region to its lowest neighbour; regions that gather
-- enough have a river. Dry regions below mountains or by a river get springs.
function land.rivers(grid)
  local cells = grid.cells
  local order = {}
  for _, c in ipairs(cells) do
    if c.land then order[#order + 1] = c end
    c.flow = 0
  end
  table.sort(order, function(a, b)
    if a.height ~= b.height then return a.height > b.height end
    return a.id < b.id
  end)
  for _, c in ipairs(order) do
    c.flow = c.flow + math.max(c.rain, 0) / 1000
    local low, lh = nil, c.height
    for _, id in ipairs(c.nb) do
      local o = cells[id]
      if o.height < lh then low, lh = o, o.height end
    end
    c.down = low and low.id or nil
    if low then low.flow = low.flow + c.flow end
  end
  for _, c in ipairs(order) do
    if c.flow > 3.5 then c.river = true end
    if c.class == "desert" and not c.river then
      local foot = false
      for _, id in ipairs(c.nb) do
        local o = cells[id]
        if o.land and (o.height > c.height + 400 or o.river) then foot = true end
      end
      c.oasis = foot and rng.float(grid.seed, "spring", c.id) < 0.55 or rng.float(grid.seed, "spring", c.id) < 0.08
    end
  end
end

-- The region at a position.
function land.cell_at(grid, pos)
  local n, cpe = grid.n, grid.cpe
  local i = math.min(n - 1, math.max(0, math.floor((pos.x + (pos.lx or 0) / 256) * n / cpe)))
  local j = math.min(n - 1, math.max(0, math.floor((pos.y + (pos.ly or 0) / 256) * n / cpe)))
  return grid.cells[pos.face * n * n + j * n + i + 1]
end

-- A place inside a region: u, v in 0..1 across it.
function land.point(grid, c, u, v)
  local cx, cy = (c.i + u) * grid.cpe / grid.n, (c.j + v) * grid.cpe / grid.n
  local ix, iy = math.floor(cx), math.floor(cy)
  return world.position(c.face, ix, iy, (cx - ix) * 256, (cy - iy) * 256, 0)
end

-- Cheapest paths from sources over the regions: cost(a, b) is the price of a
-- step (nil: no way). Returns cost and owner per cell id (owner: the source's
-- key) and the step each cell was reached from.
function land.spread(grid, sources, cost, limit)
  local dist, owner, from = {}, {}, {}
  local heap = {}
  local function push(d, id, key)
    heap[#heap + 1] = { d, id, key }
    local k = #heap
    while k > 1 do
      local p = k // 2
      local a, b = heap[p], heap[k]
      if a[1] < b[1] or (a[1] == b[1] and a[2] <= b[2]) then break end
      heap[p], heap[k] = b, a
      k = p
    end
  end
  local function pop()
    local top = heap[1]
    local last = table.remove(heap)
    if #heap > 0 then
      heap[1] = last
      local k = 1
      while true do
        local l, r, m = 2 * k, 2 * k + 1, k
        local function less(x, y) return x[1] < y[1] or (x[1] == y[1] and x[2] < y[2]) end
        if l <= #heap and less(heap[l], heap[m]) then m = l end
        if r <= #heap and less(heap[r], heap[m]) then m = r end
        if m == k then break end
        heap[k], heap[m] = heap[m], heap[k]
        k = m
      end
    end
    return top
  end
  for _, s in ipairs(sources) do
    dist[s.cell] = 0
    owner[s.cell] = s.key
    push(0, s.cell, s.key)
  end
  while #heap > 0 do
    job.pace(0.1)
    local top = pop()
    local d, id, key = top[1], top[2], top[3]
    if d <= (dist[id] or math.huge) and owner[id] == key then
      for _, nid in ipairs(grid.cells[id].nb) do
        local step = cost(grid.cells[id], grid.cells[nid], key)
        if step then
          local nd = d + step
          if nd <= (limit or math.huge) and nd < (dist[nid] or math.huge) then
            dist[nid], owner[nid], from[nid] = nd, key, id
            push(nd, nid, key)
          end
        end
      end
    end
  end
  return dist, owner, from
end

return land
