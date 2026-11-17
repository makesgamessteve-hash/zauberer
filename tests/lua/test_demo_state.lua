-- SPDX-License-Identifier: MIT
-- Unit tests of the demo's pure state logic (assets/base/scripts/demo/state.lua).
local state = require("demo.state")

local function press(fields)
  local t = { pressed = false, held = false, released = false, x = 0, y = 0 }
  for k, v in pairs(fields or {}) do t[k] = v end
  return t
end

test.case("new state has defaults", function()
  local s = state.new()
  test.expect_eq(s.yaw, 30)
  test.expect_eq(s.pitch, 15)
  test.expect_eq(s.distance, 4)
  test.expect_eq(s.spins, 0)
  test.expect_eq(s.credits, false)
end)

test.case("clamp limits values", function()
  test.expect_eq(state.clamp(5, 0, 3), 3)
  test.expect_eq(state.clamp(-5, 0, 3), 0)
  test.expect_eq(state.clamp(2, 0, 3), 2)
end)

test.case("turn and look rotate the camera", function()
  local s = state.new()
  state.update(s, { turn = press({ x = 1, y = 1 }) }, 1)
  test.expect_near(s.yaw, 120, 1e-9)
  test.expect_near(s.pitch, state.MAX_PITCH, 1e-9, "pitch is clamped")
  state.update(s, { look = press({ x = 100, y = 100 }) }, 0)
  test.expect_near(s.yaw, 140, 1e-9)
  test.expect_near(s.pitch, 40, 1e-9)
  state.update(s, { turn = press({ x = -10, y = -10 }) }, 1)
  test.expect_eq(s.pitch, state.MIN_PITCH)
  test.expect(s.yaw >= 0 and s.yaw < 360, "yaw wraps")
end)

test.case("zoom is clamped", function()
  local s = state.new()
  state.update(s, { zoom = press({ x = 1 }) }, 10)
  test.expect_eq(s.distance, state.MIN_DISTANCE)
  state.update(s, { zoom = press({ x = -1 }) }, 10)
  test.expect_eq(s.distance, state.MAX_DISTANCE)
end)

test.case("buttons produce events", function()
  local s = state.new()
  local ev = state.update(s, {
    spin = press({ pressed = true, held = true }),
    chime = press({ pressed = true }),
    credits = press({ pressed = true }),
    fullscreen = press({ pressed = true }),
    quit = press({ pressed = true }),
  }, 0.5)
  test.expect_eq(table.concat(ev, ","), "spin,chime,credits,fullscreen,quit")
  test.expect_eq(s.spins, 1)
  test.expect_eq(s.chimes, 1)
  test.expect_eq(s.credits, true)
  test.expect_near(s.spin, 90, 1e-9)
  state.update(s, { spin = press({ held = true }), credits = press({ pressed = true }) }, 0.5)
  test.expect_near(s.spin, 180, 1e-9)
  test.expect_eq(s.spins, 1, "holding does not count as a new spin")
  test.expect_eq(s.credits, false, "credits toggle")
end)

test.case("no input leaves the state still", function()
  local s = state.new()
  local ev = state.update(s, {}, 1 / 60)
  test.expect_eq(#ev, 0)
  test.expect_eq(s.yaw, 30)
  test.expect_near(s.time, 1 / 60, 1e-12)
end)

test.case("hover, bank, camera and motes", function()
  local s = state.new()
  test.expect_eq(state.hover(s), 0)
  test.expect_eq(state.bank(s), 0)
  s.time = 1
  test.expect_near(state.hover(s), 0.15 * math.sin(1.5), 1e-12)
  local eye, target = state.camera(s)
  test.expect_eq(#eye, 3)
  local dx, dy, dz = eye[1] - target[1], eye[2] - target[2], eye[3] - target[3]
  test.expect_near(math.sqrt(dx * dx + dy * dy + dz * dz), s.distance, 1e-9, "camera orbits at distance")
  local motes = state.motes(s)
  test.expect_eq(#motes, state.MOTES)
  for _, m in ipairs(motes) do
    test.expect_near(math.sqrt(m[1] * m[1] + m[3] * m[3]), 1.6, 1e-9, "motes orbit at 1.6 m")
  end
end)
