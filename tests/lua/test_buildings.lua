-- SPDX-License-Identifier: MIT
-- Unit tests of building generation (places/buildings) and structural
-- integrity (places/integrity).
local buildings = require("places.buildings")
local integrity = require("places.integrity")
local peopling = require("people.peopling")

local FAM = peopling.data().family

local function make(btype, family, extra)
  local b = { type = btype, seed = 77, household = { wealth = 0.7, trade = "weaver" },
    land = { class = "grassland", temp = 18, rain = 500 } }
  for k, v in pairs(extra or {}) do b[k] = v end
  return buildings.generate(b, FAM[family or "persianate"], 3)
end

local function reachable(b)
  -- every room can be walked to from the front door (or reached by stairs)
  local front
  for _, d in ipairs(b.doors) do if d.kind == "front" then front = d end end
  local seen, queue = {}, { front.a }
  seen[front.a] = true
  local k = 1
  while k <= #queue do
    local r = queue[k]
    k = k + 1
    for _, di in ipairs(r.doors or {}) do
      local d = b.doors[di]
      for _, o in ipairs({ d.a, d.c }) do
        if o and not seen[o] then seen[o] = true; queue[#queue + 1] = o end
      end
    end
    for _, st in ipairs(b.stairs) do
      if b.rooms[st.room] == r and not seen[b.rooms[st.to]] then
        seen[b.rooms[st.to]] = true
        queue[#queue + 1] = b.rooms[st.to]
      end
    end
  end
  for _, r in ipairs(b.rooms) do
    if not seen[r] then return false, r.kind .. "@" .. r.storey end
  end
  return true
end

-- spec: building-generation: Programs
test.case("a building's program follows its type and household", function()
  local s = require("people.rng").new(1)
  local weaver = buildings.program("home", { trade = "weaver", wealth = 0.2 }, s)
  local kinds = {}
  for _, r in ipairs(weaver) do kinds[r.kind] = (kinds[r.kind] or 0) + 1 end
  test.expect(kinds.hearth == 1 and kinds.sleeping == 1 and kinds.store == 1 and kinds.loomroom == 1)
  local rich = buildings.program("home", { wealth = 0.9 }, s)
  local up = false
  for _, r in ipairs(rich) do up = up or r.upstairs end
  test.expect(up, "the wealthy sleep upstairs")
  local serai = buildings.program("caravanserai", {}, s)
  test.expect(#serai >= 20, "a caravanserai of 20 rooms and more")
  test.expect_error(function() buildings.program("spaceship", {}, s) end, "unknown building type")
end)

-- spec: building-generation: Massing by style and land
test.case("massing follows the style family and the land", function()
  local a = make("home", "anatolian", { land = { class = "forest", temp = 10, rain = 1200 } })
  test.expect(a.material == "timber" or a.material == "stone" or a.material == "plaster")
  test.expect(a.roof == "gable" or a.roof == "flat" or a.roof == "dome")
  local d = make("home", "arabian", { land = { class = "desert", temp = 28, rain = 50 } })
  test.expect_eq(d.roof, "flat")
  test.expect(d.material == "mudbrick" or d.material == "coral_stone" or d.material == "plaster")
  test.expect(d.storeys >= 2, "tower houses")
  test.expect(d.w % 0.5 == 0 and d.d % 0.5 == 0, "on the 0.5 m grid")
  local tent = make("tent", "steppe")
  test.expect(tent.roof == "yurt" and tent.material == "felt" and tent.storeys == 1)
  local tower = make("tower")
  test.expect(tower.storeys >= 3 and tower.roof == "dome")
  local hot = make("home", "persianate", { land = { class = "desert", temp = 26, rain = 100 }, seed = 5 })
  test.expect(hot.windcatcher ~= nil)
end)

-- spec: building-generation: Floor plans
test.case("floor plans: every room reached from the door, courtyards, corridors, stairs", function()
  for _, t in ipairs({ "home", "farmhouse", "smithy", "teahouse", "temple", "bazaar", "caravanserai", "bathhouse",
    "library", "tower", "weaver", "palace", "gatehouse", "barracks", "kitchens" }) do
    local b = make(t)
    local ok, lost = reachable(b)
    test.expect(ok, t .. ": " .. tostring(lost))
    for _, r in ipairs(b.rooms) do
      test.expect(r.x1 - r.x0 >= 1.4 and r.z1 - r.z0 >= 1.4, t .. ": a room too small")
      test.expect(r.x0 >= -b.w / 2 - 1e-6 and r.x1 <= b.w / 2 + 1e-6, t .. ": a room outside")
    end
  end
  local serai = make("caravanserai")
  test.expect(serai.court and serai.iwan, "a caravanserai round a court with an iwan")
  local barracks = make("barracks")
  test.expect(barracks.corridor ~= nil or barracks.courtyard, "big buildings get a corridor")
  local tower = make("tower")
  test.expect_eq(#tower.stairs, tower.storeys - 1)
  local len = buildings.shared({ x0 = 0, z0 = 0, x1 = 2, z1 = 2 }, { x0 = 2, z0 = 1, x1 = 4, z1 = 5 })
  test.expect_eq(len, 1)
  test.expect_eq(buildings.shared({ x0 = 0, z0 = 0, x1 = 2, z1 = 2 }, { x0 = 5, z0 = 5, x1 = 6, z1 = 6 }), 0)
end)

-- spec: building-generation: Kit pieces
test.case("kit pieces: wall segments, openings, floors, roofs", function()
  local b = make("home", "anatolian")
  local kinds = {}
  for _, p in ipairs(b.pieces) do
    kinds[p.kind] = (kinds[p.kind] or 0) + 1
    test.expect(p.mat, "every piece has a material")
    if p.kind == "wall" then
      test.expect(math.max(p.w, p.d) <= buildings.SEGMENT + 1e-6, "segments are short")
    end
  end
  test.expect(kinds.wall > 20 and kinds.door == #b.doors and kinds.window >= 1 and kinds.floor >= #b.rooms)
  test.expect(kinds.roof >= 1 and kinds.stair == #b.stairs)
  local well = make("well")
  test.expect(#well.rooms == 0 and well.pieces[1].kind == "well")
  local dock = make("dock")
  test.expect(dock.pieces[1].kind == "floor" and dock.pieces[2].kind == "post")
  local stilts = make("home", "far_east", { stilts = 2 })
  test.expect_eq(stilts.pieces[1].kind, "post")
  local plinth = make("home", "anatolian", { plinth = 1.5 })
  test.expect_eq(plinth.pieces[1].kind, "plinth")
end)

-- spec: building-generation: Furnishing
test.case("furnishing: against walls with room before them, groups, lights, containers", function()
  local b = make("home", "persianate")
  local by_room = {}
  for _, p in ipairs(b.pieces) do
    if p.kind == "furniture" then
      by_room[p.room] = by_room[p.room] or {}
      table.insert(by_room[p.room], p)
      local r = b.rooms[p.room]
      test.expect(p.x - p.w / 2 >= r.x0 and p.x + p.w / 2 <= r.x1 and p.z - p.d / 2 >= r.z0
        and p.z + p.d / 2 <= r.z1, "inside its room")
    end
  end
  local items = {}
  for _, list in pairs(by_room) do
    for _, p in ipairs(list) do items[p.item] = (items[p.item] or 0) + 1 end
    for i = 1, #list do
      for j = i + 1, #list do
        local a, c = list[i], list[j]
        local overlap = math.abs(a.x - c.x) < (a.w + c.w) / 2 - 1e-6 and math.abs(a.z - c.z) < (a.d + c.d) / 2 - 1e-6
        test.expect(not overlap or a.item == "rug" or c.item == "rug" or a.on or c.on, "pieces do not overlap")
      end
    end
  end
  test.expect(items.hearth and items.bed and items.loom, "a weaver's hearth, bed and loom")
  test.expect(items.oil_lamp or items.brazier)
  test.expect(#b.containers > 0)
  test.expect_eq(buildings.furnish(b), b) -- (only once)
  local smith = make("home", "persianate", { household = { trade = "smith", wealth = 0.3 } })
  local forge = false
  for _, p in ipairs(smith.pieces) do forge = forge or p.item == "forge" end
  test.expect(forge, "a smith's workshop has a forge")
  test.expect_eq(#buildings.rooms_of(b, "hearth"), 1)
end)

-- spec: building-generation: Contents on opening
test.case("contents: the same every time, matched to the household", function()
  local b = make("library")
  local book
  for n, i in ipairs(b.containers) do
    if b.pieces[i].item == "bookcase" then book = n end
  end
  test.expect(book and buildings.contents(b, book)[1][1] == "book")
  local rich = make("home", "persianate", { household = { trade = "merchant", wealth = 1 } })
  local again = 0
  for n = 1, #rich.containers do
    local a, c = buildings.contents(rich, n), buildings.contents(rich, n)
    test.expect_eq(#a, #c)
    again = again + #a
  end
  test.expect(again > 0)
end)

-- spec: structural-integrity: Supports
test.case("every piece stands on the ground or on what holds it", function()
  for _, t in ipairs({ "home", "caravanserai", "tower", "temple", "palace" }) do
    local b = make(t, "anatolian")
    local held = integrity.held(b.pieces)
    for i, p in ipairs(b.pieces) do
      if p.kind ~= "door" and p.kind ~= "window" then test.expect(held[i], t .. ": " .. p.kind .. " floats") end
    end
    test.expect_eq(#integrity.settle(b.pieces), 0)
  end
end)

-- spec: structural-integrity: Collapse
test.case("taking away what holds a piece brings it down as rubble", function()
  local b = make("tower", "anatolian")
  local fallen = {}
  for i, p in ipairs(b.pieces) do
    if p.kind == "wall" and p.storey == 1 and not p.gone then
      for _, k in ipairs(integrity.remove(b.pieces, i)) do fallen[#fallen + 1] = k end
    end
  end
  local roof_fell, upper = false, 0
  for _, k in ipairs(fallen) do
    local p = b.pieces[k]
    if p.kind == "roof" then roof_fell = true end
    if (p.storey or 1) > 1 then upper = upper + 1 end
    test.expect(p.gone)
  end
  test.expect(roof_fell and upper > 10, "the storeys above fall")
  for _, k in ipairs(fallen) do test.expect(b.pieces[k].rubble_at ~= nil) end
  test.expect_eq(#integrity.remove(b.pieces, fallen[1]), 0) -- (already gone)
  test.expect_eq(#integrity.remove(b.pieces, 99999), 0)
end)

-- spec: structural-integrity: Overload
test.case("a piece carrying more than it can bear breaks", function()
  local pieces = {
    { kind = "wall", mat = "thatch", x = 0, y = 0, z = 0, w = 0.2, h = 3, d = 0.2 },
    { kind = "floor", mat = "stone", x = 0, y = 3, z = 0, w = 4, h = 1, d = 4 },
  }
  local fallen = integrity.settle(pieces)
  test.expect(#fallen == 2 and pieces[1].broke and pieces[2].fell)
  test.expect(integrity.capacity({ mat = "stone", w = 1, d = 1 })
    > integrity.capacity({ mat = "thatch", w = 1, d = 1 }))
  local strong = {
    { kind = "wall", mat = "stone", x = 0, y = 0, z = 0, w = 1, h = 3, d = 1 },
    { kind = "floor", mat = "timber", x = 0, y = 3, z = 0, w = 2, h = 0.2, d = 2 },
    { kind = "furniture", mat = "timber", x = 0, y = 3.2, z = 0, w = 1, h = 1, d = 1 },
  }
  test.expect_eq(#integrity.settle(strong), 0)
  local loads = integrity.loads(strong, select(2, integrity.held(strong)))
  test.expect(loads[1] > loads[2])
  strong[1].gone = true
  test.expect_eq(#integrity.settle(strong), 2) -- the floor and what stood on it
  local unknown = { { kind = "wall", mat = "adamant", x = 0, y = 0, z = 0, w = 1, h = 1, d = 1 } }
  test.expect_eq(#integrity.settle(unknown), 0)
end)
