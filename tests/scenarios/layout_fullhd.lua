-- SPDX-License-Identifier: MIT
-- Rendered with a 1920 x 1080 window.
-- spec: rendering-core: Full HD window
scenario { name = "full HD layout" }
demo.game.set_mode("showcase")
demo.game.hud = false
step(30)
local st = render.stats()
expect_eq(st.internal_width .. "x" .. st.internal_height, "640x360")
expect_eq(st.scale, 3)
expect_eq(st.out_width .. "x" .. st.out_height, "1920x1080")
local cap = render.capture("window")
expect_eq(cap.width .. "x" .. cap.height, "1920x1080")
-- Nearest-neighbour upscaling: every 3 x 3 block has one colour.
local blocks, uniform = 0, 0
for by = 20, 340, 40 do
  for bx = 20, 620, 40 do
    blocks = blocks + 1
    local r0, g0, b0 = render.pixel(bx * 3, by * 3)
    local same = true
    for dy = 0, 2 do
      for dx = 0, 2 do
        local r, g, b = render.pixel(bx * 3 + dx, by * 3 + dy)
        same = same and r == r0 and g == g0 and b == b0
      end
    end
    uniform = uniform + (same and 1 or 0)
  end
end
expect_eq(uniform, blocks, "no filtering between upscaled pixels")
