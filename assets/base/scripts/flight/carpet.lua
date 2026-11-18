-- SPDX-License-Identifier: MIT
-- The carpet's flight model: pure functions over a state table. World access
-- (moving over the sphere, terrain height) is passed in, so the model is
-- testable with a flat fake world. All math is the engine's deterministic math,
-- so hosts, clients and replays compute the same flight on every platform.
local M = {}

M.CRUISE_MAX = 60 -- m/s at full throttle
M.BOOST_MAX = 150 -- m/s while boosting
M.ACCEL = 18 -- m/s^2 at full throttle
M.BOOST_ACCEL = 45
M.BRAKE = 35
M.OVERSPEED_DECAY = 0.6 -- 1/s back to cruise speed after boosting
M.TURN_RATE = math.rad(70) -- at full stick, standing still
M.TURN_FALLOFF = 0.45 -- share of the turn rate lost at top speed
M.CLIMB_RATE = 22 -- m/s straight up at full climb input
M.MAX_PITCH = math.rad(28)
M.PITCH_RATE = 2.5 -- 1/s towards the target pitch
M.BANK_MAX = 35 -- degrees
M.BANK_RATE = 4 -- 1/s towards the target bank
M.CLEARANCE = 2.5 -- metres above the ground or the sea
M.CEILING = 6000 -- metres above sea level

local function clamp(v, lo, hi)
  if v < lo then return lo end
  if v > hi then return hi end
  return v
end
M.clamp = clamp

local function approach(v, target, rate, dt)
  local k = rate * dt
  if k > 1 then k = 1 end
  return v + (target - v) * k
end

local function wrap(a)
  return (a + math.pi) % (2 * math.pi) - math.pi
end
M.wrap = wrap

function M.new(pos, heading)
  return {
    pos = pos, heading = heading or 0, speed = 0, pitch = 0, bank = 0, vspeed = 0,
    time = 0, distance = 0, grounded = false, boosting = false, drifting = false,
  }
end

-- inp: { turn = -1..1 (right +), throttle = -1..1, climb = -1..1, boost = bool }
-- env: { move(pos, heading, forward, right, up) -> pos, heading; height(pos) -> metres;
--        water(pos) -> kind?, surface, depth, east, north (optional) }
function M.update(c, inp, dt, env)
  c.time = c.time + dt
  local throttle = clamp(inp.throttle or 0, -1, 1)
  local boosting = inp.boost and throttle >= 0
  local top = boosting and M.BOOST_MAX or M.CRUISE_MAX
  if throttle > 0 then
    local accel = boosting and M.BOOST_ACCEL or M.ACCEL
    c.speed = math.min(c.speed + accel * throttle * dt, math.max(top, c.speed))
  elseif throttle < 0 then
    c.speed = c.speed + M.BRAKE * throttle * dt
  end
  if boosting and throttle == 0 and c.speed < top then
    c.speed = math.min(top, c.speed + M.BOOST_ACCEL * dt)
  end
  if c.speed > top then -- after a boost, ease back to cruising speed
    c.speed = c.speed - (c.speed - top) * math.min(1, M.OVERSPEED_DECAY * dt)
  end
  c.speed = clamp(c.speed, 0, M.BOOST_MAX)
  c.boosting = boosting

  local turn = clamp(inp.turn or 0, -1, 1)
  local rate = M.TURN_RATE * (1 - M.TURN_FALLOFF * c.speed / M.BOOST_MAX)
  c.heading = wrap(c.heading + turn * rate * dt)
  local bank_target = -turn * M.BANK_MAX * math.min(1, c.speed / 20)
  c.bank = approach(c.bank, bank_target, M.BANK_RATE, dt)

  local climb = clamp(inp.climb or 0, -1, 1)
  c.pitch = approach(c.pitch, climb * M.MAX_PITCH, M.PITCH_RATE, dt)
  c.vspeed = climb * M.CLIMB_RATE * (1 - 0.5 * c.speed / M.BOOST_MAX) + c.speed * math.sin(c.pitch) * 0.5
  local forward = c.speed * math.cos(c.pitch) * dt
  local pos, heading = env.move(c.pos, c.heading, forward, 0, c.vspeed * dt)
  c.heading = wrap(heading)
  c.distance = c.distance + forward

  -- Skimming water: the carpet rides on its surface and a river's current carries it.
  local kind, surface, _, east, north
  if env.water then
    kind, surface, _, east, north = env.water(pos)
  end
  local floor = math.max(env.height(pos), kind and surface or 0) + M.CLEARANCE
  -- Riding a current, it follows the water down unless it climbs away.
  if c.drifting and kind and c.vspeed <= 0 and pos.alt > floor then
    pos.alt = floor
  end
  c.grounded = pos.alt <= floor
  -- (positions hold altitudes as floats: resting on the floor reads a hair above it)
  if kind and pos.alt <= floor + 0.05 and (east ~= 0 or north ~= 0) then
    local s, co = math.sin(c.heading), math.cos(c.heading)
    pos, heading = env.move(pos, c.heading, (east * s + north * co) * dt, (east * co - north * s) * dt, 0)
    c.heading = wrap(heading)
    c.drifting = true
  else
    c.drifting = false
  end
  if c.grounded then
    pos.alt = floor
    if c.vspeed < 0 then c.vspeed = 0 end
  elseif pos.alt > M.CEILING then
    pos.alt = M.CEILING
  end
  c.pos = pos
end

-- Velocity in the local frame at the carpet (x east, y up, z south), m/s.
function M.velocity(c)
  local h = c.speed * math.cos(c.pitch)
  return { h * math.sin(c.heading), c.vspeed, -h * math.cos(c.heading) }
end

-- Unit forward direction in the local frame (horizontal).
function M.forward(c)
  return { math.sin(c.heading), 0, -math.cos(c.heading) }
end

-- Model rotation (degrees, Y-X-Z Euler: x = bank, y = yaw, z = pitch) for a
-- model whose nose points along +x.
function M.rotation(c)
  return { c.bank, 90 - math.deg(c.heading), math.deg(c.pitch) }
end

-- The simulation state that replays and co-op desync checks hash.
function M.snapshot(c)
  return {
    face = c.pos.face, x = c.pos.x, y = c.pos.y, lx = c.pos.lx, ly = c.pos.ly, alt = c.pos.alt,
    heading = c.heading, speed = c.speed, pitch = c.pitch, vspeed = c.vspeed, distance = c.distance,
  }
end

return M
