-- SPDX-License-Identifier: MIT
-- Chase camera behind and above the carpet, in render coordinates.
local M = {}

M.DISTANCE = 9 -- metres behind
M.HEIGHT = 3.2 -- metres above
M.LOOK_AHEAD = 12 -- metres in front, the point looked at
M.SMOOTHING = 6 -- 1/s
M.LOOK_RATE = 2.0 -- radians per second of orbit at full look input
M.MOUSE_RATE = 0.004 -- radians per mouse pixel

function M.new()
  return { orbit = 0, eye = nil }
end

-- look: { x, y } from the look action; dt: frame time.
function M.update(cam, look, dt, mouse)
  local lx = look and look.x or 0
  if mouse then
    cam.orbit = cam.orbit + lx * M.MOUSE_RATE
  else
    cam.orbit = cam.orbit + lx * M.LOOK_RATE * dt
  end
  cam.orbit = (cam.orbit + math.pi) % (2 * math.pi) - math.pi
end

local function add(a, b, k)
  return { a[1] + b[1] * k, a[2] + b[2] * k, a[3] + b[3] * k }
end

-- Eye and target for a carpet at local position p with heading h and up u.
-- The smoothing works on the eye's offset from the carpet, so a move of the
-- render origin needs no special handling.
function M.view(cam, p, heading, up, dt)
  local h = heading + cam.orbit
  local back = { -math.sin(h), 0, math.cos(h) }
  local fwd = { math.sin(heading), 0, -math.cos(heading) }
  local offset = add(add({ 0, 0, 0 }, back, M.DISTANCE), up, M.HEIGHT)
  if cam.eye and dt then
    local k = math.min(1, M.SMOOTHING * dt)
    offset = { cam.eye[1] + (offset[1] - cam.eye[1]) * k, cam.eye[2] + (offset[2] - cam.eye[2]) * k,
      cam.eye[3] + (offset[3] - cam.eye[3]) * k }
  end
  cam.eye = offset
  return add(p, offset, 1), add(p, fwd, M.LOOK_AHEAD)
end

-- Forgets the smoothing (after a teleport).
function M.reset(cam)
  cam.eye = nil
end

return M
