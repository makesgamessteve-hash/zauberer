-- SPDX-License-Identifier: MIT
-- Rendered with a 1000 x 700 window.
-- spec: rendering-core: Non-integer window size
scenario { name = "fractional layout" }
demo.game.set_mode("showcase")
demo.game.hud = false
step(30)
local st = render.stats()
expect_eq(st.internal_height, 360)
expect_eq(st.scale, 0, "scaled fractionally")
expect(st.out_width >= 998 and st.out_height >= 698, "the scene fills the window")
render.capture("window")
local r, g, b = render.pixel(999, 699)
-- the clear colour (0.08, 0.06, 0.14) fills any border
expect(math.abs(r - 20) <= 2 and math.abs(g - 15) <= 2 and math.abs(b - 36) <= 2,
  string.format("border pixel %d,%d,%d", r, g, b))
