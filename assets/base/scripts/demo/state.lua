-- SPDX-License-Identifier: MIT
-- Pure state logic of the M0 demo: no engine calls, fully unit-testable.
local M = {}

M.TURN_RATE = 90      -- degrees per second at full stick
M.LOOK_RATE = 0.2     -- degrees per mouse pixel
M.ZOOM_RATE = 2.0     -- metres per second
M.SPIN_RATE = 180     -- degrees per second while held
M.MIN_PITCH, M.MAX_PITCH = -10, 60
M.MIN_DISTANCE, M.MAX_DISTANCE = 2, 8
M.MOTES = 6

local function clamp(v, lo, hi)
  if v < lo then return lo end
  if v > hi then return hi end
  return v
end
M.clamp = clamp

function M.new()
  return { yaw = 30, pitch = 15, distance = 4, spin = 0, time = 0, spins = 0, chimes = 0, credits = false }
end

local function axis(a, field)
  return (a and a[field]) or 0
end

-- Advances the state by dt seconds; returns a list of events
-- ("chime", "quit", "fullscreen", "spin", "credits").
function M.update(s, inp, dt)
  local events = {}
  s.time = s.time + dt
  s.yaw = (s.yaw + axis(inp.turn, "x") * M.TURN_RATE * dt + axis(inp.look, "x") * M.LOOK_RATE) % 360
  s.pitch = clamp(s.pitch + axis(inp.turn, "y") * M.TURN_RATE * dt - axis(inp.look, "y") * M.LOOK_RATE,
    M.MIN_PITCH, M.MAX_PITCH)
  s.distance = clamp(s.distance - axis(inp.zoom, "x") * M.ZOOM_RATE * dt, M.MIN_DISTANCE, M.MAX_DISTANCE)
  if inp.spin and inp.spin.held then
    s.spin = (s.spin + M.SPIN_RATE * dt) % 360
  end
  if inp.spin and inp.spin.pressed then
    s.spins = s.spins + 1
    events[#events + 1] = "spin"
  end
  if inp.chime and inp.chime.pressed then
    s.chimes = s.chimes + 1
    events[#events + 1] = "chime"
  end
  if inp.credits and inp.credits.pressed then
    s.credits = not s.credits
    events[#events + 1] = "credits"
  end
  if inp.fullscreen and inp.fullscreen.pressed then
    events[#events + 1] = "fullscreen"
  end
  if inp.quit and inp.quit.pressed then
    events[#events + 1] = "quit"
  end
  return events
end

function M.hover(s)
  return 0.15 * math.sin(s.time * 1.5)
end

function M.bank(s)
  return 8 * math.sin(s.time * 0.7)
end

-- Camera position orbiting the carpet.
function M.camera(s)
  local yaw, pitch = math.rad(s.yaw), math.rad(s.pitch)
  local target = { 0, 0.2, 0 }
  local eye = {
    target[1] + s.distance * math.cos(pitch) * math.sin(yaw),
    target[2] + s.distance * math.sin(pitch),
    target[3] + s.distance * math.cos(pitch) * math.cos(yaw),
  }
  return eye, target
end

-- Mana motes orbiting the carpet.
function M.motes(s)
  local out = {}
  for i = 1, M.MOTES do
    local a = s.time * 0.6 + (i - 1) * (2 * math.pi / M.MOTES)
    out[i] = { 1.6 * math.cos(a), 0.4 + 0.2 * math.sin(s.time * 2 + i), 1.6 * math.sin(a) }
  end
  return out
end

return M
