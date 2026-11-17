-- SPDX-License-Identifier: MIT
-- Injected actions drive the demo state.
-- spec: input-actions: Button held across ticks
scenario { name = "input drives the demo" }
local s = demo.game.state
input.inject("spin", 1, 0, 30)
step(30)
expect_eq(s.spins, 1, "a held button is pressed once")
expect(s.spin > 0, "the carpet spins while the button is held")
input.inject("chime")
step(1)
expect_eq(s.chimes, 1)
local yaw = s.yaw
input.inject("turn", 1, 0, 60)
step(60)
expect_near(s.yaw, (yaw + 90) % 360, 1e-6, "full stick turns 90 degrees per second")
input.inject("credits")
step(1)
expect_eq(s.credits, true)
expect(#demo.game.credits >= 5, "credits are listed")
step(1)
