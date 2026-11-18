-- SPDX-License-Identifier: MIT
-- Unit tests of the new-world screen (assets/base/scripts/create/screen.lua).
local screen = require("create.screen")

test.case("lines before and after progress", function()
  test.expect_eq(screen.lines({ ok = false, err = "radius out of range" })[2], "Cannot create it: radius out of range")
  test.expect_eq(screen.lines({ ok = true })[2], "Starting...")
  local lines = screen.lines({
    ok = true,
    progress = { label = "Drifting continents", fraction = 0.25, stage = "deep time", myr_ago = 312.4, attempt = 1 },
  })
  test.expect_eq(lines[2], "Drifting continents  25%")
  test.expect_eq(lines[3], "312 million years ago")
  test.expect_eq(lines[4], "Steering towards an Earth-like world (attempt 2)")
  test.expect_eq(#lines, 4)
end)

test.case("cancel", function()
  local s = screen.new({ quality = "quick", radius = 300000, seed = 5 })
  test.expect(s.ok, tostring(s.err))
  screen.tick(s, 0.5)
  test.expect(s.spin > 0)
  screen.cancel(s)
  test.expect(not s.ok)
  screen.tick(s, 0.5) -- nothing happens once cancelled
  test.expect(not screen.ready(s))
  screen.cancel(s) -- twice is harmless
  local again = screen.new({ quality = "quick", radius = 300000, seed = 6 })
  test.expect(again.ok, "a new creation after cancelling: " .. tostring(again.err))
  screen.cancel(again)
end)
