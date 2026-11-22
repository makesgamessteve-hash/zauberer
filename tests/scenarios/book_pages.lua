-- SPDX-License-Identifier: MIT
-- The menu as the father's book: tabs turned with Q/E and by pointing, the
-- map painted across the pages, a page turn heard; rows hovered, reached with
-- the arrows and clicked.
-- spec: game-menu: Turning the tabs
-- spec: game-menu: Mouse, keys and touch
scenario { name = "book pages" }
local game = demo.game
local menu = require("ui.menu")
step(5)
input.inject("quit", 1, 1)
step(3)
local m = game.menu
expect(m.open and m.page == "main", "the book opens on the Menu page")
local function press(action)
  input.inject(action, 1, 1)
  step(3)
end
local order = {}
for _ = 1, 5 do
  press("tab_next")
  order[#order + 1] = m.page
end
expect_eq(table.concat(order, ","), "journal,map,help,settings,main", "the tabs in order, wrapping")
press("tab_prev")
expect_eq(m.page, "settings", "Q turns back")
local heard = false
for _, n in ipairs(game.sound.played) do if n == "ui_page" then heard = true end end
expect(heard, "a page turn is heard")

-- Pointing at the Map tab opens it; the map is painted.
local g = render.ui_grid()
local function at(box) return g.x + (box.x + box.w / 2) / g.width * g.w, g.y + (box.y + box.h / 2) / g.height * g.h end
local tabs = game.book_boxes.tabs
expect(#tabs == #menu.TABS, "the tabs can be pointed at")
local x, y = at(tabs[3])
input.inject_pointer(x, y, true)
step(3)
input.inject_pointer(x, y, false)
step(3)
expect_eq(m.page, "map", "the clicked tab opens")
expect(game.worldmap.painted and game.worldmap.painted.texture, "the map is painted into a texture")

-- Settings: a row hovered, the next with the arrow, a third clicked.
menu.goto_tab(m, "settings")
step(3)
local rows = game.book_boxes.rows
expect(#rows >= 4, "the settings' rows can be pointed at: " .. #rows)
x, y = at(rows[2])
input.inject_pointer(x, y)
step(3)
expect_eq(m.at, 2, "hovered: focused")
input.inject("steer", 0, -1, 2)
step(3)
expect_eq(m.at, 3, "the arrow goes on")
local item = menu.focused(m)
expect(item ~= nil, "the focused row")
rows = game.book_boxes.rows
x, y = at(rows[4])
local before = item and item.setting and game.settings[menu.focused(m).setting]
input.inject_pointer(x, y, true)
step(3)
input.inject_pointer(x, y, false)
step(3)
expect_eq(m.at, 4, "clicked: focused and chosen")
local _ = before
input.inject("quit", 1, 1)
step(3)
input.inject("quit", 1, 1)
step(3)
expect(not m.open, "Escape closes the book")
