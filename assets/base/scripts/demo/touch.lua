-- SPDX-License-Identifier: MIT
-- On-screen controls for touch mode (normalised screen coordinates).
local M = { ready = false }

M.LAYOUT = {
  stick = { name = "move", x = 0.16, y = 0.74, radius = 0.13 },
  buttons = {
    { name = "spin", x = 0.74, y = 0.66, w = 0.2, h = 0.22 },
    { name = "chime", x = 0.74, y = 0.40, w = 0.2, h = 0.18 },
    { name = "credits", x = 0.84, y = 0.04, w = 0.14, h = 0.12 },
  },
}

-- Adds the controls once; returns true if they were added now.
function M.setup()
  if M.ready then
    return false
  end
  local st = M.LAYOUT.stick
  input.add_stick(st.name, st.x, st.y, st.radius)
  for _, b in ipairs(M.LAYOUT.buttons) do
    input.add_button(b.name, b.x, b.y, b.w, b.h)
  end
  M.ready = true
  return true
end

return M
