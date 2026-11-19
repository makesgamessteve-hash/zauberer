-- SPDX-License-Identifier: MIT
-- Lines of text on the screen: in the player's language, at their text size,
-- and with a dark outline in high contrast. A line can be { raw = text } for
-- text that is never translated (names in the credits).
local i18n = require("ui.i18n")

local text = {}

text.LINE = 9 -- pixels from one line to the next at size 1

local style = { scale = 1, contrast = false }

-- opts: { scale = 1..3, contrast = boolean }
function text.configure(opts)
  style.scale = math.max(1, math.min(3, math.floor((opts and opts.scale) or 1)))
  style.contrast = opts and opts.contrast or false
end

function text.style() return { scale = style.scale, contrast = style.contrast } end

local WHITE = { 1, 1, 1, 1 }
local SHADOW = { 0, 0, 0, 1 }

-- In high contrast every colour is lifted towards white.
local function lifted(c)
  return { 0.5 + 0.5 * c[1], 0.5 + 0.5 * c[2], 0.5 + 0.5 * c[3], 1 }
end

-- The text a line shows (translated unless raw).
function text.of(line)
  if type(line) == "table" then return line.raw or "" end
  return i18n.t(line)
end

-- Draws the lines from (x, y) down; color(i) gives each line's colour.
function text.lines(font, lines, color, x, y)
  local s = style.scale
  for i, line in ipairs(lines) do
    local shown = text.of(line)
    local yy = (y or 4) + (i - 1) * text.LINE * s
    local c = color and color(i) or WHITE
    if style.contrast then
      for _, d in ipairs({ { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } }) do
        render.draw_text(font, shown, (x or 4) + d[1], yy + d[2], s, SHADOW)
      end
      c = lifted(c)
    end
    render.draw_text(font, shown, x or 4, yy, s, c)
  end
end

return text
