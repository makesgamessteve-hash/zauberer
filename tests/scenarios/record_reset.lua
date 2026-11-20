-- SPDX-License-Identifier: MIT
-- The reset key pressed twice (recorded for the engine test that replays it in
-- play, tests/engine/test_headless.py). In a scenario the game cannot start
-- over: the first press asks, the second says why not.
-- spec: world-persistence: The reset key asks first
scenario { name = "record the reset key" }
local game = demo.game
step(30)
input.inject("reset")
step(2)
expect(game.reset.asked ~= nil, "the first press asks")
step(30)
input.inject("reset")
step(2)
expect(game.reset.asked == nil, "the second press answers")
expect_eq(game.reset.refused, "only in play")
step(60)
