-- SPDX-License-Identifier: MIT
-- Unit tests of the demo's touch controls (assets/base/scripts/demo/touch.lua).
local touch = require("demo.touch")

test.case("layout stays on screen", function()
  local st = touch.LAYOUT.stick
  test.expect(st.x - st.radius >= 0 and st.x + st.radius <= 1)
  for _, b in ipairs(touch.LAYOUT.buttons) do
    test.expect(b.x >= 0 and b.y >= 0 and b.x + b.w <= 1 and b.y + b.h <= 1, b.name)
  end
end)

test.case("controls are added once", function()
  touch.ready = false
  test.expect_eq(touch.setup(), true)
  test.expect_eq(touch.setup(), false)
end)
