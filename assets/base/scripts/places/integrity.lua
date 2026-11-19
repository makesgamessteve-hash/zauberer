-- SPDX-License-Identifier: MIT
-- Structural integrity: every kit piece of a building rests on what is
-- beneath it (walls on floors and plinths, floors and roofs on walls, all
-- of it at last on the ground). Weight runs down the supports; a piece that
-- carries more than its material can bear breaks, and a piece that nothing
-- holds up any more falls and lies as rubble on whatever is below. Fire,
-- force and decay take pieces away; integrity says what follows.
local integrity = {}

-- kg per cubic metre and the load a square metre of a support's cross
-- section bears (kg), by material.
integrity.MATERIALS = {
  mudbrick = { 1600, 30000 }, rammed_earth = { 1800, 35000 }, brick = { 1900, 80000 }, stone = { 2400, 150000 },
  sandstone = { 2200, 100000 }, coral_stone = { 1700, 50000 }, plaster = { 1500, 25000 }, timber = { 600, 40000 },
  bamboo = { 400, 30000 }, felt = { 300, 2000 }, tile = { 1900, 60000 }, thatch = { 200, 1000 },
  rubble = { 1500, 20000 },
}
integrity.REST = 0.25 -- metres: how near a piece's foot must be to a support's top

local function prop(mat, i)
  local m = integrity.MATERIALS[mat] or integrity.MATERIALS.timber
  return m[i]
end

local function solid(p)
  return p.kind ~= "door" and p.kind ~= "window" and not p.gone
end

local function bearing(p) -- what carries weight (furniture and stairs only rest on things)
  return solid(p) and p.kind ~= "furniture" and p.kind ~= "stair"
end

local function overlap(p, q, grow)
  local g = grow or 0
  return p.x - p.w / 2 - g < q.x + q.w / 2 and q.x - q.w / 2 < p.x + p.w / 2 + g
    and p.z - p.d / 2 - g < q.z + q.d / 2 and q.z - q.d / 2 < p.z + p.d / 2 + g
end

local BUCKET = 0.25

-- The supports of each piece: the pieces whose tops meet its foot and whose
-- footprints overlap it (by index); a lintel over a door or window is held
-- by the wall beside it. And whether it stands on the ground.
function integrity.supports(pieces)
  local tops, walls = {}, {} -- bearing pieces by the height of their tops; walls by storey
  for j, q in ipairs(pieces) do
    if solid(q) and q.kind ~= "stair" then -- (furniture holds a lamp, but carries no building)
      local k = math.floor((q.y + q.h) / BUCKET)
      tops[k] = tops[k] or {}
      table.insert(tops[k], j)
      if q.kind == "wall" then
        local st = math.floor(q.y / 3)
        walls[st] = walls[st] or {}
        table.insert(walls[st], j)
      end
    end
  end
  local under, grounded = {}, {}
  for i, p in ipairs(pieces) do
    under[i] = {}
    if solid(p) then
      grounded[i] = (p.y <= integrity.REST and p.y >= -0.25) or p.kind == "plinth" or p.kind == "post"
      for k = math.floor((p.y - integrity.REST) / BUCKET), math.floor((p.y + integrity.REST) / BUCKET) do
        for _, j in ipairs(tops[k] or {}) do
          local q = pieces[j]
          if j ~= i and math.abs(q.y + q.h - p.y) <= integrity.REST and overlap(p, q)
            and (bearing(q) or p.kind == "furniture") then
            under[i][#under[i] + 1] = j
          end
        end
      end
      if (p.kind == "wall" or p.kind == "arch") and p.y % 3 > 0.5 then -- a lintel: held from the side
        for _, j in ipairs(walls[math.floor(p.y / 3)] or {}) do
          local q = pieces[j]
          if j ~= i and q.y < p.y and q.y + q.h >= p.y + p.h - 0.01 and overlap(p, q, 0.05) then
            under[i][#under[i] + 1] = j
          end
        end
      end
    end
  end
  return under, grounded
end

-- Which pieces are held up, from the ground through their supports.
function integrity.held(pieces)
  local under, grounded = integrity.supports(pieces)
  local over = {}
  for i, list in pairs(under) do
    for _, j in ipairs(list) do
      over[j] = over[j] or {}
      table.insert(over[j], i)
    end
  end
  local held, queue = {}, {}
  for i in pairs(grounded) do
    if grounded[i] then held[i] = true; queue[#queue + 1] = i end
  end
  local k = 1
  while k <= #queue do
    local j = queue[k]
    k = k + 1
    for _, i in ipairs(over[j] or {}) do
      if not held[i] then held[i] = true; queue[#queue + 1] = i end
    end
  end
  return held, under
end

local function weight(p) -- roofs are shells, not solid blocks
  local h = p.kind == "roof" and math.min(p.h, 0.2) or p.h
  return p.w * h * p.d * prop(p.mat, 1)
end

-- The load each piece carries (its own weight and all it holds up, shared
-- among its supports), from the top down.
function integrity.loads(pieces, under)
  local order = {}
  for i, p in ipairs(pieces) do
    if bearing(p) then order[#order + 1] = i end
  end
  table.sort(order, function(a, b)
    local pa, pb = pieces[a], pieces[b]
    if pa.y ~= pb.y then return pa.y > pb.y end
    return a < b
  end)
  local load = {}
  for _, i in ipairs(order) do
    local p = pieces[i]
    load[i] = (load[i] or 0) + weight(p) + (p.extra_load or 0)
    local sup = {}
    for _, j in ipairs(under[i]) do
      if bearing(pieces[j]) then sup[#sup + 1] = j end
    end
    if #sup > 0 then
      local share = load[i] / #sup
      for _, j in ipairs(sup) do load[j] = (load[j] or 0) + share end
    end
  end
  return load
end

-- What a piece can bear: its material's strength over its cross section.
function integrity.capacity(p)
  return prop(p.mat, 2) * math.max(p.w * p.d, 0.01) * 4
end

-- Settles a building after pieces were taken away (p.gone): pieces nothing
-- holds up fall, overloaded ones break, until it stands. Fallen pieces turn
-- to rubble on the floor below (resting on it as extra load). Returns the
-- indices of the pieces that fell or broke.
function integrity.settle(pieces)
  local fallen = {}
  for _ = 1, 20 do
    local held, under = integrity.held(pieces)
    local changed = false
    for i, p in ipairs(pieces) do
      if solid(p) and not held[i] then
        p.gone, p.fell = true, true
        fallen[#fallen + 1] = i
        changed = true
      end
    end
    if not changed then
      local load = integrity.loads(pieces, under)
      for i, p in ipairs(pieces) do
        if bearing(p) and load[i] and load[i] > integrity.capacity(p) and p.kind ~= "plinth" then
          p.gone, p.broke = true, true
          fallen[#fallen + 1] = i
          changed = true
        end
      end
    end
    if not changed then break end
  end
  -- Rubble: what fell rests on the nearest floor below it, or the ground.
  for _, i in ipairs(fallen) do
    local p = pieces[i]
    local rest = 0
    for j, q in ipairs(pieces) do
      if j ~= i and bearing(q) and q.kind == "floor" and q.y + q.h <= p.y + 0.01 and q.y + q.h > rest
        and math.abs(q.x - p.x) < q.w / 2 and math.abs(q.z - p.z) < q.d / 2 then
        rest = q.y + q.h
        q.extra_load = (q.extra_load or 0) + weight(p)
      end
    end
    p.rubble_at = rest
  end
  return fallen
end

-- Takes a piece away (burnt, smashed) and settles the building.
function integrity.remove(pieces, i)
  if not pieces[i] or pieces[i].gone then return {} end
  pieces[i].gone = true
  pieces[i].rubble_at = pieces[i].y -- (smashed where it stood)
  local fallen = integrity.settle(pieces)
  table.insert(fallen, 1, i)
  return fallen
end

return integrity
