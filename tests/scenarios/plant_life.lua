-- SPDX-License-Identifier: MIT
-- Plants by the carpet: each with its identity, stage and parts; ground cover
-- taken whole grows back within weeks, a felled tree does not; the cuts are
-- kept with the world.
-- spec: plant-life-cycle: Plants from Lua
scenario { name = "plant life" }
local plants = require("life.plants")
local game = demo.game
local here = game.carpet.pos
local found = {}
for _ = 1, 900 do
  step(1)
  found = world.plants(here, 120)
  if #found > 50 then break end
  sleep(5)
end
expect(#found > 50, "plants around the carpet: " .. #found)
local state = plants.new()
local now = world.time().time
local near = plants.near(state, here, 120, now)
local seen = {}
local herb, tree
for _, p in ipairs(near) do
  expect(math.type(p.id) == "integer" and p.size >= 0 and p.size <= 1 and p.variant >= 0 and p.variant <= 255)
  expect(p.stage and p.stage.name, "a stage")
  seen[p.id] = true
  for _, offer in ipairs(p.offers) do
    expect(offer.count >= 1)
    if offer.whole and p.layer == "ground" and not herb then herb = { plant = p, item = offer.item } end
    if offer.whole and p.layer == "tree" and not tree then tree = { plant = p, item = offer.item } end
  end
end
local ids = 0
for _ in pairs(seen) do ids = ids + 1 end
expect_eq(ids, #near, "every plant its own id")
expect(herb and tree, "ground cover and a tree to take whole")
-- Taken whole: gone from the world.
local function present(id)
  for _, p in ipairs(world.plants(here, 120)) do
    if p.id == id then return true end
  end
  return false
end
local item, n = plants.take(state, herb.plant, herb.item, now, world.sky(here).season)
expect(item == herb.item and n >= 1, tostring(n))
local _, felled = plants.take(state, tree.plant, tree.item, now, world.sky(here).season)
expect(felled >= 1)
expect(not present(herb.plant.id) and not present(tree.plant.id), "cut")
expect(not world.cut_plant(herb.plant.id), "already gone")
expect(not world.cut_plant(12345), "no such plant")
expect(not pcall(world.cut_plant, -1))
-- Three game weeks later the ground cover is back; the tree takes years.
world.set_time(now + 21 * plants.DAY)
local back = false
for _ = 1, 600 do
  step(1)
  if present(herb.plant.id) then
    back = true
    break
  end
  sleep(5)
end
expect(back, "the ground cover grew back")
expect(not present(tree.plant.id), "the tree has not")
