-- SPDX-License-Identifier: MIT
-- Unit tests of the flight game modules (assets/base/scripts/flight) on a flat fake world.
local carpet = require("flight.carpet")
local controls = require("flight.controls")
local camera = require("flight.camera")
local fhud = require("flight.hud")
local spawn = require("flight.spawn")

local DT = 1 / 60

-- A flat world: x east, y north (metres), alt up; heading stays as it is.
local function flat(height)
  return {
    move = function(pos, heading, forward, right, up)
      return {
        x = pos.x + forward * math.sin(heading) + right * math.cos(heading),
        y = pos.y + forward * math.cos(heading) - right * math.sin(heading),
        alt = pos.alt + up,
      }, heading
    end,
    height = height or function() return 0 end,
  }
end

local function run(c, inp, seconds, env)
  for _ = 1, math.floor(seconds * 60 + 0.5) do
    carpet.update(c, inp, DT, env)
  end
end

-- spec: carpet-flight: Speeds
test.case("throttle, boost and brake", function()
  local env = flat()
  local c = carpet.new({ x = 0, y = 0, alt = 100 }, 0)
  run(c, { throttle = 1 }, 10, env)
  test.expect_near(c.speed, carpet.CRUISE_MAX, 1e-9, "full throttle cruises at 60 m/s")
  run(c, { throttle = 1, boost = true }, 5, env)
  test.expect_near(c.speed, carpet.BOOST_MAX, 1e-9, "boost reaches 150 m/s")
  test.expect(c.boosting)
  run(c, { throttle = 0 }, 10, env)
  test.expect(c.speed < 65 and c.speed > carpet.CRUISE_MAX, "after the boost the speed eases back")
  run(c, { throttle = -1 }, 5, env)
  test.expect_eq(c.speed, 0, "braking stops the carpet")
  run(c, { throttle = 0, boost = true }, 1, env)
  test.expect(c.speed > 40, "boost alone accelerates")
  test.expect(c.distance > 0)
end)

-- spec: carpet-flight: Turning
test.case("turning and banking", function()
  local env = flat()
  local c = carpet.new({ x = 0, y = 0, alt = 100 }, 0)
  run(c, { turn = 1 }, 1, env)
  test.expect_near(math.deg(c.heading), 70, 1e-6, "70 degrees per second at a standstill")
  c.speed = carpet.BOOST_MAX
  local before = c.heading
  run(c, { turn = 1, throttle = 0, boost = true }, 0.5, env)
  test.expect(math.deg(c.heading - before) < 20, "turning is slower at speed")
  test.expect(c.bank < -20, "the carpet banks into the turn")
  c.heading = math.pi - 0.01
  run(c, { turn = 1 }, 0.5, env)
  test.expect(c.heading < 0, "heading wraps around")
  test.expect_near(carpet.wrap(3 * math.pi), -math.pi, 1e-9, "headings lie in [-pi, pi)")
  test.expect(carpet.wrap(-3 * math.pi + 0.1) > -math.pi)
end)

-- spec: carpet-flight: Climb and ground
test.case("climbing, ground clearance and ceiling", function()
  local hill = function(pos) return pos.x > 500 and 300 or 20 end
  local env = flat(hill)
  local c = carpet.new({ x = 0, y = 0, alt = 50 }, math.pi / 2) -- east
  run(c, { climb = 1 }, 2, env)
  test.expect(c.pos.alt > 50 + 30, "climbs at full input")
  run(c, { climb = -1 }, 20, env)
  test.expect_eq(c.pos.alt, 20 + carpet.CLEARANCE, "never below the ground")
  test.expect(c.grounded)
  test.expect_eq(c.vspeed, 0)
  c.speed = 60
  run(c, { throttle = 1 }, 15, env)
  test.expect(c.pos.x > 500)
  test.expect(c.pos.alt >= 300 + carpet.CLEARANCE, "slides up a cliff instead of passing through")
  local sea = carpet.new({ x = 0, y = 0, alt = 10 }, 0)
  run(sea, { climb = -1 }, 5, flat(function() return -200 end))
  test.expect_eq(sea.pos.alt, carpet.CLEARANCE, "skims the sea surface")
  local high = carpet.new({ x = 0, y = 0, alt = carpet.CEILING - 1 }, 0)
  run(high, { climb = 1 }, 2, env)
  test.expect_eq(high.pos.alt, carpet.CEILING)
end)

-- spec: carpet-flight: Snapshot
test.case("state views", function()
  local c = carpet.new({ face = 1, x = 2, y = 3, lx = 4, ly = 5, alt = 6 }, math.pi / 2)
  c.speed = 10
  local v = carpet.velocity(c)
  test.expect_near(v[1], 10, 1e-9)
  test.expect_near(v[3], 0, 1e-9)
  local f = carpet.forward(c)
  test.expect_near(f[1], 1, 1e-12)
  local r = carpet.rotation(c)
  test.expect_near(r[2], 0, 1e-9, "a carpet heading east keeps the model's +x nose")
  local snap = carpet.snapshot(c)
  test.expect_eq(snap.face, 1)
  test.expect_eq(snap.speed, 10)
  test.expect_eq(carpet.clamp(5, 0, 1), 1)
end)

-- spec: touch-and-gamepad-controls: Response curve
test.case("response curve and input mapping", function()
  test.expect_eq(controls.curve(0), 0)
  test.expect_near(controls.curve(0.5), 0.5 ^ controls.EXPO, 1e-12)
  test.expect_near(controls.curve(-0.5), -(0.5 ^ controls.EXPO), 1e-12)
  test.expect_eq(controls.curve(2), 1)
  test.expect_eq(controls.curve(1), 1)
  local inp = controls.flight_input({ steer = { x = 1, y = -1 }, climb = { x = 0.5 }, boost = { held = true } })
  test.expect_eq(inp.turn, 1)
  test.expect_eq(inp.throttle, -1)
  test.expect_near(inp.climb, controls.curve(0.5), 1e-12)
  test.expect(inp.boost)
  local touch = controls.flight_input({ climb = { x = 0 }, climb_touch = { x = 0, y = -1 } })
  test.expect_eq(touch.climb, -1, "the touch climb stick is used when the climb axis rests")
  local empty = controls.flight_input({})
  test.expect_eq(empty.turn, 0)
  test.expect_eq(empty.boost, false)
end)

test.case("heads-up text", function()
  test.expect_eq(fhud.compass(0), "N")
  test.expect_eq(fhud.compass(math.rad(90)), "E")
  test.expect_eq(fhud.compass(math.rad(-45)), "NW")
  local c = carpet.new({ alt = 120 }, math.rad(180))
  c.speed, c.boosting, c.distance = 150, true, 2500
  local lines = fhud.lines(c, 100, "desktop", { version = "0.1.0" })
  test.expect(lines[2]:find("540 km/h BOOST", 1, true) ~= nil)
  test.expect(lines[2]:find("180 S", 1, true) ~= nil)
  test.expect(lines[3]:find("above ground 20 m", 1, true) ~= nil)
  test.expect_eq(lines[4], "flown 2.5 km")
  test.expect(fhud.lines(c, -50, "touch", { version = "x" })[5]:find("BOOST", 1, true) ~= nil)
end)

test.case("spawn on land", function()
  local pos = spawn.find(function(p) return (p.x == 5000 + 37 and p.y == 5000) and 100 or -10 end, 10000)
  test.expect_eq(pos.x, 5037)
  test.expect_eq(pos.face, 0)
  local none = spawn.find(function() return -1 end, 20)
  test.expect_eq(none.x, 10, "no land: the face centre")
end)

test.case("chase camera", function()
  local cam = camera.new()
  local eye, target = camera.view(cam, { 0, 100, 0 }, 0, { 0, 1, 0 })
  test.expect_near(eye[3], camera.DISTANCE, 1e-9, "behind a carpet heading north")
  test.expect_near(eye[2], 100 + camera.HEIGHT, 1e-9)
  test.expect_near(target[3], -camera.LOOK_AHEAD, 1e-9)
  local turned = camera.view(cam, { 0, 100, 0 }, math.pi / 2, { 0, 1, 0 }, 1 / 60)
  test.expect(turned[1] > -camera.DISTANCE and turned[1] < 0, "the eye follows smoothly")
  camera.update(cam, { x = 1 }, 0.5, false)
  test.expect_near(cam.orbit, camera.LOOK_RATE * 0.5, 1e-9)
  camera.update(cam, { x = 100 }, 0, true)
  test.expect_near(cam.orbit, camera.LOOK_RATE * 0.5 + 100 * camera.MOUSE_RATE, 1e-9)
  camera.update(cam, nil, 1, false)
  camera.reset(cam)
  test.expect_eq(cam.eye, nil)
end)

-- spec: carpet-flight: Deterministic math
test.case("engine math is deterministic", function()
  test.expect_eq(string.format("%.17g", math.sin(0.5)), "0.47942553860420301")
  test.expect_eq(string.format("%.17g", math.atan(1, 2)), "0.46364760900080609")
  test.expect_near(2 ^ 0.5, math.sqrt(2), 1e-15)
  test.expect_eq(3 ^ 2, 9)
  test.expect_near(math.log(8, 2), 3, 1e-15)
  test.expect_near(math.exp(math.log(5)), 5, 1e-14)
  test.expect_near(math.asin(1), math.pi / 2, 1e-15)
  test.expect_near(math.acos(-1), math.pi, 1e-15)
  test.expect_near(math.tan(math.pi / 4), 1, 1e-15)
  test.expect_near(math.cos(0), 1, 0)
  test.expect_near(math.atan(1), math.pi / 4, 1e-15)
end)
