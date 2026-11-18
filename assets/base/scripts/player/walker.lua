-- SPDX-License-Identifier: MIT
-- On foot: the mage steps off a carpet resting low and slow, walks and runs
-- over the ground, jumps, stands on ice, and keeps out of water too deep to
-- wade; stepping back onto the carpet flies on. The camera looks out from the
-- eyes.
local walker = {}

walker.WALK = 1.6          -- m/s
walker.RUN = 5.0           -- m/s
walker.ACCEL = 8.0         -- m/s^2
walker.TURN = 2.2          -- rad/s at full stick
walker.MOUSE_TURN = 0.004  -- radians per mouse pixel
walker.GRAVITY = 9.81
walker.JUMP = 4.2          -- m/s up
walker.EYE = 1.7           -- metres above the feet
walker.WADE = 0.9          -- metres of water a mage can wade
walker.STEP = 0.6          -- metres of ground the feet follow down without falling
walker.REACH = 3.0         -- metres to the carpet to step on or off
walker.CARPET_SLOW = 3.0   -- m/s: a carpet slower than this can be left
walker.CARPET_LOW = 4.0    -- metres above the floor

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end
local function wrap(a) return (a + math.pi) % (2 * math.pi) - math.pi end

-- The floor under a place: the ground or the ice on water; and how deep the
-- open water there is. env.height and env.water (water bodies) are used where
-- the active tier has not loaded the place.
function walker.floor(pos, env)
  local ground = env.height(pos)
  local w = sim.water_at(pos)
  if w.loaded then
    if w.frozen then return math.max(ground, w.level), 0, true end
    return ground, w.depth, false
  end
  local kind, surface = env.water(pos)
  if kind and surface > ground then return ground, surface - ground, false end
  return ground, 0, false
end

function walker.new(pos, heading, env)
  local p = env.move(pos, 0, 0, 0, 0)
  p.alt = walker.floor(p, env)
  return { pos = p, heading = heading or 0, pitch = 0, speed = 0, vspeed = 0, grounded = true, blocked = false,
    on_ice = false, landing = nil, time = 0, distance = 0 }
end

-- Whether a carpet can be left here: slow and low over the floor.
function walker.can_step_off(c, env)
  local floor = walker.floor(c.pos, env)
  return c.speed < walker.CARPET_SLOW and c.pos.alt - floor < walker.CARPET_LOW
end

-- Where the mage stands after stepping off a carpet: beside it.
function walker.step_off(c, env)
  local side = env.move(c.pos, c.heading, 0, 1.5, 0)
  local _, depth = walker.floor(side, env)
  if depth > walker.WADE then side = env.move(c.pos, 0, 0, 0, 0) end -- over water: onto the carpet's spot
  return walker.new(side, c.heading, env)
end

-- Whether the carpet is near enough to step onto.
function walker.near_carpet(w, c)
  local flat = world.distance(w.pos, c.pos)
  return math.sqrt(flat * flat + (c.pos.alt - w.pos.alt) ^ 2) <= walker.REACH
end

-- inp: turn and throttle (-1 .. 1), run and jump (booleans), look (mouse or
-- stick: x turns, y tilts the view), mouse (look is in pixels).
-- env: move(pos, heading, forward, right, up), height(pos), water(pos).
function walker.update(w, inp, dt, env)
  w.time = w.time + dt
  w.landing = nil
  local look = inp.look or {}
  local lx, ly = look.x or 0, look.y or 0
  if inp.mouse then
    w.heading = w.heading + lx * walker.MOUSE_TURN
    w.pitch = clamp(w.pitch - ly * walker.MOUSE_TURN, -1.2, 1.2)
  else
    w.heading = w.heading + lx * walker.TURN * dt
    w.pitch = clamp(w.pitch - ly * walker.TURN * 0.5 * dt, -1.2, 1.2)
  end
  w.heading = wrap(w.heading + clamp(inp.turn or 0, -1, 1) * walker.TURN * dt)
  local want = clamp(inp.throttle or 0, -1, 1) * (inp.run and walker.RUN or walker.WALK)
  local dv = want - w.speed
  w.speed = w.speed + clamp(dv, -walker.ACCEL * dt, walker.ACCEL * dt)
  if inp.jump and w.grounded then
    w.vspeed, w.grounded = walker.JUMP, false
  end
  if not w.grounded then w.vspeed = w.vspeed - walker.GRAVITY * dt end
  local pos, heading = env.move(w.pos, w.heading, w.speed * dt, 0, w.vspeed * dt)
  local floor, depth, ice = walker.floor(pos, env)
  w.blocked = depth > walker.WADE
  if w.blocked then -- too deep to wade: the mage stays on the shore
    local alt = pos.alt
    pos, heading = env.move(w.pos, w.heading, 0, 0, 0), w.heading
    pos.alt = alt
    local _
    floor, _, ice = walker.floor(pos, env)
    w.speed = 0
  else
    w.distance = w.distance + math.abs(w.speed * dt)
  end
  w.heading = wrap(heading)
  if pos.alt <= floor then
    if not w.grounded then w.landing = -w.vspeed end
    pos.alt, w.vspeed, w.grounded = floor, 0, true
  elseif w.grounded then
    if pos.alt - floor <= walker.STEP then
      pos.alt = floor -- down a slope
    else
      w.grounded = false -- off an edge
    end
  end
  w.on_ice = ice and w.grounded
  w.pos = pos
end

-- The eye and the point looked at (render coordinates).
function walker.view(w)
  local eye = world.to_local(w.pos)
  eye[2] = eye[2] + walker.EYE
  local c = math.cos(w.pitch)
  local dir = { math.sin(w.heading) * c, math.sin(w.pitch), -math.cos(w.heading) * c }
  return eye, { eye[1] + dir[1] * 10, eye[2] + dir[2] * 10, eye[3] + dir[3] * 10 }
end

-- Where the hands are, for weaving on foot (a stand-in carpet for magic).
function walker.hands(w)
  local p = world.move(w.pos, w.heading, 0.3, 0, 1.3)
  return { pos = p, heading = w.heading, speed = w.speed, pitch = w.pitch }
end

function walker.snapshot(w)
  return { pos = w.pos, heading = w.heading }
end

return walker
