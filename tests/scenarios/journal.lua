-- SPDX-License-Identifier: MIT
-- The journal: the current goal with its place, the requests people made
-- (who and where), what is finished, and the last lines said with the game
-- time (at most 100), kept with the save; holding use for a second hurries
-- Azhdar's arrival and the hiding, every line still said.
-- spec: narrative-director: Goals and what was said
-- spec: narrative-director: Hurrying the hiding
scenario { name = "journal" }
local game = demo.game
local journal = require("ui.journal")
local menu = require("ui.menu")
local prologue = require("story.prologue")
local pack = demo.pack
local P = game.world_places.places
for _ = 1, 900 do
  step(1)
  if P.ready then break end
  sleep(5)
end
prologue.ARRIVAL, prologue.HIDING = 30, 60
local j = game.journal
-- Lines said go in, with the time; a repeat once.
pack.say(game.pack, "Bhimuget: Peace be with you, child of the road.")
step(1)
pack.say(game.pack, "Bhimuget: Peace be with you, child of the road.")
step(1)
local n = 0
for _, l in ipairs(j.lines) do if l.text:find("Bhimuget", 1, true) then n = n + 1 end end
expect_eq(n, 1, "said once")
-- A request heard in talk, once per place.
for _ = 1, 2 do
  game.world_places.folk.heard = { { who = "Kesam", where = "Anarvan", text = "Wolves take our goats." } }
  step(1)
end
expect_eq(#j.requests, 1)
expect_eq(j.requests[1].who, "Kesam")
-- A turn of the story: the chores are finished.
input.inject("story", 1, 1)
step(3)
local S = game.story
S.chores = { water = true, goats = true, pomegranates = true }
S.talked = true
step(2)
expect_eq(S.beat, "arrival")
expect_eq(j.done[1], "Do your chores and talk to Farrokh")
-- Holding use hurries Azhdar's arrival; his demand is still said.
input.inject("use", 1, 0, 70)
step(70)
expect_eq(S.beat, "handover", "hurried")
local demand = false
for _, l in ipairs(S.lines) do demand = demand or l:find("Give me the Weave", 1, true) ~= nil end
expect(demand, "Azhdar's demand was said")
-- The hiding hurried: every fragment said.
S.beat, S.t, S.fragment = "hiding", 0, 0
expect(prologue.hurry(S))
expect_eq(S.fragment, #prologue.FRAGMENTS)
expect_eq(S.lines[#S.lines], prologue.FRAGMENTS[#prologue.FRAGMENTS])
expect(S.t >= prologue.HIDING)
expect(not prologue.hurry({ beat = "village" }), "nothing to hurry in the village")
-- The page.
input.inject("quit", 1, 1)
step(2)
game.menu.page = "journal"
local shown = {}
for _, l in ipairs(menu.lines(game.menu)) do shown[#shown + 1] = type(l) == "table" and l.raw or l end
local page = table.concat(shown, " | ")
expect(page:find("Journal", 1, true) and page:find("Story:", 1, true), page)
expect(page:find("Kesam in Anarvan: Wolves take our goats.", 1, true), page)
expect(page:find("Finished:", 1, true) and page:find("Do your chores", 1, true), page)
expect(page:find("What was said:", 1, true) and page:find("day %d+, %d%d:%d%d: "), page)
input.inject("quit", 1, 1)
step(2)
input.inject("quit", 1, 1)
step(2)
-- At most 100 lines; kept with the save.
for i = 1, 150 do journal.note(j, "line " .. i, i) end
expect_eq(#j.lines, journal.KEEP)
expect_eq(j.lines[#j.lines].text, "line 150")
local back = journal.new(journal.snapshot(j))
expect(#back.lines == journal.KEEP and back.requests[1].who == "Kesam" and back.done[1] == j.done[1])
expect(#journal.new(nil).lines == 0 and #journal.new({ lines = 3 }).lines == 0, "a broken save keeps nothing broken")
