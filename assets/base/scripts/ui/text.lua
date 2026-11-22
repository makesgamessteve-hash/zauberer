-- SPDX-License-Identifier: MIT
-- Lines of text on the screen: in the player's language, at their text size,
-- and with a dark outline in high contrast. A line can be { raw = text } for
-- text that is never translated (names in the credits).
local i18n = require("ui.i18n")

local text = {}

text.LINE = 9    -- pixels from one line to the next at size 1
text.ADVANCE = 6 -- pixels from one letter to the next at size 1

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

-- A line broken at spaces into pieces of at most width letters (a word
-- longer than that is cut).
function text.wrap(shown, width)
  if width < 1 or utf8.len(shown) == nil or utf8.len(shown) <= width then return { shown } end
  local out, cur = {}, ""
  for word in shown:gmatch("%S+") do
    local joined = cur == "" and word or (cur .. " " .. word)
    if utf8.len(joined) <= width then
      cur = joined
    else
      if cur ~= "" then out[#out + 1] = cur end
      while utf8.len(word) > width do
        local cut = utf8.offset(word, width + 1)
        out[#out + 1] = word:sub(1, cut - 1)
        word = word:sub(cut)
      end
      cur = word
    end
  end
  if cur ~= "" then out[#out + 1] = cur end
  return out
end

-- The screen's width in pixels of text (the internal height by the window's shape).
local function screen_width()
  local d = engine.info().drawable_size
  local h = render.internal_height()
  return (d[1] > 0 and d[2] > 0) and math.floor(h * d[1] / d[2]) or math.floor(h * 16 / 9)
end

-- Draws the lines from (x, y) down, each wrapped to the screen; color(i)
-- gives each line's colour.
function text.lines(font, lines, color, x, y)
  local s = style.scale
  local left = x or 4
  local width = math.floor((screen_width() - left - 2) / (text.ADVANCE * s))
  local row = 0
  for i, line in ipairs(lines) do
    local c = color and color(i) or WHITE
    if style.contrast then c = lifted(c) end
    for _, shown in ipairs(text.wrap(text.of(line), width)) do
      local yy = (y or 4) + row * text.LINE * s
      if style.contrast then
        for _, d in ipairs({ { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } }) do
          render.draw_text(font, shown, left + d[1], yy + d[2], s, SHADOW)
        end
      end
      render.draw_text(font, shown, left, yy, s, c)
      row = row + 1
    end
  end
end

return text
