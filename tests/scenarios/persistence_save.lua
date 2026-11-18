-- SPDX-License-Identifier: MIT
-- First half of "Save and continue" (persistence_load.lua is the second): edit
-- the world around the carpet, drop a crate, set the time, and save the game.
-- spec: world-persistence: Save and continue
-- spec: rigid-bodies: Bodies are kept with the game
scenario { name = "persistence save" }
local game = demo.game
local here = game.carpet.pos
for _ = 1, 900 do
  step(1)
  if world.chunk_stage(here) == 3 and #world.plants(here, 60) > 0 then break end
  sleep(5)
end
local before = #world.plants(here, 60)
expect(before > 0, "plants around the carpet")
local removed = world.remove_plants(here, 40)
expect(removed > 0, "plants removed")
expect_eq(#world.plants(here, 60), before - removed)
expect_eq(world.remove_plants(here, 40), 0, "nothing left to remove there")
world.set_surface(here, "rock")
expect_eq(world.ground_surface(here), "rock")
expect_error(function() world.set_surface(here, "lava") end, "invalid option")
expect_error(function() world.remove_plants(here, 999) end, "radius must be 0..256 metres")
local crate_at = world.move(here, 0, 12, 0)
crate_at.alt = world.ground_height(crate_at) + 0.6
sim.spawn { shape = "box", material = "wood", pos = crate_at, size = { 1, 2, 0.5 } }
world.set_time(86400 * 30 + 3600 * 15)
game.carpet.heading = 1.25
expect(game.save(), "saved")
local names = world.saves()
expect_eq(names[1], "default")
local bad, why = world.save_game("no way", {})
expect(not bad and why == "save names use letters, digits, - and _")
save.set("expect", { plants = #world.plants(here, 60), time = world.time().time, pos = here, bodies = 1 })
expect(save.flush())
