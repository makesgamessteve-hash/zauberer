-- SPDX-License-Identifier: MIT
-- Set pieces (concept 6.9): story places from parametric templates. A
-- template fixes what the story needs - here Anarvan: Farrokh's weaver's
-- house on the square with a hidden cellar under the loom that is larger
-- than a cellar should be (his sanctum), the qanat outlet, the goat pen,
-- the pomegranate orchards - and the procedural pipeline does the rest:
-- the terrain fit, the style of the village's people, the furnishing.
local rng = require("people.rng")
local buildings = require("places.buildings")
local dungeons = require("places.dungeons")

local setpieces = {}

setpieces.CELLAR = 6 -- metres down to the sanctum's floor

local function clear_spot(plan, x, z, r)
  for i = #plan.buildings, 1, -1 do
    local b = plan.buildings[i]
    if math.sqrt((b.x - x) ^ 2 + (b.z - z) ^ 2) < r + math.max(b.plot.w, b.plot.d) / 2 then
      table.remove(plan.buildings, i)
    end
  end
end

local function add(plan, st, ctx, rec)
  rec.seed = rng.hash(st.seed or 0, st.id, rec.role)
  rec.style = ctx.family.id
  rec.land = ctx.land
  rec.plot = rec.plot or { w = 12, d = 12 }
  rec.ground = ctx.height(rec.x, rec.z)
  rec.plinth = 0
  plan.buildings[#plan.buildings + 1] = rec
  return rec
end

-- Applies Anarvan's template to its laid-out plan.
function setpieces.anarvan(plan, st, ctx)
  local r = plan.plaza and plan.plaza.r or 10
  -- Farrokh's house on the north side of the square, its door to the square.
  local hx, hz = 0, r + 9
  clear_spot(plan, hx, hz, 9)
  local house = add(plan, st, ctx, { type = "weaver", role = "farrokh", x = hx, z = hz, yaw = math.pi,
    household = { wealth = 0.6, trade = "weaver", people = 1 } })
  -- The qanat outlet west of the square, the goat pen east.
  clear_spot(plan, -r - 8, 0, 5)
  local qanat = add(plan, st, ctx, { type = "qanat_outlet", role = "qanat", x = -r - 8, z = 0, yaw = math.pi / 2,
    household = { wealth = 0.3 }, plot = { w = 7, d = 7 } })
  clear_spot(plan, r + 9, 0, 6)
  local pen = add(plan, st, ctx, { type = "pen", role = "pen", x = r + 9, z = 0, yaw = -math.pi / 2,
    household = { wealth = 0.3 }, plot = { w = 9, d = 9 } })
  for i, b in ipairs(plan.buildings) do b.n = i; b.id = plan.id .. "/" .. i end
  -- Pomegranate orchards round the south of the village.
  plan.fields[#plan.fields + 1] = { kind = "orchards", crop = "pomegranate orchards", from = math.pi * 0.75,
    to = math.pi * 1.25, r0 = (plan.radius or 60) + 5, r1 = (plan.radius or 60) + 80 }
  plan.setpiece = "anarvan"
  plan.farrokh, plan.qanat, plan.pen = house, qanat, pen
  return plan
end

-- Farrokh's sanctum under his loom: a cellar room at the foot of a stair
-- from the hatch, the sanctum beyond it with his books and tools, and a
-- workroom. Returns a dungeon (as places.dungeons) placed under the house;
-- hatch: the hatch's east and north offsets from the house's middle.
function setpieces.sanctum(origin, hatch_e, hatch_n)
  local R = dungeons.ROOM
  local g = { kind = "sanctum", nodes = {
    { id = 1, kind = "entrance", x = hatch_e, z = hatch_n - 6 - (R - 2) / 2 },
    { id = 2, kind = "room", x = hatch_e, z = hatch_n - 6 - (R - 2) / 2 - R, sanctum = true },
    { id = 3, kind = "goal", x = hatch_e + R, z = hatch_n - 6 - (R - 2) / 2 - R, treasure = "Farrokh's letter" },
  }, edges = { { a = 1, b = 2 }, { a = 2, b = 3 } } }
  dungeons.build(g, "sanctum")
  for _, p in ipairs(g.pieces) do
    if p.node == 2 and p.kind == "floor" then -- the books, the tools and the bag in the sanctum
      g.pieces[#g.pieces + 1] = { kind = "furniture", item = "bookcase", mat = "timber", x = p.x - 2, y = 0,
        z = p.z - 2.5, w = 1.5, h = 2, d = 0.4, treasure = "the Primer", index = #g.pieces + 1 }
      g.pieces[#g.pieces + 1] = { kind = "furniture", item = "chest", mat = "timber", x = p.x + 2, y = 0,
        z = p.z - 2.5, w = 1, h = 0.6, d = 0.5, treasure = "the endless bag", index = #g.pieces + 1 }
    end
  end
  g.origin = world.move(origin, 0, 0, 0, 0)
  g.origin.alt = origin.alt - setpieces.CELLAR
  g.stair = { x = hatch_e, z0 = hatch_n, z1 = hatch_n - 6 }
  g.id, g.pos, g.life = "sanctum:farrokh", origin, {}
  return g
end

-- Where the hatch is: under the loom room of Farrokh's house (east and
-- north of the house's middle).
function setpieces.hatch(house)
  local room = buildings.rooms_of(house, "loomroom")[1] or house.rooms[1]
  local x, z = (room.x0 + room.x1) / 2, (room.z0 + room.z1) / 2
  local yaw = house.yaw
  return x * math.cos(yaw) + z * math.sin(yaw), -x * math.sin(yaw) + z * math.cos(yaw)
end

return setpieces
