-- SPDX-License-Identifier: MIT
-- The endless bag in the game: I stows the pack's goods in the bag's folds
-- and O takes the road's needs back out; food the folds lack is reached
-- from the vault for void mana; the bag is kept with the game.
-- spec: endless-bag: The bag in the game
scenario { name = "bag" }
local game = demo.game
local pack = demo.pack
local bag = demo.bag
step(30)
local function news(text)
  for _, line in ipairs(demo.flight_lines()) do
    if line:find(text, 1, true) then return true end
  end
  return false
end
local function press(action)
  input.inject(action, 1, 0, 1)
  step(2)
end
local p, b = game.pack, game.bag
pack.add(p, "cooked_meat", 6, 0)
pack.add(p, "boiled_water", 4, 0)
pack.add(p, "wood", 5, 0)
press("stow")
expect(pack.weight(p) == 0 and bag.count(b, "cooked_meat") == 6 and bag.count(b, "wood") == 5, "stowed")
expect(news("Stowed") and news("Bag 3/12 folds"), "the HUD says so")
bag.put(b, "wood", 200, 0) -- (no weight in the bag)
press("unstow")
expect(pack.count(p, "cooked_meat") == 6 and pack.count(p, "boiled_water") == 4, "food and drink first")
expect(pack.weight(p) <= pack.CARRY + 1e-9 and bag.count(b, "wood") > 0, "only what can be carried")
expect(news("Taken out"))
-- Food on the vault's shelves comes out through the bag, for void mana.
press("stow")
bag.to_vault(b, "cooked_meat", 99)
bag.to_vault(b, "boiled_water", 99)
local mage = game.magic.caster.mage
mage.mana.void = 10
press("unstow")
expect(pack.count(p, "cooked_meat") == 6 and mage.mana.void == 10 - bag.REACH_MANA, "reached from the vault")
expect(news("You reach into the vault"))
step(60 * bag.REACH_SECONDS + 5)
press("stow")
press("unstow")
expect(pack.count(p, "boiled_water") == 4 and mage.mana.void == 10 - 2 * bag.REACH_MANA, "and the water")
local back = bag.new(bag.snapshot(b))
expect(bag.in_vault(back, "cooked_meat") == bag.in_vault(b, "cooked_meat"), "kept with the game")
