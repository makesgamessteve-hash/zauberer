-- SPDX-License-Identifier: MIT
-- An emulated headset (xr.emulate) through the engine's WebXR path, headless and rendered in a
-- 640 x 360 window (tests/engine). The xr_events fixture mod records the xr_session events.
-- spec: webxr-session: No session on the desktop
-- spec: webxr-session: Head pose reaches Lua
-- spec: webxr-session: Select presses an action
-- spec: webxr-session: Emulated headset on the desktop
scenario { name = "emulated headset" }
demo.game.set_mode("showcase")
demo.game.hud = false
for _, b in ipairs({ { "xr_any", "xr:select" }, { "xr_right", "xr:right_select" }, { "xr_left", "xr:left_select" } }) do
  input.define(b[1], "button")
  input.bind(b[1], b[2])
end
local rendered = render.stats().backend == "gl"

-- Session events and emulated XR frames come at the start of an engine iteration (up to a
-- headless batch of steps later).
local function wait_until(done)
  for _ = 1, 1300 do
    if done() then return end
    step(1)
  end
end

-- No headset on the desktop: one view from the camera.
step(60)
local s = xr.state()
expect(not s.available and not s.active and not s.emulated, "no session on the desktop")
expect_eq(s.views, 0)
expect_eq(render.stats().views, 1)

-- The head 10 cm right, 20 cm up and 30 cm forward of the seat, turned 30 degrees to the left.
local half = math.rad(30) / 2
local head = { position = { 0.1, 0.2, -0.3 }, orientation = { 0, math.sin(half), 0, math.cos(half) } }
xr.emulate(head)
step(1)
expect_eq(render.stats().views, 2)
wait_until(function() return xr.state().frames >= 1 and #xr_events >= 1 end)
s = xr.state()
expect(s.available and s.active and s.emulated, "the emulated session runs")
expect_eq(s.views, 2)
expect(s.frames >= 1, "XR frames are counted")
expect_eq(xr_events[1], "started")
for i = 1, 3 do
  expect_near(s.head.position[i], head.position[i], 1e-5, "head position")
end
for i = 1, 4 do
  expect_near(s.head.orientation[i], head.orientation[i], 1e-5, "head orientation")
end
expect_eq(s.size[1], render.stats().drawable_width, "the emulated headset fills the window")

if rendered then
  local ok, info = render.match_golden("xr_stereo", "window")
  expect(ok, info)
  -- Screen text (the demo's HUD) is drawn into both eyes at the same place.
  demo.game.hud = true
  step(1)
  render.capture("window")
  local function text_at(left)
    for y = 4, 12 do
      for x = left + 4, left + 60 do
        local r, g = render.pixel(x, y)
        if r > 200 and g > 150 then return true end
      end
    end
    return false
  end
  expect(text_at(0), "text in the left eye")
  expect(text_at(320), "text in the right eye")
  demo.game.hud = false
end

-- The right controller's select starts and ends between two ticks.
xr.emulate_button("right", "select", true)
xr.emulate_button("right", "select", false)
step(1)
expect(input.state("xr_any").pressed, "xr:select is pressed")
expect(input.state("xr_right").pressed, "xr:right_select is pressed")
expect(not input.state("xr_left").pressed, "xr:left_select is not")

-- Moving the head keeps the session; ending it returns to the camera's single view.
xr.emulate({ position = { 0, 0, 0 } })
step(1)
expect_near(xr.state().head.position[1], 0, 1e-6)
xr.emulate(false)
step(1)
s = xr.state()
expect(not s.active and not s.available, "the session ended")
expect_eq(s.views, 0)
step(1)
expect_eq(render.stats().views, 1)
wait_until(function() return #xr_events >= 2 end)
expect_eq(xr_events[2], "ended")
