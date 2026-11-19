-- SPDX-License-Identifier: MIT
-- Escape in play opens the menu; while it is open the heir's actions are
-- idle but the world goes on; Escape closes it again.
-- spec: accessibility: Escape opens the menu in play
scenario { name = "menu" }
local game = demo.game
step(30)
expect_eq(game.mode, "flight")
input.inject("quit", 1, 1)
step(2)
expect(game.menu.open, "Escape opened the menu")
local t = world.time().time
input.inject("cast_left", 1, 1, 10)
step(20)
expect(world.time().time > t, "the world goes on")
expect(not game.magic.charging_left and not (game.magic.charging and game.magic.charging.hand == "left"),
  "the heir does not cast while the menu is open")
-- Down to Settings, in, and change the text size; then back out twice.
input.inject("steer", 0, -1, 2)
step(3)
input.inject("confirm", 1, 1)
step(2)
expect_eq(game.menu.page, "settings")
for _ = 1, 1 do
  input.inject("steer", 0, -1, 2)
  step(20)
end
input.inject("steer", 1, 0, 2)
step(3)
expect_eq(game.settings.text_size, 2, "the text size went up")
input.inject("quit", 1, 1)
step(2)
expect_eq(game.menu.page, "main")
input.inject("quit", 1, 1)
step(2)
expect(not game.menu.open, "Escape closed the menu")
require("ui.settings").set(game.settings, "text_size", 1)
expect_eq(game.mode, "flight", "and play goes on")
