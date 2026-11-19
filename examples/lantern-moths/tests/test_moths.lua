-- SPDX-License-Identifier: MIT
-- Unit tests of the lantern moths (run: zauberer --headless --test examples/lantern-moths/tests
-- with the mod installed).
local moths = require("lantern_moths.moths")

test.case("dust at night, every so often", function()
  local m = moths.new()
  test.expect_eq(moths.tick(m, 30, { night = false }), 0, "not by day")
  test.expect_eq(moths.tick(m, moths.EVERY - 1, { night = true }), 0)
  test.expect_eq(moths.tick(m, 1, { night = true }), 1)
  test.expect_eq(m.given, 1)
  test.expect_eq(#moths.hud_lines(m, { night = false }), 0)
  test.expect_eq(moths.hud_lines(m, { night = true })[1], "Lantern moths: 5 around you")
  test.expect_eq(#moths.places(3), moths.FLOCK)
end)
