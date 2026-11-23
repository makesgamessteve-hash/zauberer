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
  run(c, { throttle = 1 }, 10, env)
  test.expect(c.speed < 65 and c.speed > carpet.CRUISE_MAX, "after the boost the speed eases back")
  run(c, { throttle = -1 }, 5, env)
  test.expect_eq(c.speed, 0, "braking stops the carpet")
  run(c, { throttle = 0, boost = true }, 1, env)
  test.expect(c.speed > 40, "boost alone accelerates")
  test.expect(c.distance > 0)
end)

-- spec: carpet-flight: Letting go
test.case("letting go settles the carpet", function()
  local env = flat()
  local c = carpet.new({ x = 0, y = 0, alt = 100 }, 0)
  run(c, { throttle = 1 }, 10, env)
  test.expect_near(c.speed, carpet.CRUISE_MAX, 1e-9)
  run(c, { throttle = 0 }, 1.6, env)
  test.expect_eq(c.speed, 0, "it stops within 1.6 s of letting go")
  run(c, { throttle = 0.5 }, 10, env)
  test.expect_near(c.speed, carpet.CRUISE_MAX / 2, 1e-9, "half forward: half speed")
  run(c, { throttle = 1 }, 10, env)
  run(c, { throttle = 0.5 }, 1, env)
  test.expect_near(c.speed, carpet.CRUISE_MAX / 2, 1e-9, "and down to it again")
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
  local t = { day = 3, season = "spring", hour = 14, minute = 5 }
  test.expect_eq(fhud.lines(c, 100, "desktop", { version = "0.1.0" }, t)[4], "flown 2.5 km  day 3 of spring, 14:05")
end)

test.case("spawn on land", function()
  local pos = spawn.find(function(p) return (p.x == 5000 + 37 and p.y == 5000) and 100 or -10 end, 10000)
  test.expect_eq(pos.x, 5037)
  test.expect_eq(pos.face, 0)
  local none = spawn.find(function() return -1 end, 20)
  test.expect_eq(none.x, 10, "no land: the face centre")
end)

-- spec: carpet-flight: The rider's view
test.case("the rider's view", function()
  local cam = camera.new()
  local up = { 0, 1, 0 }
  local eye, target, u = camera.view(cam, { 0, 100, 0 }, 0, up, 0)
  test.expect_near(eye[3], camera.SEAT_BACK, 1e-9, "seated at the back of a carpet heading north")
  test.expect_near(eye[2], 100 + camera.EYE, 1e-9)
  test.expect(target[3] < eye[3] and target[2] < eye[2], "looking ahead and a little down")
  test.expect_near(u[2], 1, 1e-9, "level when the carpet is")
  local _, _, leaning = camera.view(cam, { 0, 100, 0 }, 0, up, 30)
  test.expect(math.abs(leaning[1]) > 0.1 and math.abs(leaning[1]) < math.sin(math.rad(30)), "leans with the bank, less")
  -- The front edge of the carpet (1.7 m ahead, 1 m down) is in view, the back edge (behind the eye) is not.
  local d = camera.direction(cam, 0)
  local front = { 0, -camera.EYE, -(1 + camera.SEAT_BACK) }
  local function angle(a, b)
    local la = math.sqrt(a[1] ^ 2 + a[2] ^ 2 + a[3] ^ 2)
    local lb = math.sqrt(b[1] ^ 2 + b[2] ^ 2 + b[3] ^ 2)
    return math.deg(math.acos((a[1] * b[1] + a[2] * b[2] + a[3] * b[3]) / (la * lb)))
  end
  test.expect(angle(d, front) < 35, "the front edge is within the 70 degree view")
  test.expect(angle(d, { 0, -camera.EYE, 1 - camera.SEAT_BACK }) > 35, "the back edge is out of view")
end)

-- spec: carpet-flight: Looking around
test.case("looking around from the seat", function()
  local cam = camera.new()
  -- Looking around: a stick turns at its rate, the mouse by pixels, both within a seated person's reach.
  camera.update(cam, { x = 1 }, 0.5, false)
  test.expect_near(cam.yaw, camera.LOOK_RATE * 0.5, 1e-9)
  camera.update(cam, { x = 100000, y = 100000 }, 0, true)
  test.expect_eq(cam.yaw, camera.MAX_YAW, "no further than over the shoulder")
  test.expect_eq(cam.pitch, camera.MAX_PITCH, "moving up looks up")
  camera.update(cam, { x = 0, y = 100000 }, 0, true, true)
  test.expect_eq(cam.pitch, camera.MIN_PITCH, "inverted, up looks down")
  -- A mouse's view stays put; a stick's drifts back ahead after a while.
  camera.update(cam, nil, 5, true)
  test.expect_eq(cam.yaw, camera.MAX_YAW)
  for _ = 1, 600 do camera.update(cam, nil, 1 / 60, false) end
  test.expect(math.abs(cam.yaw) < 0.01 and math.abs(cam.pitch - camera.REST_PITCH) < 0.01, "back ahead")
  camera.update(cam, { x = 0, y = -1 }, 10, false)
  test.expect_eq(cam.pitch, camera.MIN_PITCH, "no lower than the carpet")
  camera.reset(cam)
  test.expect_eq(cam.yaw, 0)
end)

-- spec: carpet-flight: Turning to the look
test.case("the carpet flies where the rider looks", function()
  local cam = camera.new()
  test.expect_eq(select(2, camera.steer(cam)), 0, "the resting view flies level")
  local turn
  cam.yaw = math.rad(60)
  turn = camera.steer(cam)
  test.expect_eq(turn, 1, "far off: the full turn")
  cam.yaw = math.rad(15)
  test.expect_near(camera.steer(cam), 0.5, 1e-9, "near: a gentle turn")
  cam.pitch = math.rad(35)
  test.expect_near(select(2, camera.steer(cam)), 1, 1e-9, "35 degrees up climbs at full")
  cam.pitch = math.rad(-20)
  test.expect(select(2, camera.steer(cam)) < 0, "looking down dives")
  -- Flown with the carpet: it swings round to the look, which stays on its bearing.
  local env = flat()
  local c = carpet.new({ x = 0, y = 0, alt = 100 }, 0)
  cam.yaw, cam.pitch = math.rad(60), camera.REST_PITCH
  local bearing = c.heading + cam.yaw
  for _ = 1, 300 do
    local t, climb = camera.steer(cam)
    local before = c.speed
    carpet.update(c, { turn = t, throttle = 1, climb = climb }, 1 / 60, env)
    camera.follow(cam, t * carpet.turn_rate(before) / 60)
  end
  test.expect(math.abs(cam.yaw) < math.rad(2), "the carpet flies where the rider looks")
  test.expect_near(c.heading + cam.yaw, bearing, 0.01, "the view stayed on its bearing")
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
