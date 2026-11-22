-- SPDX-License-Identifier: MIT
-- In company: quick messages from a fixed list reach every heir with the
-- sender's name, in each heir's own language; the host's Company page can
-- remove a heir after a question and close the session to new heirs (the
-- session itself is tested between processes in tests/unit/net/test_net_coop.c).
-- spec: coop-sessions: Removed and talking
scenario { name = "company" }
local game = demo.game
local quick = require("coop.quick")
local menu = require("ui.menu")
local settings = require("ui.settings")
step(5)
local function news()
  local out = {}
  for _, l in ipairs(demo.pack.hud_lines(game.pack)) do out[#out + 1] = type(l) == "table" and l.raw or l end
  return table.concat(out, " | ")
end
-- A quick message from player 2 arrives: shown with the sender's name.
game.coop.inbox = { { event = "data", channel = "quick", player = 2, value = { id = "wait" } },
  { event = "data", channel = "other", player = 1, value = {} },
  { event = "data", channel = "quick", player = 1, value = { id = "nonsense" } } }
step(2)
expect(news():find("Heir 3: Wait", 1, true), news())
expect_eq(#game.coop.inbox, 1, "only the quick messages are taken")
-- In German the same message reads in German.
settings.set(game.settings, "language", "de")
local lines = quick.take({ { channel = "quick", player = 0, value = { id = "follow" } } },
  function() return "Bahar" end)
expect_eq(lines[1], "Bahar: Folgt mir")
settings.set(game.settings, "language", "en")
expect(quick.send("nonsense") == nil, "only the list is sent")
-- The Company page: the quick messages (and, for the host, removing and closing).
input.inject("quit", 1, 1)
step(2)
game.menu.page = "company"
local function page()
  local out = {}
  for _, l in ipairs(menu.lines(game.menu)) do out[#out + 1] = type(l) == "table" and l.raw or l end
  return table.concat(out, " | ")
end
expect(page():find("Follow me", 1, true) and page():find("Help!", 1, true), page())
game.menu.at = 2 -- Wait
input.inject("confirm", 1, 1)
step(2)
expect(not game.menu.open and news():find(": Wait", 1, true), "sent and shown: " .. news())
-- Removing asks first; without a session it is refused with a reason.
input.inject("quit", 1, 1)
step(2)
game.menu.page, game.menu.chosen_heir = "remove", { player = 2, name = "Bahar" }
expect(page():find("Remove Bahar from the session?", 1, true), page())
game.menu.at = 2
input.inject("confirm", 1, 1)
step(2)
expect_eq(game.menu.page, "company")
expect(page():find("only the host removes", 1, true), page())
local ok, why = net.kick(1)
expect(not ok and why, "no session: nothing to remove")
net.set_open(false)
net.set_open(true)
expect(net.status().open)
input.inject("quit", 1, 1)
step(2)
input.inject("quit", 1, 1)
step(2)
