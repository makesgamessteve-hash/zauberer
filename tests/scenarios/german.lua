-- SPDX-License-Identifier: MIT
-- The game in German: everything the HUD and the menu show has a German
-- entry; only generated names (and numbers like the version) stay as they are.
-- spec: localisation: The game in German
scenario { name = "german" }
local i18n = require("ui.i18n")
local settings = require("ui.settings")
local game = demo.game
settings.set(game.settings, "language", "de")
i18n.collect(true)
step(120)
input.inject("quit", 1, 1) -- the menu, each of its pages
step(3)
for page = 2, 5 do
  game.menu.page, game.menu.at = ({ "main", "settings", "controls", "packages", "credits" })[page], 1
  step(3)
end
game.menu.page = "main"
input.inject("quit", 1, 1)
step(3)
-- The title and its pages (a saved game to list).
game.save_name = "deutsch"
game.save()
require("ui.menu").open_title(game.menu)
step(3)
game.menu.chosen = game.saves_list()[1]
for _, page in ipairs({ "load", "save", "confirm", "title" }) do
  game.menu.page, game.menu.at = page, 1
  step(3)
end
game.menu.note = "This game is being played"
step(3)
require("ui.menu").close(game.menu)
step(3)
local missed = {}
for _, text in ipairs(i18n.collect(false)) do
  local value = text:match("^%(value%) (.*)$")
  -- A generated name or a version may stay; anything else needs an entry.
  if not value or value:match("^%l") then missed[#missed + 1] = text end
end
expect(#missed == 0, "without German: " .. table.concat(missed, " | "))
settings.set(game.settings, "language", "en")
expect_eq(i18n.code(), "en")
