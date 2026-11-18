-- SPDX-License-Identifier: MIT
-- M2 water bodies in a created world: find a river, a lake and the sea, read
-- their water, and let a river carry the carpet (headless).
-- spec: water-bodies: Water from Lua
-- spec: water-bodies: The carpet rides on water
scenario { name = "water bodies" }
local game = demo.game
-- The noise world has the sea and nothing else.
local start = game.carpet.pos
expect_eq(world.find_water(start, "river"), nil)
expect_error(function() world.find_water(start, "puddle") end, "invalid option")
game.world_options = { radius = 400000, quality = "quick", seed = 11 }
game.set_mode("create")
for _ = 1, 20000 do
  step(1)
  if game.creation.progress and game.creation.progress.done then break end
  sleep(2)
end
expect(game.fly_in_new_world())
local here = game.carpet.pos
-- An inland stretch of river (the nearest may be a mouth, where it is the sea).
local river
for k = 0, 15 do
  local from = world.move(here, k * math.pi / 8, (k // 4) * 15000 + 1, 0, 0)
  local found = world.find_water(from, "river", 400000)
  local kind, level = world.water(found)
  if kind == "river" and level > 5 then
    river = found
    break
  end
end
expect(river, "a river")
local kind, surface, depth, east, north = world.water(river)
expect_eq(kind, "river")
expect(depth > 0.3, "a channel deep enough to float in")
expect(math.sqrt(east * east + north * north) > 0.3, "a current")
expect(world.height(river) < surface, "the bed lies below the water")
local lake = world.find_water(here, "lake", 400000)
expect(lake, "a lake")
local lake_kind, lake_surface = world.water(lake)
expect(lake_kind == "lake" or lake_kind == nil, tostring(lake_kind))
if lake_kind then
  expect(lake_surface > 0, "lakes lie above the sea")
end
local sea = world.find_water(here, "ocean", 400000)
expect(sea, "the sea")
local sea_kind, sea_surface, sea_depth = world.water(sea)
expect_eq(sea_kind, "ocean")
expect_eq(sea_surface, 0)
expect(sea_depth > 0)
expect_eq(world.find_water(here, "river", 1), nil, "none within a metre")
-- Set the carpet down on the river: it rides on the water and drifts downstream.
-- (flight replaces game.carpet every tick, so read it afresh)
river.alt = surface + 1
game.carpet.pos, game.carpet.speed, game.carpet.vspeed = river, 0, 0
step(1)
expect(game.carpet.drifting, "carried by the current")
local before = game.carpet.pos
local drifted = 0
for _ = 1, 120 do
  step(1)
  drifted = drifted + (game.carpet.drifting and 1 or 0)
end
expect_eq(drifted, 120, "drifting all along")
local moved = world.distance(before, game.carpet.pos)
expect(moved > 0.5, "it moved " .. moved .. " m in two seconds")
local _, s2 = world.water(game.carpet.pos)
expect(s2 and math.abs(game.carpet.pos.alt - (s2 + 2.5)) < 0.5, "riding on the surface")
