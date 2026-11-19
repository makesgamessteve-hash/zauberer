-- SPDX-License-Identifier: MIT
-- Unit tests of the disciplines (assets/base/scripts/magic/disciplines.lua).
local disciplines = require("magic.disciplines")
local news = require("player.news")

-- spec: discipline-progression: Practice, study and trials
test.case("experience stops at a threshold until the trial is passed; mastery betters the work", function()
  local s = disciplines.new()
  test.expect(s.tier.alchemy == 1 and disciplines.factor(s, "alchemy") == 1)
  test.expect(disciplines.factor(s, "ritual", true) == 1)
  disciplines.practice(s, "alchemy", 150, "brew", "healing_draught")
  test.expect(s.tier.alchemy == 1 and s.xp.alchemy == 100, "held at the threshold: the trial wants two kinds")
  disciplines.practice(s, "alchemy", 10, "brew", "healing_draught")
  test.expect(s.tier.alchemy == 1)
  disciplines.practice(s, "alchemy", 10, "brew", "spirit")
  test.expect(s.tier.alchemy == 2, "Apprentice")
  local said = false
  for _, line in ipairs(news.lines(s.news)) do said = said or line == "Alchemy: Apprentice" end
  test.expect(said)
  test.expect(math.abs(disciplines.factor(s, "alchemy") - 1.1) < 1e-9 and disciplines.factor(s, "alchemy", true) < 1)
  -- Study counts as experience without events (the trial still holds the tier).
  disciplines.study(s, "ritual", 10)
  test.expect(s.xp.ritual == 100 and s.tier.ritual == 1)
  disciplines.practice(s, "ritual", 30, "ritual", "call_rain")
  test.expect(s.tier.ritual == 2)
  disciplines.practice(s, "nothing", 100, "x")
  -- The page.
  s.shown = true
  local lines = disciplines.hud_lines(s)
  local page = table.concat(lines, "\n")
  test.expect(page:find("alchemy      Apprentice", 1, true) and page:find("naturalism   Novice     0/100", 1, true))
  disciplines.practice(s, "summoning", 200)
  test.expect(table.concat(disciplines.hud_lines(s), "\n"):find("trial awaits", 1, true))
  -- Kept with the game; broken saves keep what they can.
  local back = disciplines.new(disciplines.snapshot(s))
  test.expect(back.tier.alchemy == 2 and back.done.alchemy.kinds.brew.spirit and back.done.alchemy.counts.brew == 3)
  local odd = disciplines.new({ alchemy = { tier = 9, xp = -5 }, ritual = "x" })
  test.expect(odd.tier.alchemy == 5 and odd.xp.alchemy == 0 and odd.tier.ritual == 1)
end)
