-- SPDX-License-Identifier: MIT
-- The title: at the first start it offers a new game (no Continue) and the
-- version; once games are saved it offers Continue and Load game, which lists
-- them newest first with their day; a game is loaded from the list, and
-- deleted only when the question is answered yes (not the one being played).
-- spec: game-flow: First start and later starts
-- spec: game-flow: Loading and deleting
scenario { name = "title" }
local game = demo.game
local menu = require("ui.menu")
local m = game.menu
step(10)
expect_eq(game.mode, "flight")

local function text_of(l) return type(l) == "table" and l.raw or l end
local function lines() -- the menu's lines as text
  local out = {}
  for _, l in ipairs(menu.lines(m)) do out[#out + 1] = text_of(l) end
  return out
end
local function shown(label)
  for _, l in ipairs(lines()) do if l:find(label, 1, true) then return true end end
  return false
end
-- Chooses the item whose line shows the label (the cursor moves there) and confirms it.
local function pick(label)
  local row = 0
  for _, l in ipairs(lines()) do
    if l:sub(1, 2) == "> " or l:sub(1, 2) == "  " then
      row = row + 1
      if l:find(label, 1, true) then
        m.at = row
        input.inject("confirm", 1, 1)
        step(2)
        return true
      end
    end
  end
  error("no item " .. label .. " in: " .. table.concat(lines(), " | "))
end

-- The first start: a new game, no Continue, the version.
menu.open_title(m)
step(2)
expect(m.open and m.page == "title")
expect(shown("Zauberer") and shown("v" .. engine.info().version), table.concat(lines(), " | "))
expect(shown("New game") and not shown("Continue") and not shown("Load game"), table.concat(lines(), " | "))
local t = world.time().time
step(10)
expect_eq(world.time().time, t, "the world waits at the title")
input.inject("quit", 1, 1) -- (Escape does not leave the title)
step(2)
expect(m.open and m.page == "title")
pick("New game")
expect(not m.open, "the game begins")

-- Two saved games: the first here, the second 500 m on and a day later.
game.save_name = "first"
local here = world.move(game.carpet.pos, 0, 0, 0, 0)
expect(game.save())
game.carpet.pos = world.move(game.carpet.pos, 0, 500, 0, 0)
world.set_time(world.time().time + 86400)
game.save_name = "second"
expect(game.save())

-- Later starts: Continue and Load game; the list newest first with the day.
menu.open_title(m)
step(2)
expect(shown("Continue") and shown("Load game"), table.concat(lines(), " | "))
pick("Load game")
expect_eq(m.page, "load")
local list = lines()
expect(list[2]:find("second, day", 1, true) and list[3]:find("first, day", 1, true), table.concat(list, " | "))
-- Loading the first.
pick("first")
expect_eq(m.page, "save")
pick("Play")
expect(not m.open, "playing the loaded game")
expect_eq(game.save_name, "first")
expect(world.distance(game.carpet.pos, here) < 5, "where the first game was saved")
step(5)

-- Deleting the second: No keeps it, Yes deletes it.
menu.open_title(m)
step(2)
pick("Load game")
pick("second")
pick("Delete")
expect_eq(m.page, "confirm")
expect(shown("Delete second?"), table.concat(lines(), " | "))
pick("No, keep it")
expect_eq(m.page, "save")
local names = table.concat(world.saves(), " ")
expect(names:find("second", 1, true), "kept: " .. names)
pick("Delete")
pick("Yes, delete it")
names = table.concat(world.saves(), " ")
expect(not names:find("second", 1, true) and names:find("first", 1, true), "deleted: " .. names)
-- The game being played is not deleted.
expect_eq(m.page, "load")
pick("first")
pick("Delete")
pick("Yes, delete it")
expect(shown("This game is being played"), table.concat(lines(), " | "))
expect(table.concat(world.saves(), " "):find("first", 1, true), "still there")
expect(not world.delete_save("../x"), "names that leave the saves are refused")
input.inject("quit", 1, 1)
step(2)
expect_eq(m.page, "title", "back up to the title")
pick("Continue")
expect(not m.open)
