-- SPDX-License-Identifier: MIT
-- Layout on the UI grid (the internal framebuffer, in its pixels, origin top
-- left): the grid and the safe box, boxes at nine anchors, the pointer in grid
-- pixels, what it points at and clicks, and boxes the UI claims against the
-- touch controls (sent once a frame with layout.commit).
local layout = {}

layout.MARGIN = 4 -- grid pixels kept clear at every edge
local insets = { 0, 0, 0, 0 } -- left, top, right, bottom (grid pixels), as a platform reports them
local claims = {}

function layout.grid()
  local g = render.ui_grid()
  return g.width, g.height
end

-- Insets a platform reports (a notch, rounded corners, a home bar), in grid pixels.
function layout.set_insets(left, top, right, bottom)
  insets = { left or 0, top or 0, right or 0, bottom or 0 }
end

-- The safe box: the grid less the insets and the margin.
function layout.safe()
  local w, h = layout.grid()
  local m = layout.MARGIN
  local x, y = insets[1] + m, insets[2] + m
  return { x = x, y = y, w = w - x - insets[3] - m, h = h - y - insets[4] - m }
end

local ANCHORS = {
  top_left = { 0, 0 }, top = { 0.5, 0 }, top_right = { 1, 0 },
  left = { 0, 0.5 }, middle = { 0.5, 0.5 }, right = { 1, 0.5 },
  bottom_left = { 0, 1 }, bottom = { 0.5, 1 }, bottom_right = { 1, 1 },
}

-- A box of w by h at an anchor of the safe box, moved dx, dy inwards from it.
function layout.anchor(where, w, h, dx, dy)
  local a = ANCHORS[where]
  assert(a, "no anchor " .. tostring(where))
  local s = layout.safe()
  dx, dy = dx or 0, dy or 0
  local x = s.x + (s.w - w) * a[1] + (a[1] == 1 and -dx or dx)
  local y = s.y + (s.h - h) * a[2] + (a[2] == 1 and -dy or dy)
  return { x = math.floor(x + 0.5), y = math.floor(y + 0.5), w = w, h = h }
end

-- Boxes of the given sizes laid out from (x, y) down ("down") or across ("across") with a gap.
function layout.stack(x, y, dir, gap, sizes)
  local out = {}
  for _, sz in ipairs(sizes) do
    out[#out + 1] = { x = x, y = y, w = sz[1], h = sz[2] }
    if dir == "across" then x = x + sz[1] + gap else y = y + sz[2] + gap end
  end
  return out
end

-- The pointer in grid pixels (nil when it has been nowhere yet).
function layout.pointer()
  local p = input.pointer()
  if not p.seen then return nil end
  local g = render.ui_grid()
  return { x = (p.x - g.x) / g.w * g.width, y = (p.y - g.y) / g.h * g.height, held = p.held, pressed = p.pressed,
    released = p.released, wheel = p.wheel, touch = p.touch }
end

local function inside(box, x, y)
  return x >= box.x and x < box.x + box.w and y >= box.y and y < box.y + box.h
end

-- The pointer is over the box (a mouse hovering, or a finger held on it).
function layout.hover(box)
  local p = layout.pointer()
  return p ~= nil and inside(box, p.x, p.y) and (not p.touch or p.held)
end

-- The box was clicked or tapped since the last tick.
function layout.clicked(box)
  local p = layout.pointer()
  return p ~= nil and p.pressed and inside(box, p.x, p.y)
end

-- Holds the box against the touch controls (until layout.commit).
function layout.claim(box)
  claims[#claims + 1] = box
end

-- Sends this frame's claims to the input (normalized) and starts the next frame's.
function layout.commit()
  local g = render.ui_grid()
  local rects = {}
  for _, b in ipairs(claims) do
    rects[#rects + 1] = { g.x + b.x / g.width * g.w, g.y + b.y / g.height * g.h, b.w / g.width * g.w,
      b.h / g.height * g.h }
  end
  input.claim(rects)
  claims = {}
  return #rects
end

return layout
