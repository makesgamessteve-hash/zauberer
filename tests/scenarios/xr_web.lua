-- SPDX-License-Identifier: MIT
-- Web E2E (tests/web/web.test.mjs) with a mock of navigator.xr: waits until the test presses
-- Enter VR, checks the head pose and the views, waits for a select and for the session to end.
-- Its log lines pace the browser test. The scene is only the sky: the left view looks at the
-- horizon, the right one 60 degrees up at the zenith.
-- spec: webxr-session: Enter and leave
-- spec: webxr-session: Head pose reaches Lua
-- spec: webxr-session: Select presses an action
scenario { name = "webxr session" }
if demo then
  demo.game.hud = false
  demo.game.camera = function()
    return { eye = { 0, 1.5, 0 }, target = { 0, 1.5, -1 }, fov = 60, near = 0.1, far = 1000 }
  end
end
render.atmosphere { zenith = { 0.1, 0.2, 0.8 }, horizon = { 0.9, 0.6, 0.3 }, sun = { 0, -1, 0 } }
for _, b in ipairs({ { "xr_any", "xr:select" }, { "xr_right", "xr:right_select" }, { "xr_left", "xr:left_select" } }) do
  input.define(b[1], "button")
  input.bind(b[1], b[2])
end

local function wait_until(done, what)
  for _ = 1, 7200 do
    if done() then return true end
    step(1)
  end
  expect(false, "timed out waiting for " .. what)
  return false
end

expect(not xr.state().active, "no session before the button is pressed")
wait_until(function() return xr.state().available end, "a supported device")
engine.log("info", "xr web: waiting for the session")
wait_until(function() return xr.state().frames >= 5 end, "XR frames")
local s = xr.state()
expect(s.active and not s.emulated, "the device's session runs")
expect_eq(s.views, 2)
expect_eq(s.size[1], 820)
expect_eq(s.size[2], 420)
expect_eq(render.stats().views, 2)
-- The mock reports the head at (0.1, 0.2, -0.3), turned 30 degrees to the left.
local head = { 0.1, 0.2, -0.3 }
local q = { 0, math.sin(math.rad(15)), 0, math.cos(math.rad(15)) }
for i = 1, 3 do
  expect_near(s.head.position[i], head[i], 1e-5, "head position")
end
for i = 1, 4 do
  expect_near(s.head.orientation[i], q[i], 1e-5, "head orientation")
end
engine.log("info", "xr web: views checked")

local seen
wait_until(function()
  if input.state("xr_any").pressed then
    seen = { right = input.state("xr_right").pressed, left = input.state("xr_left").pressed }
  end
  return seen ~= nil
end, "a select")
expect(seen and seen.right, "xr:right_select is pressed")
expect(seen and not seen.left, "xr:left_select is not")
engine.log("info", "xr web: select seen")

wait_until(function() return not xr.state().active end, "the end of the session")
local frames = render.stats().frames
step(5)
expect_eq(render.stats().views, 1, "one view from the camera again")
expect(render.stats().frames > frames, "the page's loop renders to the canvas again")
expect(xr.state().available, "a new session can be started")
