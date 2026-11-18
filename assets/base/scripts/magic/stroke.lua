-- SPDX-License-Identifier: MIT
-- Drawing a gesture (concept 3.3, 8.5): while the draw button is held, the
-- pointer's movement (mouse motion, or the right stick and finger drags
-- integrated over time) traces a stroke; on release the stroke is recognised.
-- The trail is kept for the HUD to draw.
local stroke = {}

local MAX_POINTS = 1024
local STICK_SPEED = 600 -- trail units per second at full stick

function stroke.new(options)
  return { points = {}, x = 0, y = 0, drawing = false, options = options or {}, last = nil }
end

-- Feeds one tick: held (the draw button), dx, dy (pointer motion this tick in
-- trail units) and, for sticks, sx, sy (deflection -1 .. 1) with dt.
-- Returns the recognised gesture on the tick the button is let go, else nil.
function stroke.update(s, held, dx, dy, sx, sy, dt)
  if held then
    if not s.drawing then
      s.drawing, s.points, s.x, s.y = true, { 0, 0 }, 0, 0
    end
    local mx = (dx or 0) + (sx or 0) * STICK_SPEED * (dt or 0)
    local my = (dy or 0) + (sy or 0) * STICK_SPEED * (dt or 0)
    if (mx ~= 0 or my ~= 0) and #s.points < 2 * MAX_POINTS then
      s.x, s.y = s.x + mx, s.y + my
      s.points[#s.points + 1] = s.x
      s.points[#s.points + 1] = s.y
    end
    return nil
  end
  if not s.drawing then
    return nil
  end
  s.drawing = false
  s.last = gesture.recognize(s.points, s.options)
  return s.last
end

-- The trail as screen points around (cx, cy), scaled to fit within radius.
function stroke.trail(s, cx, cy, radius)
  local out, n = {}, #s.points // 2
  if n == 0 then return out end
  local lo_x, hi_x, lo_y, hi_y = math.huge, -math.huge, math.huge, -math.huge
  for i = 1, n do
    local x, y = s.points[2 * i - 1], s.points[2 * i]
    lo_x, hi_x = math.min(lo_x, x), math.max(hi_x, x)
    lo_y, hi_y = math.min(lo_y, y), math.max(hi_y, y)
  end
  local span = math.max(hi_x - lo_x, hi_y - lo_y, 1)
  local k = math.min(1, 2 * radius / span)
  local mx, my = (lo_x + hi_x) / 2, (lo_y + hi_y) / 2
  local every = math.max(1, n // 48)
  for i = 1, n, every do
    out[#out + 1] = { cx + (s.points[2 * i - 1] - mx) * k, cy + (s.points[2 * i] - my) * k }
  end
  return out
end

return stroke
