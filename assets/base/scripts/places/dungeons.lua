-- SPDX-License-Identifier: MIT
-- Built dungeons (concept 6.7): crypts, catacombs, tombs of notable figures,
-- abandoned mines, elder vaults, mage sanctums and bandit hideouts, made
-- with a graph grammar. A mission graph from the entrance to the goal is
-- rewritten: a lock is put on the way with its key in a room before it, a
-- loop joins two rooms, an optional branch leads to a reward, a puzzle bars
-- a door. The graph is then laid out on a grid of rooms joined by
-- corridors and built of kit pieces. Puzzles are solved through the
-- simulation: melt the ice-sealed door, burn the thorny vines, flood the
-- chamber to float to the ledge, bring light to the crystal, step past the
-- plates that breathe gas, put out the torches to sneak past.
local rng = require("people.rng")

local dungeons = {}

dungeons.KINDS = {
  crypt = { rooms = { 6, 10 }, rules = { "lock", "branch", "puzzle", "loop" }, mat = "stone", goal = "sarcophagus",
    life = { "rats", "bats" } },
  catacomb = { rooms = { 10, 16 }, rules = { "loop", "loop", "lock", "branch", "branch", "puzzle" }, mat = "stone",
    goal = "ossuary", life = { "rats" } },
  tomb = { rooms = { 6, 12 }, rules = { "lock", "puzzle", "puzzle", "branch" }, mat = "sandstone", goal = "sarcophagus",
    life = { "scorpions" } },
  mine = { rooms = { 8, 14 }, rules = { "branch", "branch", "loop", "puzzle" }, mat = "timber", goal = "ore vein",
    life = { "bats", "cave crickets" } },
  elder_vault = { rooms = { 8, 12 }, rules = { "lock", "puzzle", "puzzle", "loop", "branch" }, mat = "stone",
    goal = "elder spellbook", life = {} },
  sanctum = { rooms = { 6, 10 }, rules = { "lock", "puzzle", "branch" }, mat = "plaster", goal = "spellbook",
    life = { "familiar" } },
  hideout = { rooms = { 5, 9 }, rules = { "branch", "loop", "puzzle" }, mat = "timber", goal = "loot",
    life = { "bandits" } },
}

dungeons.PUZZLES = {
  ice_door = { solve = "heat", text = "a door sealed in ice" },
  thorns = { solve = "fire", text = "thorny vines across the way" },
  flood = { solve = "water", text = "a ledge high above the floor" },
  crystal = { solve = "light", text = "a dark crystal in a shaft" },
  gas_plates = { solve = "careful", text = "pressure plates that breathe gas" },
  torches = { solve = "darkness", text = "guards by burning torches" },
}
dungeons.ROOM = 8    -- metres: a grid cell
dungeons.HEIGHT = 3.5

-- The mission graph: nodes { id, kind ("entrance", "room", "goal", "key",
-- "reward"), key, puzzle } and edges { a, b, lock (a key id), puzzle }.
function dungeons.mission(kind, seed)
  local K = assert(dungeons.KINDS[kind], "unknown dungeon " .. tostring(kind))
  local s = rng.new(seed, "mission", kind)
  local g = { nodes = {}, edges = {}, kind = kind }
  local function node(k) g.nodes[#g.nodes + 1] = { id = #g.nodes + 1, kind = k }; return g.nodes[#g.nodes] end
  local function edge(a, b, extra)
    local e = { a = a.id, b = b.id }
    for k2, v in pairs(extra or {}) do e[k2] = v end
    g.edges[#g.edges + 1] = e
    return e
  end
  -- The spine: entrance, rooms, goal.
  local n = s:int(K.rooms[1], K.rooms[2])
  local spine = { node("entrance") }
  for _ = 2, n - 1 do spine[#spine + 1] = node("room") end
  spine[#spine + 1] = node("goal")
  spine[#spine].treasure = K.goal
  for i = 2, #spine do edge(spine[i - 1], spine[i]) end
  local keys = 0
  local spanned, locked = {}, {} -- spine steps a loop goes round; spine steps locked
  local function spine_edge(i) -- the edge from spine[i] to spine[i + 1]
    for _, ed in ipairs(g.edges) do
      if ed.a == spine[i].id and ed.b == spine[i + 1].id then return ed end
    end
  end
  for _, rule in ipairs(K.rules) do
    if rule == "lock" then -- a locked door on the spine, no loop round it; its key in a side room before it
      local choices = {}
      for at = math.max(3, #spine // 2), #spine do
        if not spanned[at - 1] and not locked[at - 1] then choices[#choices + 1] = at end
      end
      if #choices > 0 then
        local at = s:pick(choices)
        keys = keys + 1
        spine_edge(at - 1).lock = keys
        locked[at - 1] = true
        local before = spine[s:int(1, at - 2)]
        local room = node("key")
        room.key = keys
        edge(before, room)
      end
    elseif rule == "loop" and #spine >= 4 then -- a second way round two steps of the spine (none locked)
      local choices = {}
      for a = 1, #spine - 3 do
        if not locked[a] and not locked[a + 1] then choices[#choices + 1] = a end
      end
      if #choices > 0 then
        local a = s:pick(choices)
        spanned[a], spanned[a + 1] = true, true
        local mid = node("room")
        edge(spine[a], mid)
        edge(mid, spine[a + 2], { loop = true })
      end
    elseif rule == "branch" then
      local from = spine[s:int(2, #spine - 1)]
      local r1 = node("room")
      local r2 = node("reward")
      r2.treasure = s:pick({ "a chest of coins", "an heirloom", "a scroll", "a silver lamp" })
      edge(from, r1, { optional = true })
      edge(r1, r2, { optional = true })
    elseif rule == "puzzle" then
      local list = {}
      for _, ed in ipairs(g.edges) do
        if not ed.lock and not ed.puzzle and not ed.optional then list[#list + 1] = ed end
      end
      local e = list[s:int(1, #list)]
      local names = {}
      for name in pairs(dungeons.PUZZLES) do names[#names + 1] = name end
      table.sort(names)
      e.puzzle = s:pick(names)
    end
  end
  g.spine = {}
  for i, nd in ipairs(spine) do g.spine[i] = nd.id end
  return g
end

-- Whether the goal can be reached: walking from the entrance, picking up
-- keys, opening locks with them, solving puzzles (all are solvable).
function dungeons.solvable(g)
  local have, reached = {}, { [1] = true }
  local changed = true
  while changed do
    changed = false
    for _, nd in ipairs(g.nodes) do
      if reached[nd.id] and nd.key and not have[nd.key] then have[nd.key], changed = true, true end
    end
    for _, e in ipairs(g.edges) do
      local open = not e.lock or have[e.lock]
      if open and reached[e.a] and not reached[e.b] then reached[e.b], changed = true, true end
      if open and reached[e.b] and not reached[e.a] then reached[e.a], changed = true, true end
    end
  end
  for _, nd in ipairs(g.nodes) do
    if nd.kind == "goal" then return reached[nd.id] == true end
  end
  return false
end

-- Lays the graph out on a grid: each node a cell next to one it joins, no
-- two in one cell; corridors along the edges.
function dungeons.layout(g, seed)
  local s = rng.new(seed, "layout")
  local at, used = {}, {}
  local function free(x, z) return not used[x .. "," .. z] end
  local function put(nd, x, z) at[nd.id] = { x = x, z = z }; used[x .. "," .. z] = nd.id end
  put(g.nodes[1], 0, 0)
  used["0,1"], used["0,2"] = true, true -- (the stair down comes in from the north)
  local placed = { [1] = true }
  for _ = 1, #g.nodes * 4 do
    for _, e in ipairs(g.edges) do
      local a, b = e.a, e.b
      if placed[a] ~= placed[b] then
        local from, to = placed[a] and a or b, placed[a] and b or a
        local p = at[from]
        local dirs = s:shuffle({ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
        for r = 1, 4 do
          local done = false
          for _, d in ipairs(dirs) do
            local x, z = p.x + d[1] * r, p.z + d[2] * r
            if free(x, z) then
              put(g.nodes[to], x, z)
              placed[to] = true
              done = true
              break
            end
          end
          if done then break end
        end
      end
    end
  end
  for _, nd in ipairs(g.nodes) do
    nd.cell = at[nd.id]
    nd.x, nd.z = at[nd.id].x * dungeons.ROOM, at[nd.id].z * dungeons.ROOM
  end
  for _, e in ipairs(g.edges) do
    local a, b = g.nodes[e.a], g.nodes[e.b]
    e.length = math.abs(a.x - b.x) + math.abs(a.z - b.z)
  end
  return g
end

-- Builds a laid-out dungeon of kit pieces (rooms of stone or timber, doors
-- where locks and puzzles bar the way); pieces as in places.buildings.
function dungeons.build(g, kind)
  local K = dungeons.KINDS[kind]
  local H, R = dungeons.HEIGHT, dungeons.ROOM
  local pieces = {}
  local function piece(p) pieces[#pieces + 1] = p; p.index = #pieces; return p end
  -- Where each room's corridors leave it: straight to a room in line, or
  -- along x first (from a) and z last (into b) round a corner.
  local exits = {}
  local function exit(nd, dx, dz) exits[nd.id] = exits[nd.id] or {}; exits[nd.id][dx .. "," .. dz] = true end
  local function sign(v) return v > 0 and 1 or (v < 0 and -1 or 0) end
  exit(g.nodes[1], 0, 1) -- the way in, from the stair
  for _, e in ipairs(g.edges) do
    local a, b = g.nodes[e.a], g.nodes[e.b]
    if a.x == b.x or a.z == b.z then
      exit(a, sign(b.x - a.x), sign(b.z - a.z))
      exit(b, sign(a.x - b.x), sign(a.z - b.z))
    else
      exit(a, sign(b.x - a.x), 0)
      exit(b, 0, sign(a.z - b.z))
    end
  end
  for _, nd in ipairs(g.nodes) do
    local w = R - 2
    piece({ kind = "floor", mat = K.mat, x = nd.x, y = -0.2, z = nd.z, w = w, h = 0.2, d = w, node = nd.id })
    piece({ kind = "roof", shape = "flat", mat = K.mat, x = nd.x, y = H, z = nd.z, w = w, h = 0.4, d = w,
      node = nd.id })
    for _, side in ipairs({ { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }) do -- walls, open where a corridor leaves
      local open = exits[nd.id] and exits[nd.id][side[1] .. "," .. side[2]]
      local cx, cz = nd.x + side[1] * w / 2, nd.z + side[2] * w / 2
      local along = side[1] == 0
      if open then -- two wall pieces beside a 2 m opening
        for _, off in ipairs({ -1, 1 }) do
          local ox, oz = along and off * (w / 4 + 0.5) or 0, along and 0 or off * (w / 4 + 0.5)
          piece({ kind = "wall", mat = K.mat, x = cx + ox, y = 0, z = cz + oz, w = along and w / 2 - 1 or 0.6,
            h = H, d = along and 0.6 or w / 2 - 1 })
        end
      else
        piece({ kind = "wall", mat = K.mat, x = cx, y = 0, z = cz, w = along and w or 0.6, h = H,
          d = along and 0.6 or w })
      end
    end
    if nd.treasure then
      piece({ kind = "furniture", item = nd.kind == "goal" and "altar" or "chest", mat = "stone", x = nd.x, y = 0,
        z = nd.z, w = 1.2, h = 1, d = 0.8, container = true, treasure = nd.treasure, node = nd.id })
    end
    if nd.key then
      piece({ kind = "furniture", item = "chest", mat = "timber", x = nd.x, y = 0, z = nd.z, w = 1, h = 0.6,
        d = 0.5, key = nd.key, node = nd.id })
    end
  end
  for i, e in ipairs(g.edges) do -- corridors, with their doors and puzzles at the far room
    local a, b = g.nodes[e.a], g.nodes[e.b]
    local mx, mz = (a.x + b.x) / 2, (a.z + b.z) / 2
    local horizontal = a.z == b.z
    if a.x == b.x or a.z == b.z then
      local len = horizontal and math.abs(b.x - a.x) - (R - 2) or math.abs(b.z - a.z) - (R - 2)
      piece({ kind = "floor", mat = K.mat, x = mx, y = -0.2, z = mz, w = horizontal and len or 2, h = 0.2,
        d = horizontal and 2 or len, edge = i })
    else -- round the corner (b.x, a.z)
      piece({ kind = "floor", mat = K.mat, x = (a.x + b.x) / 2, y = -0.2, z = a.z, w = math.abs(b.x - a.x) + 2,
        h = 0.2, d = 2, edge = i })
      piece({ kind = "floor", mat = K.mat, x = b.x, y = -0.2, z = (a.z + b.z) / 2, w = 2, h = 0.2,
        d = math.abs(b.z - a.z), edge = i })
    end
    if e.lock or e.puzzle then
      local dx, dz = (b.x - a.x), (b.z - a.z)
      local l = math.max(1e-6, math.sqrt(dx * dx + dz * dz))
      local px, pz = b.x - dx / l * (R / 2 - 1), b.z - dz / l * (R / 2 - 1)
      local barrier = piece({ kind = e.lock and "door" or "barrier", mat = e.puzzle == "ice_door" and "ice"
        or (e.puzzle == "thorns" and "thorns" or "timber"), x = px, y = 0, z = pz, w = horizontal and 0.3 or 2,
        h = 2.4, d = horizontal and 2 or 0.3, lock = e.lock, puzzle = e.puzzle, edge = i, open = false })
      e.barrier = barrier.index
    end
  end
  g.pieces = pieces
  return g
end

-- Whether a puzzle's barrier gives way to what the world brings to it:
-- { heat (deg C), burnt, water (metres deep), light (0..1), careful (crept
-- past the plates), dark (no torch burning near) }.
function dungeons.solve(puzzle, w)
  local P = dungeons.PUZZLES[puzzle]
  if not P then return false end
  if P.solve == "heat" then return (w.heat or 0) > 5 end
  if P.solve == "fire" then return w.burnt == true end
  if P.solve == "water" then return (w.water or 0) >= 2 end
  if P.solve == "light" then return (w.light or 0) >= 0.5 end
  if P.solve == "careful" then return w.careful == true end
  return w.dark == true
end

-- Generates a whole dungeon: { kind, graph, pieces }.
function dungeons.generate(kind, seed)
  local g = dungeons.mission(kind, seed)
  dungeons.layout(g, seed)
  dungeons.build(g, kind)
  g.life = dungeons.KINDS[kind].life
  return g
end

return dungeons
