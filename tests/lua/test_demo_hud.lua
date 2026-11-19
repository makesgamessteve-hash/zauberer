-- SPDX-License-Identifier: MIT
-- Unit tests of the demo's heads-up text (assets/base/scripts/demo/hud.lua).
local hud = require("demo.hud")
local state = require("demo.state")

local INFO = { version = "0.1.0", platform = "linux", build = "release" }

test.case("desktop lines", function()
  local s = state.new()
  s.spins, s.chimes = 2, 3
  local lines = hud.lines(s, "desktop", INFO)
  test.expect_eq(#lines, 4)
  test.expect_eq(lines[1], "Zauberer 0.1.0 - engine foundation")
  test.expect_eq(lines[2], "linux | release | input: desktop")
  test.expect(lines[3]:find("Space: spin", 1, true) ~= nil)
  test.expect_eq(lines[4], "spins: 2  chimes: 3")
end)

test.case("touch lines", function()
  local lines = hud.lines(state.new(), "touch", INFO)
  test.expect_eq(lines[3], "Stick: turn | SPIN | CHIME | CREDITS")
end)

test.case("credits list", function()
  local s = state.new()
  s.credits = true
  local credits = {
    { kind = "asset", package = "base", file = "models/carpet.glb", author = "A", license = "CC-BY-4.0" },
    { kind = "code", name = "Lua", version = "5.4.9", license = "MIT" },
    {},
  }
  local lines = hud.lines(s, "desktop", INFO, credits)
  test.expect_eq(lines[5], "Credits:")
  test.expect_eq(lines[6].raw, "  [base] models/carpet.glb - A (CC-BY-4.0)")
  test.expect_eq(lines[7].raw, "  Lua 5.4.9 (MIT)")
  test.expect_eq(lines[8].raw, "  [?] ? - ? (?)")
  test.expect_eq(hud.credit_line({ kind = "code" }), "  ?  (?)")
  test.expect_eq(#hud.lines(s, "desktop", INFO), 5, "no credits data")
end)

test.case("colors", function()
  test.expect_eq(#hud.color(1), 4)
  test.expect_eq(hud.color(9), hud.color(3), "later lines reuse the last colour")
end)
