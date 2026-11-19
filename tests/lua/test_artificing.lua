-- SPDX-License-Identifier: MIT
-- Unit tests of crafting (magic/crafting.lua) and artificing (magic/artificing.lua).
local crafting = require("magic.crafting")
local artificing = require("magic.artificing")
local condition = require("flight.condition")
local bag = require("player.bag")
local pack = require("player.pack")

local function craft(id)
  for _, c in ipairs(crafting.crafts()) do
    if c.id == id then return c end
  end
end

-- spec: crafting: The workbench
test.case("cord from any thread, a lantern, and the carpet mended fully", function()
  local p = pack.new()
  local made, why = crafting.craft(craft("cord"), p)
  test.expect(not made and why == "not enough: thread 3")
  pack.add(p, "reed_fibre", 2, 0)
  pack.add(p, "palm_fibre", 2, 0)
  test.expect_eq(crafting.thread(p), 4)
  local hours
  made, hours = crafting.craft(craft("cord"), p)
  test.expect(made.cord == 1 and hours == 0.5 and crafting.thread(p) == 4 + 1, "a cord (4 thread) and one fibre left")
  pack.add(p, "deadwood", 1, 0)
  pack.add(p, "resin", 1, 0)
  pack.add(p, "hide", 1, 0)
  test.expect(crafting.line(craft("lantern"), p):find("Work: Horn lantern (cord 1/1, dry deadwood 1/1, hide 1/1", 1,
    true))
  made = crafting.craft(craft("lantern"), p)
  test.expect(made.lantern == 1 and pack.count(p, "cord") == 0 and pack.count(p, "hide") == 0)
  -- The workshop's repair mends the carpet fully.
  local cond = condition.new({ integrity = 30 })
  test.expect(select(2, crafting.craft(craft("carpet_repair"), p, cond, condition)):find("cord 2", 1, true))
  pack.add(p, "cord", 2, 0)
  made = crafting.craft(craft("carpet_repair"), p, cond, condition)
  test.expect(made and cond.integrity == 100 and pack.count(p, "cord") == 0)
  pack.add(p, "cord", 2, 0)
  test.expect(select(2, crafting.craft(craft("carpet_repair"), p, cond, condition)) == "the carpet is whole")
end)

-- spec: artificing: Enchanting
test.case("a lantern of perpetual light; carpet upgrades; folds for the bag", function()
  local p, b = pack.new(), bag.new()
  local cond = condition.new()
  local mage = { mana = { light = 5, water = 30, storm = 30, void = 30, fire = 0 } }
  local list = {}
  local lantern = artificing.get("lantern_of_light")
  local cost = artificing.cost(lantern)
  test.expect(math.abs(cost.light - 20 * 0.9) < 1e-9, "the horn lantern carries light")
  local ctx = { pack = p, mage = mage, condition = cond, cmod = condition, bag = b, artifacts = list }
  local made, why = artificing.enchant(lantern, ctx)
  test.expect(not made and why:find("horn lantern 1", 1, true) and why:find("light mana", 1, true))
  pack.add(p, "lantern", 1, 0)
  mage.mana.light = 40
  test.expect(artificing.line(lantern, p, mage):find("Enchant: Lantern of perpetual light on horn lantern", 1, true))
  made = artificing.enchant(lantern, ctx)
  test.expect(made and #list == 1 and list[1].charge == lantern.capacity and pack.count(p, "lantern") == 0)
  test.expect(math.abs(mage.mana.light - (40 - 18)) < 1e-9)
  -- Carpet upgrades go into its slots; folds into the bag.
  pack.add(p, "resin", 2, 0)
  test.expect(artificing.enchant(artificing.get("weatherproof_weave"), ctx) == "Weatherproof weave")
  test.expect(condition.has(cond, "weatherproof") and mage.mana.water < 30)
  pack.add(p, "resin", 2, 0)
  test.expect(select(2, artificing.enchant(artificing.get("weatherproof_weave"), ctx)) == "already woven in")
  pack.add(p, "cord", 2, 0)
  test.expect(artificing.enchant(artificing.get("folding_knot"), ctx) == "Folding knot" and b.folds == bag.FOLDS + 6)
  -- Kept with the game.
  local back = artificing.restore(artificing.snapshot(list))
  test.expect(#back == 1 and back[1].id == "lantern_of_light" and #artificing.restore({ { id = "x" }, 5 }) == 0)
end)

-- spec: artificing: Artifacts at work
test.case("a passive artifact drains and refills from its source", function()
  local list = { { id = "lantern_of_light", charge = 1 } }
  artificing.tick(list, 50, {}) -- no source within reach: it drains
  test.expect(list[1].charge == 0.5 and list[1].lit)
  artificing.tick(list, 100, {})
  test.expect(list[1].charge == 0 and not list[1].lit and not artificing.working(list, "lantern_of_light"))
  test.expect(artificing.hud_lines(list)[1] == "Lantern of perpetual light: dark")
end)
