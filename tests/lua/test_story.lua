-- SPDX-License-Identifier: MIT
-- Unit tests of the story's pure parts (story/duel, finale, aftermath,
-- rivals, narrative).
local duel = require("story.duel")
local finale = require("story.finale")
local aftermath = require("story.aftermath")
local rivals = require("story.rivals")
local narrative = require("story.narrative")

-- spec: story-act-3-and-endings: The duel
test.case("a duel: colours through the ward, a calmed storm, the foe's blows", function()
  local D = finale.duel()
  local weak = duel.cast(D, { colours = { "water" }, power = 1 })
  local strong = duel.cast(D, { colours = { "fire" }, power = 1 })
  local calm = duel.cast(D, { colours = { "water" }, power = 1 }, { calm = true })
  test.expect(weak > strong and calm > weak, "water through his ward, fire hardly, more in the calm")
  local harm = 0
  test.expect_eq(duel.tick(D, finale.AZHDAR.every * 2 + 0.1, function(h) harm = harm + h end), 2)
  test.expect_eq(harm, finale.AZHDAR.attack * 2)
  while not D.over do duel.cast(D, { colours = { "light", "water" }, power = 3 }, { calm = true }) end
  test.expect(D.won and D.hp == 0)
  test.expect(duel.cast(D, { colours = { "water" } }) == 0 and duel.tick(D, 10, error) == 0)
  local E = duel.new({ hp = 10, attack = 1, every = 1, colour = "fire" })
  duel.yield(E)
  test.expect(E.over and not E.won)
end)

-- spec: story-act-3-and-endings: Ready for the storm
test.case("readiness: ritual mastery, an ash ward, a summoned ally", function()
  local ok, why = finale.ready({ ritual = 1 })
  test.expect(not ok and why:find("ritual", 1, true) and why:find("ash ward", 1, true) and why:find("ally", 1, true))
  test.expect(finale.ready({ ritual = finale.RITUAL, ash_ward = true, ally = true }))
end)

-- spec: story-act-3-and-endings: The endings
test.case("four endings; the Reweave only with deep ritual and void", function()
  local plain = finale.endings({ ritual = 1 })
  test.expect(#plain == 3, "unravel, bind, succession")
  local deep = finale.endings({ ritual = 3, void = true })
  test.expect(#deep == 4 and deep[3] == "reweave")
  test.expect_eq(#finale.endings({ ritual = 5 }), 3)
  for _, id in ipairs(deep) do test.expect(narrative.data().endings[id].text) end
end)

-- spec: post-game-world-state: Each people's chronicle of the end
test.case("chronicles of the ending in each people's way", function()
  local free = { values = { "piety" }, magic = "revered" }
  local afraid = { values = { "martial prowess" }, magic = "forbidden" }
  local a = aftermath.chronicle(free, "succession")
  local b = aftermath.chronicle(afraid, "succession")
  test.expect(a:find("heirs of Farrokh", 1, true) and a:find("gods", 1, true) and a:find("kneel", 1, true))
  test.expect(b:find("witch-children", 1, true) and b:find("arms", 1, true) and b:find("curse", 1, true))
  test.expect(aftermath.chronicle(free, "unravel"):find("fade", 1, true))
  local ley = 1
  local ctx = { set_ley = function(v) ley = v end, ley = 1 }
  aftermath.year("unravel", ctx)
  test.expect(ley < 1)
  ctx.ley = 0.11
  aftermath.year("unravel", ctx)
  test.expect_eq(ley, aftermath.FLOOR)
  ley = 0.5
  aftermath.year("bind", ctx)
  test.expect_eq(ley, 0.5)
end)

-- spec: rival-mages-and-domains: Mages and how they take the heirs
test.case("meeting, truces, apprenticeships, alliances and betrayal", function()
  local function mage(p, extra)
    local m = { personality = p, power = 80, reputation = 0, colours = { "fire", "void" }, name = "M",
      relation = "stranger" }
    for k, v in pairs(extra or {}) do m[k] = v end
    return m
  end
  test.expect_eq(rivals.meet(mage("mad"), 999), "hostile")
  test.expect_eq(rivals.meet(mage("territorial"), 10), "hostile")
  test.expect_eq(rivals.meet(mage("territorial"), 100), "trade")
  test.expect_eq(rivals.meet(mage("greedy"), 10), "trade")
  test.expect_eq(rivals.meet(mage("scholarly"), 10), "teach")
  test.expect_eq(rivals.meet(mage("kind", { serves_azhdar = true }), 10), "hostile")
  test.expect(rivals.propose(mage("scholarly"), "apprentice", 10))
  test.expect(not rivals.propose(mage("greedy"), "apprentice", 10))
  test.expect(not rivals.propose(mage("mad"), "truce", 10) and rivals.propose(mage("mad"), "truce", 200))
  local g = mage("greedy")
  test.expect(rivals.propose(g, "alliance", 10) and g.relation == "alliance")
  test.expect(not rivals.propose(mage("greedy", { serves_azhdar = true }), "alliance", 999))
  test.expect(not rivals.propose(g, "marriage", 10))
  test.expect(not rivals.betrays(g, 2) and rivals.betrays(g, 3) and g.relation == "betrayer")
  local D = rivals.contest(mage("mad"))
  test.expect(D.hp == 80 and D.foe.weak[1] == "water")
  test.expect_eq(rivals.contest(mage("mad", { colours = { "storm" } })).foe.weak[1], "fire")
end)

-- spec: rival-mages-and-domains: A domain of the heirs' own
test.case("a won sanctum gives its domain: mana and servants", function()
  local R = { list = {} }
  test.expect_eq(rivals.domain_income(R), 0)
  local m = { domain = { [3] = true, [4] = true }, servants = 3, sanctum = "settlement:1" }
  rivals.defeat(R, m, true)
  local mana_a_day, servants = rivals.domain_income(R)
  test.expect(m.defeated and mana_a_day == 10 and servants == 3)
  rivals.defeat(R, { domain = {}, servants = 0 }, false)
  test.expect_eq(select(2, rivals.domain_income(R)), 3)
end)
