-- SPDX-License-Identifier: MIT
-- Unit tests of drawing gestures (assets/base/scripts/magic/stroke.lua).
local stroke = require("magic.stroke")

-- spec: gesture-recognition: Gestures from Lua
test.case("a circle drawn with the mouse", function()
  local s = stroke.new()
  test.expect_eq(stroke.update(s, false, 5, 5), nil, "nothing while the button is up")
  local t = gesture.template("circle", 40)
  local px, py = t[1] * 80, t[2] * 80
  stroke.update(s, true, 0, 0)
  for i = 2, 40 do
    local x, y = t[2 * i - 1] * 80, t[2 * i] * 80
    test.expect_eq(stroke.update(s, true, x - px, y - py), nil)
    px, py = x, y
  end
  test.expect(s.drawing)
  local g = stroke.update(s, false, 0, 0)
  test.expect_eq(g.form, "circle")
  test.expect(g.accuracy > 0.9, "accuracy " .. g.accuracy)
  test.expect_eq(s.last, g)
  test.expect(not s.drawing)
  local trail = stroke.trail(s, 100, 100, 20)
  test.expect(#trail > 10 and #trail <= 48)
  for _, p in ipairs(trail) do
    test.expect(math.abs(p[1] - 100) <= 21 and math.abs(p[2] - 100) <= 21, "within the radius")
  end
end)

test.case("a line drawn with a stick, and a tap", function()
  local s = stroke.new({ assist = true })
  for _ = 1, 30 do
    stroke.update(s, true, 0, 0, 1, 0, 1 / 60)
  end
  local g = stroke.update(s, false)
  test.expect_eq(g.form, "line")
  test.expect_near(g.angle, 0, 1e-9)
  stroke.update(s, true, 0, 0)
  stroke.update(s, true, 2, 1)
  test.expect_eq(stroke.update(s, false).form, "tap")
  test.expect_eq(#stroke.trail(stroke.new(), 0, 0, 10), 0)
end)

test.case("gesture API", function()
  local forms = gesture.forms()
  test.expect_eq(#forms, 8)
  test.expect_eq(forms[1], "tap")
  test.expect_eq(forms[8], "wave")
  local sq = gesture.template("square", 5)
  test.expect_eq(#sq, 10)
  local g = gesture.recognize({ 0, 0, 100, 0, 100, 100, 0, 100, 0, 0 })
  test.expect_eq(g.form, "square")
  test.expect_near(g.size, 50, 1e-6)
  test.expect_near(g.x, 50, 1e-6)
  test.expect(g.second ~= "square")
  test.expect_eq(gesture.recognize({}).form, "none")
  test.expect_error(function() gesture.recognize({ 1, 2, 3 }) end, "points must be x, y pairs")
  test.expect_error(function() gesture.template("hexagon") end, "no such form")
  test.expect_error(function() gesture.template("line", 1) end, "count must be 2..512")
end)
