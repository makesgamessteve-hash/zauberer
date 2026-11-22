-- SPDX-License-Identifier: MIT
-- The rider's view: out of the heir's eyes, seated at the back of the carpet.
-- Only the carpet's front shows, low in the picture; the rider looks around
-- (within what a seated person can turn) and the view leans a little with
-- the carpet's bank. In render coordinates.
local M = {}

M.SEAT_BACK = 0.7     -- metres behind the carpet's middle (it is 2 m long)
M.EYE = 1.0           -- metres above the carpet
M.REST_PITCH = math.rad(-11) -- looking ahead and a little down
M.MAX_YAW = math.rad(115)    -- over the shoulder, no further
M.MIN_PITCH, M.MAX_PITCH = math.rad(-57), math.rad(40)
M.LOOK_RATE = 2.0     -- radians per second at full stick
M.MOUSE_RATE = 0.004  -- radians per mouse pixel
M.RETURN_AFTER = 1.5  -- seconds without looking before a stick's view drifts back ahead
M.RETURN_RATE = 2.0   -- 1/s back towards ahead
M.LEAN = 0.4          -- share of the carpet's bank the view takes

function M.new()
  return { yaw = 0, pitch = M.REST_PITCH, idle = 0 }
end

local function clamp(v, lo, hi) return math.max(lo, math.min(hi, v)) end

-- look: { x, y } from the look action; dt: frame time; mouse: the mouse looks
-- (it keeps where it was put); invert: up is down.
function M.update(cam, look, dt, mouse, invert)
  local lx, ly = look and look.x or 0, look and look.y or 0
  if invert then ly = -ly end
  local rate = mouse and M.MOUSE_RATE or M.LOOK_RATE * dt
  cam.yaw = clamp(cam.yaw + lx * rate, -M.MAX_YAW, M.MAX_YAW)
  cam.pitch = clamp(cam.pitch - ly * rate, M.MIN_PITCH, M.MAX_PITCH)
  if lx ~= 0 or ly ~= 0 then
    cam.idle = 0
  else
    cam.idle = cam.idle + dt
    if not mouse and cam.idle > M.RETURN_AFTER then
      local k = math.min(1, M.RETURN_RATE * dt)
      cam.yaw = cam.yaw * (1 - k)
      cam.pitch = cam.pitch + (M.REST_PITCH - cam.pitch) * k
    end
  end
end

local function add(a, b, k)
  return { a[1] + b[1] * k, a[2] + b[2] * k, a[3] + b[3] * k }
end

local function cross(a, b)
  return { a[2] * b[3] - a[3] * b[2], a[3] * b[1] - a[1] * b[3], a[1] * b[2] - a[2] * b[1] }
end

local function normalize(v)
  local l = math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
  return l > 0 and { v[1] / l, v[2] / l, v[3] / l } or v
end

-- The unit direction looked in for a carpet heading h (render axes).
function M.direction(cam, heading)
  local h = heading + cam.yaw
  local cp = math.cos(cam.pitch)
  return { math.sin(h) * cp, math.sin(cam.pitch), -math.cos(h) * cp }
end

-- Where the eye is: above the back of the carpet at local position p.
function M.eye(p, heading, up)
  local fwd = { math.sin(heading), 0, -math.cos(heading) }
  return add(add(p, fwd, -M.SEAT_BACK), up, M.EYE)
end

-- Eye, target and up for a carpet at local position p with heading h, up u
-- and bank (degrees, as the carpet keeps it).
function M.view(cam, p, heading, up, bank)
  local eye = M.eye(p, heading, up)
  local dir = M.direction(cam, heading)
  local right = normalize(cross(dir, up))
  local lean = math.rad(bank or 0) * M.LEAN
  local u = normalize(add(add({ 0, 0, 0 }, up, math.cos(lean)), right, math.sin(lean)))
  return eye, add(eye, dir, 10), u
end

-- Looking ahead again (after a teleport, a revival or mounting).
function M.reset(cam)
  cam.yaw, cam.pitch, cam.idle = 0, M.REST_PITCH, 0
end

return M
