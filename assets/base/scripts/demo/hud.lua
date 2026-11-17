-- SPDX-License-Identifier: MIT
-- Heads-up text of the M0 demo.
local M = {}

local COLORS = {
  { 1.0, 0.85, 0.4, 1 },
  { 0.8, 0.8, 0.9, 1 },
  { 0.6, 0.9, 0.7, 1 },
}

function M.lines(s, mode, info)
  local controls
  if mode == "touch" then
    controls = "Stick: turn | SPIN | CHIME"
  else
    controls = "WASD/stick: turn | Space: spin | C: chime | Esc: quit"
  end
  return {
    string.format("Zauberer %s - engine foundation", info.version),
    string.format("%s | %s | input: %s", info.platform, info.build, mode),
    controls,
    string.format("spins: %d  chimes: %d", s.spins, s.chimes),
  }
end

function M.color(i)
  return COLORS[math.min(i, #COLORS)]
end

return M
