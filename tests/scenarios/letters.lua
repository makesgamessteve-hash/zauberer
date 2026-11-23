-- SPDX-License-Identifier: MIT
-- The pixel font has the letters of German and of the other left-to-right
-- languages packages may add (extended Latin), and typographic signs.
-- spec: localisation: Extended Latin letters
scenario { name = "letters" }
local font = render.font("fonts/pixel.lua")
step(2)
for _ = 1, 300 do -- (in the browser the font may still be loading)
  if render.state(font) ~= "pending" then break end
  step(1)
end
expect_eq(render.state(font), "ready", "the font is loaded")
local box = render.text_glyphs(font, "\u{25A1}")[1]
local text = "ÄÖÜäöüß éèêëàâîïôûùç ñ ãõ åæøÆØ œŒ ğĞşŞıİ ąćęłńśźżŁ čďěňřšťůžŽ őű șț «»„“”‘’–—…€°·×¡¿"
local glyphs = render.text_glyphs(font, text)
local n = 0
for _ in utf8.codes(text) do n = n + 1 end
expect_eq(#glyphs, n)
for i, g in ipairs(glyphs) do
  expect(g ~= box, "letter " .. i .. " has a glyph")
end
expect_eq(render.text_glyphs(font, "\u{263A}")[1], box, "letters it lacks show the box")
