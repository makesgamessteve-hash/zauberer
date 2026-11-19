-- SPDX-License-Identifier: MIT
-- What lies under the land (concept 6.7): cave mouths where the land makes
-- caves (a few per region, by its rock and climate), and dungeons under
-- the sites history left - tombs of notable figures and elder vaults under
-- elder ruins - and under places: mines by mining towns, sanctums under
-- mage towers, bandit hideouts by the roads, crypts and catacombs under
-- temples and cities. They come to be when an heir draws near; a cave or
-- vault is walked into from its mouth or stair, and inside its floors are
-- the ground.
local rng = require("people.rng")
local land = require("people.land")
local history = require("people.history")
local caves = require("places.caves")
local dungeons = require("places.dungeons")

local underground = {}

underground.NEAR = 60     -- metres from a mouth within which a cave is made
underground.DEPTH = 6     -- metres down to a dungeon's floor
underground.MOUTHS = 2    -- cave mouths at most per region

-- The cave mouths of a region: { pos, kind, seed }.
function underground.mouths(db, grid, cell)
  local out = {}
  if not cell.land or cell.class == "sea" then return out end
  local s = rng.new(db.seed, "mouths", cell.id)
  local kind = caves.kind_for({ class = cell.class, rain = cell.rain, relief = cell.relief, coast = cell.coast,
    limestone = false, volcanic = cell.relief > 900 })
  local n = (cell.relief > 80 or cell.coast or cell.class == "ice") and s:int(0, underground.MOUTHS) or 0
  for i = 1, n do
    local p = land.point(grid, cell, 0.15 + s:float() * 0.7, 0.15 + s:float() * 0.7)
    p.alt = world.height(p)
    if p.alt > 1 then out[#out + 1] = { pos = p, kind = kind, seed = rng.hash(db.seed, "cave", cell.id, i) } end
  end
  return out
end

-- The dungeons of the world: { site (an id with pos), kind, seed }.
function underground.dungeon_sites(db)
  if db.dungeon_sites then return db.dungeon_sites end
  local out = {}
  for _, site in ipairs(db.site) do
    if site.kind == "tomb" then out[#out + 1] = { at = site.id, pos = site.pos, kind = "tomb" }
    elseif site.kind == "elder ruin" then out[#out + 1] = { at = site.id, pos = site.pos, kind = "elder_vault" } end
  end
  for _, st in ipairs(db.settlement) do
    local k = ({ mining_town = "mine", mage_tower = "sanctum", temple = "crypt", city = "catacomb" })[st.type]
    if k then out[#out + 1] = { at = st.id, pos = world.move(st.pos, 1, 80, 0, 0), kind = k } end
  end
  for i, r in ipairs(db.roads or {}) do -- bandits in the hills by some roads
    if i % 9 == 0 then
      local a = history.get(db, r.a)
      out[#out + 1] = { at = r.a, pos = world.move(a.pos, i, 3000, 0, 0), kind = "hideout" }
    end
  end
  for i, d in ipairs(out) do
    d.seed = rng.hash(db.seed, "dungeon", i)
    d.pos.alt = world.height(d.pos)
    d.id = "dungeon:" .. i
  end
  db.dungeon_sites = out
  return out
end

function underground.new()
  return { caves = {}, dungeons = {} }
end

-- Makes the caves and dungeons near the heirs.
function underground.tick(U, db, grid, viewers)
  for _, v in ipairs(viewers) do
    local cell = land.cell_at(grid, v)
    local cells = { cell }
    for _, id in ipairs(cell.nb) do cells[#cells + 1] = grid.cells[id] end
    for _, c in ipairs(cells) do
      for _, m in ipairs(underground.mouths(db, grid, c)) do
        if not U.caves[m.seed] and world.distance(v, m.pos) < underground.NEAR then
          local cave = caves.generate({ seed = m.seed, kind = m.kind })
          cave.pos, cave.id = m.pos, "cave:" .. m.seed
          U.caves[m.seed] = cave
        end
      end
    end
    for _, d in ipairs(underground.dungeon_sites(db)) do
      if not U.dungeons[d.id] and world.distance(v, d.pos) < underground.NEAR then
        local g = dungeons.generate(d.kind, d.seed)
        g.pos, g.id, g.kind = d.pos, d.id, d.kind
        g.origin = world.move(d.pos, 0, 0, 0, 0)
        g.origin.alt = d.pos.alt - underground.DEPTH
        underground.stair(g)
        U.dungeons[d.id] = g
      end
    end
  end
end

local function local_of(origin, pos)
  local a, b = world.to_local(origin), world.to_local(pos)
  return b[1] - a[1], (pos.alt or 0) - origin.alt, -(b[3] - a[3])
end
underground.local_of = local_of

-- The floor under a position if it is in a cave or dungeon (or on the way
-- down into one): its altitude and true (it is the ground there).
function underground.floor(U, pos, alt)
  for _, cave in pairs(U.caves) do
    local x, _, z = local_of(cave.pos, pos)
    local y = alt - cave.pos.alt
    if math.abs(x) < 400 and math.abs(z) < 400 then
      local f = caves.floor(cave, x, y, z)
      local entering = false
      local p = cave.passages[1]
      local lx, lz = p.b.x, p.b.z
      local t = (x * lx + z * lz) / (lx * lx + lz * lz)
      if t > 0 and t < 1 and math.sqrt((x - lx * t) ^ 2 + (z - lz * t) ^ 2) < p.r then entering = true end
      if f and (y < -1.5 or entering) then return cave.pos.alt + f, true, cave end
    end
  end
  for _, g in pairs(U.dungeons) do
    local x, _, z = local_of(g.origin, pos)
    local y = alt - g.origin.alt
    local stair = g.stair
    local on_stair = stair and (not g.closed or y < underground.DEPTH - 2) -- (a shut hatch shuts the top of the stair)
      and math.abs(x - stair.x) < 1 and z <= stair.z0 and z >= stair.z1
    if y < underground.DEPTH - 1.5 or on_stair then
      local best
      for _, p in ipairs(g.pieces) do
        if p.kind == "floor" and math.abs(x - p.x) <= p.w / 2 and math.abs(z - p.z) <= p.d / 2 then
          best = math.max(best or -1e9, p.y + p.h)
        end
      end
      if on_stair then -- down the stair from the surface to the entrance room
        best = math.max(best or -1e9, underground.DEPTH * (z - stair.z1) / (stair.z0 - stair.z1))
      end
      if best and best <= y + 0.6 then return g.origin.alt + best, true, g end
    end
  end
  return nil
end

-- The stair down into a dungeon's entrance room (made with the dungeon).
function underground.stair(g)
  local e = g.nodes[1]
  g.stair = { x = e.x, z0 = e.z + 10, z1 = e.z + 2 }
  return g.stair
end

-- Whether a step inside a dungeon runs into a wall, a locked door or a
-- puzzle's barrier not yet overcome.
function underground.blocked(U, a, b, alt)
  for _, g in pairs(U.dungeons) do
    local ax = local_of(g.origin, a)
    local ay = alt - g.origin.alt
    if ay < underground.DEPTH - 1 and math.abs(ax) < 200 then
      local bx, _, bz = local_of(g.origin, b)
      for _, p in ipairs(g.pieces) do
        if (p.kind == "wall" or ((p.kind == "door" or p.kind == "barrier") and not p.open))
          and math.abs(bx - p.x) <= p.w / 2 + 0.2 and math.abs(bz - p.z) <= p.d / 2 + 0.2 and ay < p.y + p.h then
          return true
        end
      end
    end
  end
  return false
end

-- The key, lock, barrier or treasure in front of someone in a dungeon:
-- returns the dungeon and the piece.
function underground.nearby(U, pos, heading)
  local hand = world.move(pos, heading, 1.0, 0, 0)
  for _, g in pairs(U.dungeons) do
    local x, _, z = local_of(g.origin, hand)
    local y = (pos.alt or 0) - g.origin.alt
    if y < underground.DEPTH and y > -1 then
      local best, bd
      for _, p in ipairs(g.pieces) do
        if not p.gone and (p.key or p.treasure or p.kind == "door" or p.kind == "barrier") then
          local d = math.sqrt((x - p.x) ^ 2 + (z - p.z) ^ 2)
          if d < 2 and (not bd or d < bd) then best, bd = p, d end
        end
      end
      if best then return g, best end
    end
  end
  return nil
end

-- Uses a dungeon piece: takes a key or a treasure, opens a lock with its
-- key; says what a barrier needs. Returns what was said.
function underground.use(U, g, p)
  U.keys = U.keys or {}
  U.keys[g.id] = U.keys[g.id] or {}
  if p.key then
    U.keys[g.id][p.key] = true
    p.gone = true
    return "Taken a key"
  elseif p.treasure then
    p.gone = true
    return "Found " .. p.treasure
  elseif p.kind == "door" then
    if p.open then return "The door stands open" end
    if U.keys[g.id][p.lock] then
      p.open = true
      return "The key turns: the door opens"
    end
    return "Locked"
  end
  return p.open and "The way is open" or dungeons.PUZZLES[p.puzzle].text
end

-- Barriers give way to what the world brings near them: w(pos) gives what
-- is at a world position ({ heat, burnt, water, light, careful, dark }).
function underground.puzzles(U, viewers, w)
  local opened = {}
  for _, g in pairs(U.dungeons) do
    for _, p in ipairs(g.pieces) do
      if p.kind == "barrier" and not p.open then
        local at = world.move(g.origin, 0, p.z, p.x, 0)
        at.alt = g.origin.alt + 1
        for _, v in ipairs(viewers) do
          if world.distance(v, at) < 5 and dungeons.solve(p.puzzle, w(at, v)) then
            p.open = true
            opened[#opened + 1] = p
          end
        end
      end
    end
  end
  return opened
end

return underground
