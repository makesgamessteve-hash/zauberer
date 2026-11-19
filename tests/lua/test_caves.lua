-- SPDX-License-Identifier: MIT
-- Unit tests of caves (places/caves) and dungeons (places/dungeons).
local caves = require("places.caves")
local dungeons = require("places.dungeons")

local cave = caves.generate({ seed = 11, kind = "karst" })

-- spec: caves-and-dungeons: Caves from a cave graph
test.case("a karst cave: chambers and passages, a loop and a branch, all reached", function()
  test.expect(#cave.chambers >= 6 and #cave.passages >= #cave.chambers)
  test.expect(caves.connected(cave))
  local loop, branch = false, false
  for _, p in ipairs(cave.passages) do
    loop = loop or p.loop
    branch = branch or p.branch
  end
  test.expect(loop and branch)
  test.expect(cave.cells > 2000, "carved voxels")
  for i = 2, #cave.chambers - 1 do
    if not cave.chambers[i].branch and not cave.chambers[i - 1].branch then
      test.expect(cave.chambers[i].y < cave.chambers[i - 1].y, "each chamber lower")
    end
  end
  test.expect(#cave.formations > 0 and cave.spring and cave.lake and cave.outflow)
  test.expect(caves.open(cave, cave.chambers[1].x, cave.chambers[1].y, cave.chambers[1].z))
  test.expect(not caves.open(cave, 300, -100, 300))
  test.expect(caves.open(cave, 0, 0, 0), "the mouth is open")
  local k = caves.key(-3, 7, 12)
  local x, y, z = caves.unkey(k)
  test.expect(x == -3 and y == 7 and z == 12)
  for _, kind in ipairs({ "lava_tube", "sea_cave", "glacier_cave" }) do
    local c = caves.generate({ seed = 3, kind = kind })
    test.expect(caves.connected(c) and c.kind == kind, kind)
  end
  test.expect_eq(caves.generate({ seed = 3, kind = "nonsense" }).kind, "karst")
  test.expect_eq(caves.kind_for({ class = "ice" }), "glacier_cave")
  test.expect_eq(caves.kind_for({ class = "grassland", rain = 800, relief = 200 }), "karst")
  test.expect_eq(caves.kind_for({ class = "grassland", volcanic = true }), "lava_tube")
  test.expect_eq(caves.kind_for({ class = "grassland", coast = true }), "sea_cave")
  test.expect_eq(caves.kind_for(nil), "karst")
end)

-- spec: caves-and-dungeons: Walking in a cave
test.case("smooth floors down from the mouth into the chambers", function()
  local f0 = caves.floor(cave, 0, 0, 0)
  test.expect(f0 and math.abs(f0) < 0.6, "the mouth is level with the ground")
  local c1 = cave.chambers[1]
  local f1 = caves.floor(cave, c1.x, c1.y, c1.z)
  test.expect(f1 and f1 < c1.y and f1 > c1.y - c1.ry, "the first chamber's floor")
  -- Walking from the mouth to the first chamber the floor never jumps.
  local last = f0
  for t = 0.02, 1, 0.02 do
    local x, z = c1.x * t, c1.z * t
    local f = caves.floor(cave, x, last + 0.3, z)
    test.expect(f, "a floor all the way")
    test.expect(math.abs(f - last) < caves.STEP + 0.05, "no step too high")
    last = f
  end
  test.expect(caves.floor(cave, 900, 0, 900) == nil)
end)

-- spec: caves-and-dungeons: The cave's own simulation
test.case("water falls and spreads, kept but for spring and outflow; heat and foul air diffuse", function()
  local c = caves.generate({ seed = 5, kind = "karst" })
  local spring, outflow = c.spring, c.outflow
  c.spring, c.outflow = nil, nil
  c.water = {}
  local ch = c.chambers[2]
  local k = caves.key(math.floor(ch.x), math.floor(ch.y), math.floor(ch.z))
  c.water[k] = 1
  c.water[k + 1] = 1
  local before = caves.water_volume(c)
  for _ = 1, 30 do caves.flow(c, 0.1) end
  test.expect(math.abs(caves.water_volume(c) - before) < 1e-6, "water is kept")
  local lowest = math.huge
  for kk in pairs(c.water) do
    local _, y = caves.unkey(kk)
    lowest = math.min(lowest, y)
  end
  test.expect(lowest < math.floor(ch.y), "it fell to the floor")
  c.spring = spring
  c.outflow = outflow
  local v0 = caves.water_volume(c)
  for _ = 1, 20 do caves.flow(c, 0.5) end
  test.expect(caves.water_volume(c) > v0, "the spring runs")
  -- Heat from a torch spreads and fades into the rock.
  test.expect(caves.release(c, "heat", ch.x, ch.y, ch.z, 200))
  test.expect(not caves.release(c, "heat", 500, 500, 500, 10))
  test.expect(caves.temperature(c, ch.x, ch.y, ch.z) > c.temp + 100)
  caves.diffuse(c, 0.5)
  test.expect(caves.temperature(c, ch.x + 1, ch.y, ch.z) > c.temp, "heat spreads")
  for _ = 1, 25 do caves.diffuse(c, 0.5) end
  test.expect(caves.temperature(c, ch.x, ch.y, ch.z) < c.temp + 20, "and fades")
  caves.release(c, "gas", ch.x, ch.y, ch.z, 1)
  caves.diffuse(c, 0.5)
  test.expect(caves.foul(c, ch.x + 1, ch.y, ch.z) > 0)
  test.expect(#caves.walls_near(c, ch.x, ch.y, ch.z, 4) > 10, "rock round the open space")
end)

-- spec: caves-and-dungeons: Dungeons from a graph grammar
test.case("mission graphs: solvable, with locks after their keys, loops and branches", function()
  for kind in pairs(dungeons.KINDS) do
    for seed = 1, 12 do
      local g = dungeons.mission(kind, seed)
      test.expect(dungeons.solvable(g), kind .. " " .. seed)
      for _, e in ipairs(g.edges) do
        if e.lock then -- without its key the goal is out of reach
          local copy = { nodes = {}, edges = g.edges }
          for i, nd in ipairs(g.nodes) do
            copy.nodes[i] = { id = nd.id, kind = nd.kind, key = nd.key ~= e.lock and nd.key or nil }
          end
          test.expect(not dungeons.solvable(copy), "the lock bars the way")
        end
      end
    end
  end
  local g = dungeons.mission("catacomb", 4)
  local loops, optional = 0, 0
  for _, e in ipairs(g.edges) do
    if e.loop then loops = loops + 1 end
    if e.optional then optional = optional + 1 end
  end
  test.expect(loops >= 1 and optional >= 2)
  test.expect_error(function() dungeons.mission("pyramid", 1) end, "unknown dungeon")
  test.expect(not dungeons.solvable({ nodes = { { id = 1, kind = "entrance" } }, edges = {} }))
end)

-- spec: caves-and-dungeons: Laid out and built
test.case("laid out on a grid without overlaps and built of kit pieces", function()
  for kind in pairs(dungeons.KINDS) do
    local g = dungeons.generate(kind, 7)
    local cells = {}
    for _, nd in ipairs(g.nodes) do
      local key = nd.cell.x .. "," .. nd.cell.z
      test.expect(not cells[key], "one room per cell")
      cells[key] = true
    end
    local doors, barriers, floors = 0, 0, 0
    for _, p in ipairs(g.pieces) do
      if p.kind == "door" then doors = doors + 1 end
      if p.kind == "barrier" then barriers = barriers + 1 end
      if p.kind == "floor" then floors = floors + 1 end
    end
    local locks, puzzles = 0, 0
    for _, e in ipairs(g.edges) do
      if e.lock then locks = locks + 1 end
      if e.puzzle and not e.lock then puzzles = puzzles + 1 end
    end
    test.expect(doors == locks and barriers == puzzles, kind)
    test.expect(floors >= #g.nodes + #g.edges - 1)
    test.expect(g.life)
  end
end)

-- spec: caves-and-dungeons: Puzzles of the simulation
test.case("each puzzle gives way to what the world brings", function()
  test.expect(dungeons.solve("ice_door", { heat = 20 }) and not dungeons.solve("ice_door", { heat = -5 }))
  test.expect(dungeons.solve("thorns", { burnt = true }) and not dungeons.solve("thorns", {}))
  test.expect(dungeons.solve("flood", { water = 2.5 }) and not dungeons.solve("flood", { water = 1 }))
  test.expect(dungeons.solve("crystal", { light = 1 }) and not dungeons.solve("crystal", { light = 0.1 }))
  test.expect(dungeons.solve("gas_plates", { careful = true }) and not dungeons.solve("gas_plates", {}))
  test.expect(dungeons.solve("torches", { dark = true }) and not dungeons.solve("torches", { dark = false }))
  test.expect(not dungeons.solve("riddle", {}))
end)
