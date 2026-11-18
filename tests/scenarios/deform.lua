-- SPDX-License-Identifier: MIT
-- Terrain deformation from Lua: dig a crater and raise a mound by the carpet,
-- and the ground, the plants and the active tier's water follow.
-- spec: terrain-deformation: Deformation from Lua
scenario { name = "deform" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if sim.water_at(here).loaded then break end
  sleep(5)
end
expect(sim.water_at(here).loaded, "the carpet's ground is in the active tier")
-- Dry land near the carpet.
local spot
for r = 0, 120, 6 do
  for _, a in ipairs({ 0, 1.6, 3.1, 4.7 }) do
    local p = world.move(here, a, r, 0)
    local c = sim.water_at(p)
    if not spot and c.loaded and not c.fixed and c.depth == 0 then
      spot = p
    end
  end
end
expect(spot, "dry land near the carpet")
-- A crater: deepest at the centre, untouched beyond its rim.
local before = world.ground_height(spot)
local rim = world.move(spot, 0, 30, 0)
local rim_before = world.ground_height(rim)
expect(world.deform(spot, 12, -6) >= 1, "a chunk changed")
expect_near(world.ground_height(spot), before - 6, 0.5)
expect_near(world.ground_height(rim), rim_before, 1e-3)
expect_eq(#world.plants(spot, 8), 0, "the plants were dug up")
expect(sim.water_at(spot).ground < before - 4, "the water sees the crater")
-- A mound on the same place brings it back up.
expect(world.deform(spot, 12, 6) >= 1)
expect_near(world.ground_height(spot), before, 0.5)
-- Out of range and unloaded places.
expect(not pcall(world.deform, spot, 0.1, -1), "too small")
expect(not pcall(world.deform, spot, 100, -1), "too wide")
expect(not pcall(world.deform, spot, 10, 50), "too much")
expect_eq(world.deform(world.move(here, 0, 200000, 0), 10, -2), 0, "nothing loaded there")
