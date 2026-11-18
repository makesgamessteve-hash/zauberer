-- SPDX-License-Identifier: MIT
-- Unit tests of the endless bag (assets/base/scripts/player/bag.lua).
local bag = require("player.bag")
local pack = require("player.pack")
local news = require("player.news")

local function said(b, text)
  for _, line in ipairs(news.lines(b.news)) do
    if line:find(text, 1, true) then return true end
  end
  return false
end

-- spec: endless-bag: Folds, not weight
test.case("folds hold stacks, without weight, and the living are refused", function()
  local b = bag.new()
  test.expect_eq(b.folds, bag.FOLDS)
  test.expect_eq(bag.put(b, "wood", 120, 0), 120) -- 120 kg of wood: three folds, no weight
  test.expect_eq(bag.folds_used(b), 3)
  test.expect_eq(bag.put(b, "wood", 30, 0), 30) -- (fills its last fold)
  test.expect_eq(bag.folds_used(b), 3)
  for _, id in ipairs({ "acorns", "beechnuts", "hazelnuts", "dates", "resin", "oak_bark", "birch_bark", "coconut" }) do
    bag.put(b, id, 1, 0)
  end
  test.expect_eq(bag.folds_used(b), 11)
  test.expect_eq(bag.put(b, "pine_nuts", 70, 0), 50, "one fold left")
  test.expect(said(b, "The bag is full"))
  test.expect_eq(bag.put(b, "reed_roots", 1, 0), 0)
  test.expect(not bag.put_creature(b, { alive = true }) and said(b, "will not take the living") and b.refused == 1)
  test.expect(not bag.put_creature(b, { alive = false }) and said(b, "Butcher it first"))
  local n, load, reached = bag.take(b, "wood", 200)
  test.expect(n == 150 and load == 0 and not reached)
  test.expect_error(function() bag.put(b, "gold", 1, 0) end, "no such item")
end)

-- spec: endless-bag: The vault through the bag
test.case("the vault's shelves are reached through the bag for void mana", function()
  local b = bag.new()
  bag.put(b, "cooked_meat", 10, 0.2)
  test.expect_eq(bag.to_vault(b, "cooked_meat", 10), 10)
  test.expect(bag.count(b, "cooked_meat") == 0 and bag.in_vault(b, "cooked_meat") == 10)
  -- Without a mage (no mana to pay with) the vault is out of reach.
  test.expect_eq(bag.take(b, "cooked_meat", 4), 0)
  local mage = { mana = { void = 0.5 } }
  test.expect_eq(bag.take(b, "cooked_meat", 4, mage), 0)
  test.expect(said(b, "Too little void mana"))
  mage.mana.void = 5
  local n, load, reached = bag.take(b, "cooked_meat", 4, mage)
  test.expect(n == 4 and reached and math.abs(load - 0.2) < 1e-9 and mage.mana.void == 5 - bag.REACH_MANA)
  test.expect(said(b, "You reach into the vault"))
  test.expect_eq(bag.take(b, "cooked_meat", 4, mage), 0, "busy reaching")
  test.expect(said(b, "still reaching"))
  bag.tick(b, bag.REACH_SECONDS)
  test.expect_eq(select(1, bag.take(b, "cooked_meat", 4, mage)), 4)
  -- In the vault, goods move freely.
  test.expect_eq(bag.from_vault(b, "cooked_meat", 9), 2)
  test.expect(bag.count(b, "cooked_meat") == 2 and bag.in_vault(b, "cooked_meat") == 0)
  test.expect_eq(bag.from_vault(b, "wood", 1), 0)
end)

-- spec: endless-bag: Stowing and slowed time
test.case("the pack is stowed and the road's needs come out; time is slowed", function()
  local b = bag.new()
  local p = pack.new()
  pack.add(p, "cooked_meat", 4, 0)
  pack.add(p, "boiled_water", 6, 0)
  pack.add(p, "deadwood", 5, 0)
  local moved = bag.stow(b, p)
  test.expect(moved.cooked_meat == 4 and moved.boiled_water == 6 and pack.weight(p) == 0)
  bag.put(b, "wood", 100, 0)
  bag.put(b, "raw_meat", 2, 0.4)
  local out = bag.unstow(b, p)
  test.expect(out.cooked_meat == 4 and out.raw_meat == 2 and out.boiled_water == 6, "food and drink first")
  test.expect(pack.weight(p) <= pack.CARRY + 1e-9 and (out.wood or 0) < 100, "as much as can be carried")
  test.expect(math.abs(pack.load(p, "raw_meat") - 0.4) < 1e-9, "its load comes with it")
  -- Spoiling: a day in the bag is like a tenth of a day outside.
  local q = pack.new()
  pack.add(q, "raw_meat", 1, 0)
  bag.put(b, "raw_meat", 1, 0)
  bag.to_vault(b, "wood", 999)
  pack.spoil(q, 24, 25, false)
  bag.spoil(b, 24, 25)
  test.expect(bag.count(b, "raw_meat") == 1 and b.stacks.raw_meat.load < pack.load(q, "raw_meat") / 3)
  -- Kept with the game; broken saves keep what they can.
  local back = bag.new(bag.snapshot(b))
  test.expect(bag.count(back, "raw_meat") == 1 and bag.in_vault(back, "wood") == bag.in_vault(b, "wood"))
  local odd = bag.new({ folds = "x", stacks = { gold = { count = 1 }, wood = { count = 0 }, acorns = 3 } })
  test.expect(odd.folds == bag.FOLDS and next(odd.stacks) == nil)
  test.expect(#bag.hud_lines(bag.new()) == 0 and bag.hud_lines(back)[1]:find("Bag 1/12 folds", 1, true))
end)
