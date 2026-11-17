-- SPDX-License-Identifier: MIT
-- spec: game-loop: Paused game keeps rendering
scenario { name = "paused game keeps rendering" }
step(10)
local ticks, frames = engine.tick(), render.stats().frames
engine.pause(true)
step(30)
expect_eq(engine.tick(), ticks, "no ticks while paused")
expect(render.stats().frames >= frames + 30, "frames continue while paused")
engine.pause(false)
step(5)
expect_eq(engine.tick(), ticks + 5)
