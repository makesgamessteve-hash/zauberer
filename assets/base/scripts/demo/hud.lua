-- SPDX-License-Identifier: MIT
-- Heads-up text of the M0 demo.
local M = {}

local COLORS = {
  { 1.0, 0.85, 0.4, 1 },
  { 0.8, 0.8, 0.9, 1 },
  { 0.6, 0.9, 0.7, 1 },
}

function M.lines(s, mode, info, credits)
  local controls
  if mode == "touch" then
    controls = "Stick: turn | SPIN | CHIME | CREDITS"
  else
    controls = "WASD/stick: turn | Space: spin | C: chime | F1: credits | Esc: quit"
  end
  local lines = {
    string.format("Zauberer %s - engine foundation", info.version),
    string.format("%s | %s | input: %s", info.platform, info.build, mode),
    controls,
    string.format("spins: %d  chimes: %d", s.spins, s.chimes),
  }
  if s.credits then
    lines[#lines + 1] = "Credits:"
    for _, c in ipairs(credits or {}) do
      lines[#lines + 1] = { raw = M.credit_line(c) } -- (names: never translated)
    end
  end
  return lines
end

-- One line of the credits list: assets name their file and author, code its name and version.
function M.credit_line(c)
  if c.kind == "code" then
    return string.format("  %s %s (%s)", c.name or "?", c.version or "", c.license or "?")
  end
  return string.format("  [%s] %s - %s (%s)", c.package or "?", c.file or "?", c.author or "?", c.license or "?")
end

function M.color(i)
  return COLORS[math.min(i, #COLORS)]
end

return M
