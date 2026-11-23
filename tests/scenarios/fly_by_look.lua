-- SPDX-License-Identifier: MIT
-- The carpet flies where the rider looks: hovering, the rider looks around
-- and the carpet stays; holding forward, it swings round to the look, which
-- stays on its bearing; looking up climbs; letting go settles it.
-- spec: carpet-flight: Looking around while hovering
-- spec: carpet-flight: Turning to the look
scenario { name = "fly by look" }
local game = demo.game
local camera = require("flight.camera")
step(5)
local c = game.carpet
c.speed = 0
step(2)
local h0 = c.heading
-- Hovering: a look 90 degrees to the left does not turn the carpet.
input.inject("look", -math.rad(90) / camera.MOUSE_RATE, 0, 1)
step(30)
expect(math.abs(game.rider.yaw + math.rad(90)) < 1e-6, "the rider looks left: " .. game.rider.yaw)
expect(math.abs(game.carpet.heading - h0) < 1e-3, "hovering, the carpet stays")
-- Looking 60 degrees right of the heading and holding forward: it swings round.
camera.reset(game.rider)
input.inject("look", math.rad(60) / camera.MOUSE_RATE, 0, 1)
step(1)
c = game.carpet
local bearing = c.heading + game.rider.yaw
input.inject("steer", 0, 1, 240)
step(240)
c = game.carpet
expect(c.speed > 30, "forward flies: " .. c.speed)
expect(math.abs(game.rider.yaw) < math.rad(3), "it flies where the rider looks: " .. math.deg(game.rider.yaw))
local d = ((c.heading + game.rider.yaw - bearing + math.pi) % (2 * math.pi)) - math.pi
expect(math.abs(d) < math.rad(3), "the view stayed on its bearing: " .. math.deg(d))
-- Looking up while flying climbs.
local alt = c.pos.alt
input.inject("look", 0, math.rad(45) / camera.MOUSE_RATE, 1)
input.inject("steer", 0, 1, 120)
step(120)
expect(game.carpet.pos.alt > alt + 20, "looking up climbs")
-- Letting go: it settles within 1.6 s.
step(96)
expect_eq(game.carpet.speed, 0, "let go, it stops")
