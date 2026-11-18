-- SPDX-License-Identifier: MIT
-- M2: create a small world while its continents drift, fly in it, read what
-- it knows about a place, then load it again (headless; creation runs on a
-- worker, so the scenario waits in real time between ticks).
-- spec: world-creation: A new world from the game
-- spec: world-creation: Places know their past
-- spec: world-creation: Saved worlds come back
scenario { name = "create a world" }
local game = demo.game
expect(not world.info().created, "the noise world to begin with")
expect_eq(world.biome(world.position(0, 100, 100)), nil)
-- Wrong options and calls are refused.
local bad, why = world.create({ quality = "quick", tilt = 80 })
expect(not bad and why == "axial tilt must be 0..45 degrees")
expect_error(function() world.create({ target = "moon" }) end, "invalid option")
expect_eq(world.create_step(), nil)
local no, none = world.create_finish("x")
expect(not no and none == "no finished world")
world.create_cancel() -- nothing to cancel
game.world_options = { radius = 400000, quality = "quick", seed = 11, target = "earth" }
game.set_mode("create")
local screen = game.creation
expect(screen.ok, "creation started: " .. tostring(screen.err))
local twice, busy = world.create({})
expect(not twice and busy == "a world is already being created")
local early, unfinished = world.create_finish("early")
expect(not early and unfinished == "no finished world")
local frames_seen, last_fraction = 0, -1
for _ = 1, 20000 do
  step(1)
  local p = screen.progress
  if p then
    expect(p.fraction >= last_fraction, "progress never goes back")
    last_fraction = p.fraction
    frames_seen = math.max(frames_seen, p.frames)
    if p.done then break end
  end
  sleep(2)
end
expect(screen.progress and screen.progress.done, "the world was finished")
expect_eq(frames_seen, 80, "a time-lapse frame per deep-time step")
local lines = demo.creation.lines(screen)
expect_eq(lines[#lines], "Ready. Press Enter to fly")
-- Enter: fly in it.
input.inject("confirm", 1, 1)
step(1)
input.inject("confirm", 0, 1)
step(2)
expect_eq(game.mode, "flight")
local info = world.info()
expect(info.created, "playing the created world")
expect_eq(info.seed, 11)
expect_eq(info.radius, 400000)
expect_eq(info.quality, "quick")
expect(info.continents >= 1 and info.provinces >= 1)
expect_eq(save.get("world"), "world-" .. tostring(engine.info().seed))
-- The carpet spawned over land of the new world; the place knows its past.
local pos = game.carpet.pos
expect(world.height(pos) > 0, "spawned over land")
expect(type(world.biome(pos)) == "string")
local strata = world.strata(pos)
expect(#strata >= 1 and strata[#strata].environment == "basement", "a rock record down to the basement")
local t, p = world.climate(pos)
expect(t > -60 and t < 50 and p >= 0, "a plausible climate")
local tj = world.climate(pos, 1)
local tl = world.climate(pos, 7)
expect(tj ~= tl, "seasons")
expect(type(world.minerals(pos)) == "table")
local province, isolation = world.province(pos)
expect(province >= 1 and isolation >= 0 and isolation <= 1)
local named, bad_name = world.create_finish("bad name!")
expect(not named and bad_name == "world names use letters, digits, - and _")
-- Fly for a while over the created terrain.
for _ = 1, 120 do
  input.inject("boost", 1, 1)
  step(1)
end
expect(game.carpet.speed > 20)
-- Load it again (as a new session would) and find the same world.
local ok, err = world.load(save.get("world"))
expect(ok, "loaded: " .. tostring(err))
expect_eq(world.info().seed, 11)
expect_eq(world.biome(pos), world.biome(pos))
local missing_ok, missing = world.load("no-such-world")
expect(not missing_ok and missing == "no such world")
local bad_ok, escape = world.load("../escape")
expect(not bad_ok and escape == "world names use letters, digits, - and _")
