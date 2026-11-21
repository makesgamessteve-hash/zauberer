-- SPDX-License-Identifier: MIT
-- Runs with ZB_TOUCH_PRIMARY=1 (a touch-first device).
-- spec: touch-and-gamepad-controls: Touch layout
scenario { name = "touch-first device" }
expect_eq(engine.info().input_mode, "touch")
expect_eq(input.mode(), "touch")
expect_eq(demo.game.input_mode_event, "touch", "scripts receive an input-mode event")
expect(demo.controls.touch_ready(), "the flight's on-screen controls are set up")
expect(not demo.controls.setup_touch(), "they are added only once")
step(1)
-- A menu button opens the menu (it is bound to the menu's action); an OK button chooses.
expect(table.concat(input.bindings("quit"), " "):find("touch:button:menu", 1, true), "the menu button opens the menu")
expect(table.concat(input.bindings("confirm"), " "):find("touch:button:confirm", 1, true), "an OK button")
local names = {}
for _, b in ipairs(demo.controls.TOUCH.buttons) do
  names[b.name] = true
  expect(b.x >= 0 and b.y >= 0 and b.x + b.w <= 1 and b.y + b.h <= 1, b.name .. " on the screen")
end
expect(names.menu and names.confirm and names.boost and names.chime and names.credits)
-- Every control is labelled where it is drawn, inside the screen.
local marks = demo.controls.touch_marks(640, 360)
expect_eq(#marks, #demo.controls.TOUCH.sticks + #demo.controls.TOUCH.buttons)
local menu_mark
for _, m in ipairs(marks) do
  expect(m.x >= 0 and m.y >= 0 and m.x < 640 and m.y < 360, m.text .. " on the screen")
  if m.text == "[Menu]" then menu_mark = m end
end
expect(menu_mark, "the menu button is labelled")
step(2)
-- The menu says how to use it by touch.
input.inject("quit", 1, 1)
step(2)
expect(demo.game.menu.open, "the menu opened")
local said = table.concat((function()
  local out = {}
  for _, l in ipairs(require("ui.menu").lines(demo.game.menu)) do out[#out + 1] = type(l) == "table" and l.raw or l end
  return out
end)(), " | ")
expect(said:find("OK: open, Menu: back", 1, true), said)
input.inject("quit", 1, 1)
step(2)
expect(not demo.game.menu.open)
