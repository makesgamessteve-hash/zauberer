-- SPDX-License-Identifier: MIT
-- Under the land in play: a cave mouth near the heir becomes a cave; the
-- heir walks down into it, under the ground, on its floors, its river
-- running; a dungeon under a site of history is entered by its stair, its
-- locked door opened with the key from its side room, its puzzle solved
-- by what the heir brings.
scenario { name = "caves" }
local game = demo.game
local places = demo.places
local underground = require("places.underground")
local caves = require("places.caves")
local land = require("people.land")
local wp = game.world_places
local P = wp.places
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if P.ready and world.chunk_stage(here) == 3 then break end
  sleep(5)
end
local FOOT = { move = world.move, height = world.ground_height, water = world.water,
  structure = function(pos, alt) return places.floor(P, pos, alt) end,
  blocked = function(a, b, alt) return places.blocked(P, a, b, alt) end }

-- spec: caves-and-dungeons: Caves in the land
local mouth
do
  local cell = land.cell_at(P.grid, here)
  local seen = {}
  local frontier = { cell.id }
  for _ = 1, 6 do
    local nextf = {}
    for _, id in ipairs(frontier) do
      if not seen[id] then
        seen[id] = true
        for _, m in ipairs(underground.mouths(P.db, P.grid, P.grid.cells[id])) do
          if not mouth then mouth = m end
        end
        for _, nid in ipairs(P.grid.cells[id].nb) do nextf[#nextf + 1] = nid end
      end
    end
    frontier = nextf
  end
  expect(mouth, "a cave mouth in the land about")
  expect(#underground.mouths(P.db, P.grid, P.grid.cells[cell.id]) == #underground.mouths(P.db, P.grid,
    P.grid.cells[cell.id]), "the same mouths every time")
  underground.tick(P.under, P.db, P.grid, { mouth.pos })
  expect(P.under.caves[mouth.seed], "near its mouth the cave comes to be")
end

-- spec: caves-and-dungeons: Down into a cave
do
  local cave = P.under.caves[mouth.seed]
  local c1 = cave.chambers[1]
  local heading = math.atan(c1.x, c1.z)
  game.carpet.pos = world.move(mouth.pos, 0, 0, 30, 0)
  local w = demo.walker.new(world.move(mouth.pos, heading, -0.5, 0, 0), heading, FOOT)
  w.pos.alt = mouth.pos.alt
  for _ = 1, 60 * 30 do
    demo.walker.update(w, { throttle = 1 }, 1 / 60, FOOT)
    world.set_focus(1, w.pos)
    local _, _, z = underground.local_of(cave.pos, w.pos)
    if z and math.sqrt((select(1, underground.local_of(cave.pos, w.pos))) ^ 2 + z ^ 2)
      > math.sqrt(c1.x ^ 2 + c1.z ^ 2) then break end
  end
  local ground = world.ground_height(w.pos)
  expect(w.pos.alt < ground - 3, "under the ground: " .. w.pos.alt .. " below " .. ground)
  local x, _, z = underground.local_of(cave.pos, w.pos)
  local f = caves.floor(cave, x, w.pos.alt - cave.pos.alt, z)
  expect(f and math.abs(cave.pos.alt + f - w.pos.alt) < 0.7, "on the cave's floor")
  -- The river runs: the spring fills, the lake drains out.
  local before = caves.water_volume(cave)
  for _ = 1, 30 do caves.flow(cave, 0.5) end
  expect(caves.water_volume(cave) ~= before, "water moves")
  expect(#cave.life > 0)
end

-- spec: caves-and-dungeons: Into a dungeon
do
  local sites = underground.dungeon_sites(P.db)
  expect(#sites > 0, "dungeons under the sites of history")
  local kinds = {}
  for _, d in ipairs(sites) do kinds[d.kind] = true end
  expect(kinds.tomb, "tombs of notable figures")
  local site
  for _, d in ipairs(sites) do
    if not site and d.kind == "tomb" then site = d end
  end
  underground.tick(P.under, P.db, P.grid, { site.pos })
  local g = P.under.dungeons[site.id]
  expect(g and g.stair, "the tomb comes to be, a stair down into it")
  -- Down the stair from the surface to the entrance room's floor.
  local top = world.move(g.origin, 0, g.stair.z0 - 0.5, g.stair.x, 0)
  top.alt = site.pos.alt
  local w = demo.walker.new(top, math.pi, FOOT)
  w.pos.alt = site.pos.alt
  for _ = 1, 60 * 8 do demo.walker.update(w, { throttle = 1 }, 1 / 60, FOOT) end
  expect(math.abs(w.pos.alt - g.origin.alt) < 0.7, "at the bottom of the stair: " .. (w.pos.alt - g.origin.alt))
  -- The locked door: shut without the key, open with it.
  local door, chest
  for _, p in ipairs(g.pieces) do
    if p.kind == "door" and not door then door = p end
    if p.key and not chest then chest = p end
  end
  if door then
    expect_eq(underground.use(P.under, g, door), "Locked")
    expect_eq(underground.use(P.under, g, chest), "Taken a key")
    expect(underground.use(P.under, g, door):find("opens", 1, true) and door.open)
  end
  -- A puzzle gives way to what the heir brings.
  local barrier
  for _, p in ipairs(g.pieces) do
    if p.kind == "barrier" and not barrier then barrier = p end
  end
  expect(barrier, "a puzzle bars the way")
  local at = world.move(g.origin, 0, barrier.z, barrier.x, 0)
  at.alt = g.origin.alt + 1
  expect(#underground.puzzles(P.under, { at }, function() return {} end) == 0 and not barrier.open)
  expect(underground.use(P.under, g, barrier) ~= "The way is open")
  local all = { heat = 50, burnt = true, water = 3, light = 1, careful = true, dark = true }
  expect(#underground.puzzles(P.under, { at }, function() return all end) == 1 and barrier.open)
  expect_eq(underground.use(P.under, g, barrier), "The way is open")
  -- Treasure at the goal.
  local goal
  for _, p in ipairs(g.pieces) do if p.treasure then goal = p end end
  expect(underground.use(P.under, g, goal):find("Found", 1, true))
end
