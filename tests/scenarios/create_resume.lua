-- SPDX-License-Identifier: MIT
-- Second run after create_world.lua: the game starts in the world made last
-- time. N opens the new-world screen and Escape leaves it again.
-- spec: world-creation: Saved worlds come back
scenario { name = "resume a created world" }
local game = demo.game
local info = world.info()
expect(info.created, "the saved world was loaded at start")
expect_eq(info.seed, 11)
expect_eq(game.mode, "flight")
expect(world.height(game.carpet.pos) > 0, "spawned over its land")
input.inject("new_world", 1, 1)
step(1)
expect_eq(game.mode, "create")
expect(game.creation.ok, tostring(game.creation.err))
step(3)
input.inject("quit", 1, 1)
step(1)
expect_eq(game.mode, "flight", "Escape leaves the new-world screen")
expect(not game.creation.ok, "and abandons the world being made")
local again, err = world.create({ quality = "quick", radius = 300000 })
expect(again, "nothing is being created any more: " .. tostring(err))
world.create_cancel()
-- A created world travels to joining heirs as its bundle's bytes.
-- spec: coop-sessions: The world sent to joiners
local bytes = world.export_bundle()
expect(type(bytes) == "string" and #bytes > 1000, "the bundle as bytes")
expect(world.play_bundle(bytes), "played from the bytes")
expect_eq(world.info().seed, 11)
local bad, why = world.play_bundle("not a world")
expect(not bad and why, "other bytes are refused")
