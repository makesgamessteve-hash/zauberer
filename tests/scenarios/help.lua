-- SPDX-License-Identifier: MIT
-- Help: prompts name the control of the device in hand; first-time hints show
-- once each (kept with the player's data), not when turned off, and again
-- after "Show hints again"; the help pages list the controls and explain
-- flying, walking, casting, survival, the bag and the story so far.
-- spec: help: Switching device
-- spec: help: A hint once
-- spec: help: Reading help
scenario { name = "help" }
local game = demo.game
local help = require("ui.help")
local menu = require("ui.menu")
local settings = require("ui.settings")
step(5)
-- The controls of each device.
expect_eq(input.device(), "desktop")
expect_eq(help.control("mount", "desktop"), "E")
expect_eq(help.control("cast_left", "desktop"), "Left mouse button")
expect_eq(help.control("steer", "desktop"), "W S A D")
expect_eq(help.control("mount", "gamepad"), "Right stick")
expect_eq(help.control("boost", "touch"), "[Boost]")
expect_eq(help.control("steer", "touch"), "(Steer)")
expect_eq(help.control("sleep", "gamepad"), "not set")
expect_eq(help.fill("Step off the carpet with {mount}.", "desktop"), "Step off the carpet with E.")
expect_eq(help.fill("Step off the carpet with {mount}.", "gamepad"), "Step off the carpet with Right stick.")

local function hud_text()
  local out = {}
  for _, l in ipairs(demo.flight_lines()) do out[#out + 1] = type(l) == "table" and l.raw or l end
  return table.concat(out, " | ")
end
-- First-time hints: flying, the first shows; six seconds on, the next; each once.
help.reset(game.help)
step(2)
expect(hud_text():find("Steer with W S A D", 1, true), hud_text())
step(60 * 7)
expect(hud_text():find("Menu, help and saving: Escape", 1, true), hud_text())
expect(game.help.seen.fly and game.help.seen.menu)
step(60 * 7)
expect(not hud_text():find("Steer with", 1, true), "shown once")
-- Kept with the player's data.
local again = help.new()
expect(again.seen.fly and again.seen.menu, "the hints seen are kept")
-- Turned off: none; shown again after the reset.
settings.set(game.settings, "hints", false)
help.reset(game.help)
step(60)
expect(not hud_text():find("Steer with", 1, true), "no hints when turned off")
settings.set(game.settings, "hints", true)
step(2)
expect(hud_text():find("Steer with", 1, true), "again after the reset")

-- The help pages, from the menu.
input.inject("quit", 1, 1)
step(2)
expect(game.menu.open)
game.menu.page = "help"
local function lines()
  local out = {}
  for _, l in ipairs(menu.lines(game.menu)) do out[#out + 1] = type(l) == "table" and l.raw or l end
  return table.concat(out, " | ")
end
expect(lines():find("Flying", 1, true) and lines():find("Show hints again", 1, true), lines())
for _, topic in ipairs(help.TOPICS) do
  game.menu.page, game.menu.topic = "topic", topic.id
  local shown = lines()
  expect(#shown > #topic.title + 10, topic.id .. ": " .. shown)
  expect(not shown:find("{", 1, true), "every control filled in: " .. shown)
end
game.menu.topic = "controls"
expect(lines():find("Step off, get on: E", 1, true), lines())
game.menu.topic = "casting"
expect(lines():find("Left mouse button", 1, true) and lines():find("spiral", 1, true), lines())
game.menu.topic = "story"
expect(lines():find("The story has not begun: press F5", 1, true), lines())
input.inject("quit", 1, 1)
step(2)
expect_eq(game.menu.page, "help", "back to the topics")
input.inject("quit", 1, 1)
step(2)
expect_eq(game.menu.page, "main")
input.inject("quit", 1, 1)
step(2)
expect(not game.menu.open)
