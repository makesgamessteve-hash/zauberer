-- SPDX-License-Identifier: MIT
-- Citadels and castles (concept 6.6): sited on the highest ground about, the
-- curtain wall following the defensible contour (where the ground falls
-- away), with corner and flanking towers and a gatehouse facing the way in
-- (gate, portcullis and murder holes); a moat or ditch on level ground; an
-- outer bailey with barracks, stables, smithy and armoury, kitchens and
-- stores, and an inner bailey round the keep or palace (audience hall with
-- an iwan, the ruler's quarters, a garden court, a shrine of the realm's
-- faith, a library), cistern and well house, bathhouse; below, cellars, a
-- dungeon and a secret passage out. Buildings are added until the complex
-- has the rooms its size asks for. It is held, abandoned, ruined, or taken
-- by bandits, a rival mage or the Ashen. Styles come from the people's
-- style family. Coordinates as in places.layout.
local rng = require("people.rng")
local buildings = require("places.buildings")

local castles = {}

castles.STATES = { "held", "abandoned", "ruined", "bandits", "rival mage", "the Ashen" }
castles.STYLES = {
  highland_citadel = { wall = "mudbrick", towers = "round", height = 10 },
  desert_fort = { wall = "mudbrick", towers = "round", height = 8 },
  stone_fortress = { wall = "stone", towers = "square", height = 12 },
  ring_fort = { wall = "timber", towers = "square", height = 6 },
  kasbah = { wall = "rammed_earth", towers = "square", height = 11 },
  river_fortress = { wall = "brick", towers = "round", height = 10 },
  cliff_stronghold = { wall = "stone", towers = "square", height = 9 },
}

local function rooms_of(btype, household, seed)
  return #buildings.program(btype, household, rng.new(seed, "program"))
end

-- Plans a castle. st: the settlement (or a part of one); ctx as for
-- places.layout; s: a random stream; at: { x, z, rooms } to place it there
-- with that many rooms (default: the highest ground within 300 m, 50-150).
function castles.plan(st, ctx, s, at)
  at = at or {}
  local style = castles.STYLES[ctx.family.castle] or castles.STYLES.stone_fortress
  local cx, cz = at.x, at.z
  if not cx then
    local best = -math.huge
    for r = 0, 300, 100 do
      for k = 0, (r == 0 and 0 or 15) do
        local a = k * math.pi / 8
        local x, z = math.sin(a) * r, math.cos(a) * r
        local h = ctx.height(x, z)
        if h > best then best, cx, cz = h, x, z end
      end
    end
  end
  local target = at.rooms or (50 + math.floor((st.prosperity or 0.5) * 100))
  local core_h = ctx.height(cx, cz)
  local R = 30 + target * 0.3
  -- The curtain wall along the defensible contour.
  local n, verts = 16, {}
  for k = 0, n - 1 do
    local a = k / n * 2 * math.pi
    local r = R * 0.75
    while r < R * 1.3 do
      if ctx.height(cx + math.sin(a) * r, cz + math.cos(a) * r) < core_h - 6 then break end
      r = r + 5
    end
    verts[#verts + 1] = { x = cx + math.sin(a) * r, z = cz + math.cos(a) * r, a = a, r = r }
  end
  local plan = { x = cx, z = cz, radius = R * 1.3 + 12, walls = {}, towers = {}, buildings = {}, style = style,
    state = st.castle_state or s:weighted({ { "held", 6 }, { "abandoned", 1 }, { "ruined", 1 }, { "bandits", 1 },
      { "rival mage", 0.5 }, { "the Ashen", 0.5 } }) }
  for k = 1, n do
    local a, b = verts[k], verts[k % n + 1]
    plan.walls[#plan.walls + 1] = { x0 = a.x, z0 = a.z, x1 = b.x, z1 = b.z, h = style.height, thick = 2.5,
      walk = true, crenels = true, mat = style.wall }
    if k % 2 == 1 then
      plan.towers[#plan.towers + 1] = { x = a.x, z = a.z, r = 3.5, h = style.height + 4,
        round = style.towers == "round",
        mat = style.wall }
    end
  end
  -- The gate faces the way in (towards the settlement's centre, or south).
  local gate_a = (cx ~= 0 or cz ~= 0) and math.atan(-cx, -cz) or math.pi
  local gv = verts[1]
  for _, v in ipairs(verts) do
    local d = math.abs((v.a - gate_a + math.pi) % (2 * math.pi) - math.pi)
    if d < math.abs((gv.a - gate_a + math.pi) % (2 * math.pi) - math.pi) then gv = v end
  end
  plan.gate = { x = gv.x, z = gv.z, heading = gv.a, portcullis = true, murder_holes = true }
  local level = math.abs(ctx.height(cx + R, cz) - core_h) + math.abs(ctx.height(cx - R, cz) - core_h) < 8
  if level then plan.moat = { r = R * 1.3 + 8, w = 6, water = (ctx.land or {}).river or false } end
  plan.inner = { r = R * 0.45 }
  local count = 0
  local function add(btype, x, z, yaw, size)
    local rec = { type = btype, x = x, z = z, yaw = yaw,
      seed = rng.hash(st.seed or 0, st.id, "castle", #plan.buildings),
      household = { wealth = 0.9, trade = btype == "armoury" and "smith" or nil, people = 8 },
      plot = { w = size, d = size }, style = ctx.family.id, land = ctx.land, castle = true }
    rec.ground = ctx.height(x, z)
    rec.plinth = 0
    plan.buildings[#plan.buildings + 1] = rec
    count = count + rooms_of(btype, rec.household, rec.seed)
    return rec
  end
  -- The gatehouse, the keep or palace, then the baileys' buildings.
  add("gatehouse", gv.x - math.sin(gv.a) * 6, gv.z - math.cos(gv.a) * 6, gv.a, 12)
  local palace = add("palace", cx, cz, gate_a, 34)
  palace.keep = true
  local ring = { "barracks", "stables", "armoury", "kitchens", "well_house", "bathhouse", "library", "storehouse",
    "barracks", "kitchens", "storehouse", "barracks" }
  local k = 0
  while count < target - 6 and k < 48 do
    k = k + 1
    local btype = ring[(k - 1) % #ring + 1]
    local a = gate_a + math.pi * 0.35 + k * 2 * math.pi / 13
    local r = R * (k <= 13 and 0.72 or 0.58)
    add(btype, cx + math.sin(a) * r, cz + math.cos(a) * r, a + math.pi, 16)
  end
  for _, t in ipairs(plan.towers) do -- every tower holds guard rooms
    if count >= target then break end
    local rec = add("castle_tower", t.x, t.z, 0, 7)
    rec.tower = true
  end
  -- Below the keep: cellars, a dungeon and a secret passage out.
  plan.cellars = {}
  for i = 1, 3 do plan.cellars[#plan.cellars + 1] = { kind = "cellar", n = i } end
  for i = 1, 3 do plan.cellars[#plan.cellars + 1] = { kind = "dungeon", n = i } end
  count = count + #plan.cellars
  local out_a = gate_a + math.pi
  plan.secret = { from = { x = cx, z = cz }, to = { x = cx + math.sin(out_a) * (R * 1.3 + 20),
    z = cz + math.cos(out_a) * (R * 1.3 + 20) } }
  plan.rooms = count
  if plan.state == "ruined" then
    for _, b in ipairs(plan.buildings) do b.ruin = { age = 200, cause = "war", seed = rng.hash(b.seed, "ruin") } end
  end
  plan.garrison = plan.state == "held" and math.floor(count * 0.6) or (plan.state == "abandoned" and 0 or
    math.floor(count * 0.3))
  return plan
end

return castles
