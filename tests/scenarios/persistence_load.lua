-- SPDX-License-Identifier: MIT
-- Second half of "Save and continue": the game starts where it was saved,
-- with the same time, carpet, edits and bodies.
-- spec: world-persistence: Save and continue
-- spec: rigid-bodies: Bodies are kept with the game
scenario { name = "persistence load" }
local game = demo.game
local want = save.get("expect")
expect(want, "the first half ran")
expect_near(world.time().time, want.time, 1, "the time came back")
expect_near(game.carpet.heading, 1.25, 1e-9, "the carpet came back")
local here = want.pos
expect(world.distance(here, game.carpet.pos) < 1, "where it was saved")
for _ = 1, 900 do
  step(1)
  if world.chunk_stage(here) == 3 and world.ground_surface(here) then break end
  sleep(5)
end
expect_eq(#world.plants(here, 60), want.plants, "the removed plants stay removed")
expect_eq(world.ground_surface(here), "rock", "the painted ground stays painted")
local near = sim.bodies_near(here, 400)
expect_eq(#near, want.bodies or 0, "the bodies came back") -- saves from before bodies have none
if want.bodies then
  local crate = sim.body(near[1])
  expect_eq(crate.shape, "box")
  expect_eq(crate.material, "base:wood")
  expect_near(crate.size[2], 2, 1e-9)
end
local missing, why = world.load_game("nothing-here")
expect(not missing and why == "no such save")
