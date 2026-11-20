-- SPDX-License-Identifier: MIT
-- Unit tests of the reset key (assets/base/scripts/ui/reset.lua) and of
-- engine.reset where the game cannot start over.
local reset = require("ui.reset")

-- spec: world-persistence: The reset key asks first
test.case("the reset key: asked by the first press, done by the second within the window", function()
  local r = reset.new()
  local calls = 0
  local function start_over() calls = calls + 1 return true end
  test.expect(reset.tick(r, false, 1, start_over) == nil)
  test.expect_eq(reset.tick(r, true, 1, start_over), "ask")
  test.expect_eq(reset.lines(r, 2, "F9")[1], "Press F9 again to start the game over (settings are kept)")
  test.expect_eq(reset.tick(r, true, 3, start_over), "reset")
  test.expect_eq(calls, 1)
  test.expect_eq(#reset.lines(r, 3, "F9"), 0)
  -- Too late: the second press only asks again.
  test.expect_eq(reset.tick(r, true, 10, start_over), "ask")
  test.expect(reset.tick(r, false, 10 + reset.WINDOW + 1, start_over) == nil)
  test.expect(r.asked == nil, "the question lapsed")
  test.expect_eq(#reset.lines(r, 10 + reset.WINDOW + 1, "F9"), 0)
  test.expect_eq(reset.tick(r, true, 20, start_over), "ask")
  test.expect_eq(calls, 1)
end)

test.case("the reset key: why the game cannot start over is shown for a while", function()
  local r = reset.new()
  reset.tick(r, true, 1, function() return true end)
  test.expect_eq(reset.tick(r, true, 2, function() return false, "not in a co-op session" end), "refused")
  test.expect_eq(reset.lines(r, 3, "F9")[1], "The game cannot start over: not in a co-op session")
  test.expect_eq(#reset.lines(r, 2 + reset.WINDOW + 1, "F9"), 0)
  test.expect_eq(reset.tick(r, true, 30, function() return true end), "ask", "a new press asks again")
  test.expect(r.refused == nil)
end)

test.case("the reset key's name comes from its bindings", function()
  test.expect_eq(reset.key_name({ "gamepad:y", "key:f9" }), "F9")
  test.expect_eq(reset.key_name({ "gamepad:y" }), "?")
  test.expect_eq(reset.key_name(nil), "?")
  test.expect_eq(reset.key_name(input.bindings("reset")), "F9", "the base game's binding")
end)

-- spec: world-persistence: No reset outside play
test.case("engine.reset is refused in tests and scenarios, and the save data stays", function()
  save.set("reset_probe", 7)
  local ok, why = engine.reset({ "settings" })
  test.expect(not ok)
  test.expect_eq(why, "only in play")
  test.expect_eq(save.get("reset_probe"), 7)
  save.set("reset_probe", nil)
  test.expect_error(function() engine.reset("settings") end)
end)
