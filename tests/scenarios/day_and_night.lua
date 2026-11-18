-- SPDX-License-Identifier: MIT
-- Time passes in flight: the date advances with the ticks, noon brings the
-- sun up over the carpet and midnight takes it down (headless).
-- spec: sky-and-time: Time passes in the game
-- spec: sky-and-time: Day and night from Lua
scenario { name = "day and night" }
local game = demo.game
local hud = require("flight.hud")
local t0 = world.time()
expect_eq(t0.year, 0)
expect_eq(t0.season, "spring")
expect_eq(t0.day, 1)
-- A new world begins in the morning where the carpet starts.
local sky = world.sky(game.carpet.pos)
expect(sky.sun_elevation > 0 and sky.sun_elevation < 1.2, "morning sun: " .. sky.sun_elevation)
step(60) -- one real second: 36 game seconds
expect_near(world.time().time - t0.time, 36, 0.01)
-- Local noon and midnight at the carpet.
local x, y = world.to_cartesian(game.carpet.pos)
local noon = (math.pi - math.atan(y, x)) / (2 * math.pi) % 1 * 86400 + 86400
world.set_time(noon)
sky = world.sky(game.carpet.pos)
expect(sky.sun_elevation > 0.7 and sky.daylight == 1, "the sun is high at noon")
world.set_time(noon + 43200)
sky = world.sky(game.carpet.pos)
expect(sky.sun_elevation < -0.5 and sky.daylight == 0, "night at midnight")
expect(sky.moon_phase >= 0 and sky.moon_phase < 1 and sky.moon_light >= 0 and sky.moon_light <= 1)
expect(sky.season >= 0 and sky.season < 4)
-- Months later: summer, and the HUD shows it.
world.set_time(86400 * 14 + 15 * 3600 + 7 * 60)
local t = world.time()
expect_eq(t.season, "summer")
expect_eq(t.day, 3)
expect_eq(t.hour, 15)
expect_eq(t.minute, 7)
local lines = hud.lines(game.carpet, 0, "desktop", engine.info(), t)
expect(lines[4]:find("day 3 of summer, 15:07", 1, true) ~= nil, lines[4])
expect_error(function() world.set_time(-1) end, "time must not be negative")
expect_error(function() render.daylight("yes") end, "boolean expected")
