-- SPDX-License-Identifier: MIT
-- Unit tests of the spellbook (assets/base/scripts/magic/spellbook.lua).
local spellbook = require("magic.spellbook")

local FIREBALL = { verb = "move", colors = { "fire" }, form = "line", power = 10 }
local HOLD = { verb = "bind", colors = { "water" }, form = "circle", power = 4, modifiers = { "lasting" } }

-- spec: spellbook: Discovery and familiarity
test.case("discovering weaves and growing familiar", function()
  local book = spellbook.new()
  test.expect_eq(spellbook.casts(book, FIREBALL), 0)
  local e, new = spellbook.record_cast(book, FIREBALL)
  test.expect(new)
  test.expect_eq(e.name, "Fireball")
  test.expect_eq(spellbook.take_news(book)[1], "You discovered Fireball")
  test.expect_eq(#spellbook.take_news(book), 0)
  local _, again = spellbook.record_cast(book, FIREBALL)
  test.expect(not again)
  local reached
  for _ = 3, 10 do
    local _, _, tier = spellbook.record_cast(book, FIREBALL)
    reached = tier or reached
  end
  test.expect_eq(reached, "Practised")
  test.expect_eq(spellbook.casts(book, FIREBALL), 10)
  test.expect_eq(spellbook.take_news(book)[1], "Fireball is now Practised")
  local _, learned = spellbook.learn(book, HOLD, "book")
  test.expect(learned)
  local list = spellbook.list(book)
  test.expect_eq(#list, 2)
  test.expect_eq(list[1].tier, "Practised")
  test.expect_eq(list[2].name, "Bind Water (circle)")
end)

-- spec: spellbook: Prepared weaves and hand slots
test.case("preparing weaves for the hands and the wheel", function()
  local book = spellbook.new()
  local i, err = spellbook.prepare(book, FIREBALL)
  test.expect(i == nil and err == "not in the journal")
  spellbook.learn(book, FIREBALL)
  spellbook.learn(book, HOLD)
  test.expect_eq(spellbook.prepare(book, FIREBALL), 1)
  test.expect_eq(spellbook.prepare(book, FIREBALL), 1, "once")
  test.expect_eq(spellbook.prepare(book, HOLD), 2)
  test.expect(spellbook.assign(book, "right", 1))
  test.expect(spellbook.assign(book, 8, 2))
  test.expect(not spellbook.assign(book, 9, 2))
  test.expect(not spellbook.assign(book, "left", 7))
  test.expect_eq(spellbook.in_slot(book, "right").form, "line")
  test.expect_eq(spellbook.in_slot(book, 8).modifiers[1], "lasting")
  test.expect_eq(spellbook.in_slot(book, "left"), nil)
  -- A full book.
  local full = spellbook.new()
  for k = 1, spellbook.MAX_PREPARED + 1 do
    local w = { verb = "create", colors = { "fire" }, form = "tap", power = 1, modifiers = {} }
    for _ = 1, k % 4 do w.modifiers[#w.modifiers + 1] = "far" end
    w.form = ({ "tap", "line", "circle", "spiral", "triangle", "square", "zigzag", "wave", "tap" })[1 + k % 9]
    w.verb = ({ "create", "unmake", "move", "shape" })[1 + k % 4]
    spellbook.learn(full, w)
    spellbook.prepare(full, w)
  end
  test.expect(#full.prepared <= spellbook.MAX_PREPARED)
end)

-- spec: spellbook: The spellbook is saved
test.case("saving and restoring", function()
  local book = spellbook.new()
  for _ = 1, 12 do spellbook.record_cast(book, FIREBALL) end
  spellbook.learn(book, HOLD)
  spellbook.prepare(book, FIREBALL)
  spellbook.assign(book, "left", 1)
  spellbook.assign(book, 3, 1)
  local back = spellbook.restore(spellbook.snapshot(book))
  test.expect_eq(spellbook.casts(back, FIREBALL), 12)
  test.expect_eq(spellbook.in_slot(back, "left").verb, "move")
  test.expect_eq(spellbook.in_slot(back, 3).form, "line")
  test.expect_eq(#spellbook.list(back), 2)
  test.expect_eq(#spellbook.take_news(back), 0, "no news from a restore")
  test.expect_eq(#spellbook.list(spellbook.restore(nil)), 0)
  local bad = spellbook.restore({ journal = { { weave = { verb = "burn", colors = { "fire" }, form = "tap" } } } })
  test.expect_eq(#spellbook.list(bad), 0)
end)
