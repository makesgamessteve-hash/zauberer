-- SPDX-License-Identifier: MIT
-- luacheck: globals render (ui_nine is wrapped to record the frames drawn)
-- The art kit's variants: high contrast draws the high-contrast frames; each
-- colour-vision mode gives mana colours that stay apart, with the same glyphs.
-- spec: ui-art-kit: Switching variants
scenario { name = "kit variants" }
local kit = require("ui.kit")
local data = kit.data
step(2)
local srcs = {}
local real = render.ui_nine
render.ui_nine = function(tex, x, y, w, h, opts)
  srcs[#srcs + 1] = opts.src
  return real(tex, x, y, w, h, opts)
end
kit.configure({ contrast = false })
kit.frame("kilim", 0, 0, 40, 20)
kit.configure({ contrast = true })
expect(kit.contrast(), "high contrast on")
kit.frame("kilim", 0, 0, 40, 20)
render.ui_nine = real
expect_eq(srcs[1][2], 0, "the plain frame")
expect_eq(srcs[2][2], data.contrast_offset, "the high-contrast frame")
expect(not pcall(kit.frame, "no such frame", 0, 0, 1, 1), "an unknown frame is an error")
for _, mode in ipairs({ "full", "deuteranopia", "protanopia", "tritanopia" }) do
  kit.configure({ vision = mode })
  for i, a in ipairs(data.mana_order) do
    local ca, ga = kit.mana(a)
    expect_eq(ga, data.glyphs[a], "the glyph stays")
    for j = i + 1, #data.mana_order do
      local cb = kit.mana(data.mana_order[j])
      local d = math.sqrt((ca[1] - cb[1]) ^ 2 + (ca[2] - cb[2]) ^ 2 + (ca[3] - cb[3]) ^ 2) * 255
      expect(d >= data.min_mana_distance - 1, string.format("%s: %s and %s apart (%.0f)", mode, a,
        data.mana_order[j], d))
    end
  end
end
kit.configure({ vision = "no such mode" })
local fire = kit.mana("fire")
expect(math.abs(fire[1] - data.mana.normal.fire[1]) < 1e-6, "an unknown mode is the normal colours")
expect(kit.has_icon("goal") and not kit.has_icon("nothing"), "icons by name")
expect_eq(kit.binding_icon("touch:button:menu"), "touch")
expect_eq(kit.binding_icon("gamepad:a"), "pad_a")
expect(#kit.wrap("one two three four five six", 40) > 1, "words wrap")
kit.configure({})
