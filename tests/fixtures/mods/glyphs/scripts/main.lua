-- SPDX-License-Identifier: MIT
local font
engine.on("init", function() font = render.font("fonts/pixel.lua") end)
engine.on("frame", function()
  -- U+263A is not in the pixel font: the replacement glyph is drawn instead.
  render.draw_text(font, "Rune \u{263A} ok", 8, 340, 1, { 1, 1, 1, 1 })
end)
