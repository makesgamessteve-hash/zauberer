-- SPDX-License-Identifier: MIT
-- Rendered (window, 640 x 360) with the fixture mod "glyphs" installed.
-- spec: rendering-core: Missing glyph
scenario { name = "missing glyph" }
demo.game.set_mode("showcase")
demo.game.hud = false
step(30)
local font = render.font("fonts/pixel.lua")
step(1)
local glyphs = render.text_glyphs(font, "Rune \u{263A} ok")
local box = render.text_glyphs(font, "\u{25A1}")[1]
expect_eq(#glyphs, 9)
expect_eq(glyphs[6], box, "the missing character maps to the replacement glyph")
local ok, info = render.match_golden("missing_glyph")
expect(ok, info)
