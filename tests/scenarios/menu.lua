-- SPDX-License-Identifier: MIT
-- Escape in play opens the menu; while it is open the heir's actions are
-- idle, and alone the world stands still (in company it goes on); Escape
-- closes it again. Losing focus opens the menu and mutes; focus back, the
-- sound returns and the menu stays.
-- spec: accessibility: Escape opens the menu in play
-- spec: accessibility: Focus lost and back
-- spec: game-loop: The world held and let go
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
expect_eq(world.time().time, t, "alone, the world stands still")
expect(not game.magic.charging_left and not (game.magic.charging and game.magic.charging.hand == "left"),
  "the heir does not cast while the menu is open")
-- Down to Settings (past Help), in, and change the text size; then back out twice.
input.inject("steer", 0, -1, 2)
step(20)
input.inject("steer", 0, -1, 2)
step(3)
input.inject("confirm", 1, 1)
step(2)
expect_eq(game.menu.page, "settings")
for i, name in ipairs(require("ui.settings").ORDER) do -- (the cursor down to the text size)
  if name == "text_size" then game.menu.at = i - 1 end
end
input.inject("steer", 0, -1, 2)
step(20)
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
t = world.time().time
step(10)
expect(world.time().time > t, "the world goes on again")
-- Losing focus: the menu opens, the sound is muted; focus back: the sound returns, the menu stays.
require("ui.settings").set(game.settings, "master_volume", 80)
game.on_focus(false)
expect(game.menu.open, "the menu opened")
expect_eq(audio.group_volume("master"), 0, "muted")
game.on_focus(false)
game.on_focus(true)
expect(math.abs(audio.group_volume("master") - 0.8) < 1e-6, "the sound is back")
expect(game.menu.open, "the menu stays open")
step(1)
t = world.time().time
step(10)
expect_eq(world.time().time, t, "and the world stands still")
input.inject("quit", 1, 1)
step(2)
expect(not game.menu.open)
