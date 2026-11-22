-- SPDX-License-Identifier: MIT
-- Flight controls: reads the input actions of every device (keyboard and
-- mouse, gamepad, touch) and shapes them with response curves.
local M = {}

M.EXPO = 1.6 -- stick response: small deflections give fine control
M.ACTIONS = { "steer", "climb", "climb_touch", "boost", "look", "chime", "credits", "fullscreen", "quit", "new_world" }

-- On-screen controls in touch mode (normalised screen coordinates).
M.TOUCH = {
  sticks = {
    { name = "steer", x = 0.16, y = 0.74, radius = 0.13 },
    { name = "climb", x = 0.86, y = 0.70, radius = 0.11 },
  },
  buttons = {
    { name = "boost", x = 0.62, y = 0.76, w = 0.13, h = 0.18 },
    { name = "chime", x = 0.62, y = 0.52, w = 0.13, h = 0.14 },
    { name = "menu", x = 0.68, y = 0.04, w = 0.14, h = 0.12 },
    { name = "credits", x = 0.84, y = 0.04, w = 0.14, h = 0.12 },
    { name = "confirm", x = 0.44, y = 0.76, w = 0.13, h = 0.18 },
  },
}

-- What each on-screen control is called where it is drawn.
M.LABELS = { steer = "Steer", climb = "Climb", boost = "Boost", chime = "Chime", menu = "Menu", credits = "Credits",
  confirm = "OK" }

-- Response curve: sign-preserving power with a dead zone already applied by the engine.
function M.curve(v)
  if v == 0 then return 0 end
  local a = math.abs(v)
  if a > 1 then a = 1 end
  local s = a ^ M.EXPO
  return v < 0 and -s or s
end

-- Turns raw action states into flight input.
function M.flight_input(states)
  local steer = states.steer or {}
  local climb = (states.climb and states.climb.x) or 0
  if climb == 0 and states.climb_touch then
    climb = states.climb_touch.y or 0
  end
  local boost = states.boost or {}
  return {
    turn = M.curve(steer.x or 0),
    throttle = M.curve(steer.y or 0),
    climb = M.curve(climb),
    boost = boost.held or false,
  }
end

function M.read()
  local states = {}
  for _, name in ipairs(M.ACTIONS) do
    states[name] = input.state(name)
  end
  return states
end

-- The labels of the on-screen controls at their places, for a screen of
-- w x h pixels: { text, x, y } (sticks in round brackets, buttons in square).
function M.touch_marks(w, h)
  local i18n = require("ui.i18n")
  local out = {}
  local function mark(open, name, cx, cy, close)
    local label = open .. i18n.t(M.LABELS[name] or name) .. close
    out[#out + 1] = { text = label, x = math.floor(cx * w - #label * 3), y = math.floor(cy * h - 4) }
  end
  for _, st in ipairs(M.TOUCH.sticks) do mark("( ", st.name, st.x, st.y, " )") end
  for _, b in ipairs(M.TOUCH.buttons) do mark("[", b.name, b.x + b.w / 2, b.y + b.h / 2, "]") end
  return out
end

-- Draws the on-screen controls' labels over the game (touch mode).
function M.draw_touch(font, color)
  local h = render.internal_height()
  local d = engine.info().drawable_size
  local w = math.floor(h * (d[1] > 0 and d[1] or 16) / math.max(1, d[2] > 0 and d[2] or 9))
  for _, m in ipairs(M.touch_marks(w, h)) do render.draw_text(font, m.text, m.x, m.y, 1, color) end
end

local touch_ready = false
-- Adds the on-screen controls once; returns true if they were added now.
function M.setup_touch()
  if touch_ready then return false end
  for _, st in ipairs(M.TOUCH.sticks) do
    input.add_stick(st.name, st.x, st.y, st.radius)
  end
  for _, b in ipairs(M.TOUCH.buttons) do
    input.add_button(b.name, b.x, b.y, b.w, b.h)
  end
  touch_ready = true
  return true
end

function M.touch_ready()
  return touch_ready
end

return M
