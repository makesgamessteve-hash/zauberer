-- SPDX-License-Identifier: MIT
-- Fire and food (concept 7.2): a campfire laid from the pack's firewood (to
-- be lit with Kindle or any flame); over a fire within reach raw food cooks,
-- water boils and herbs steep in boiled water into teas; and eating and
-- drinking the best of what the pack holds: cooked before raw, remedies when
-- sick, clean water before dirty, the sea last.
local items = require("life.items")
local smalllife = require("life.smalllife")
local survival = require("player.survival")
local pack = require("player.pack")

local cooking = {}

cooking.REACH = 3       -- metres to a fire
cooking.CAMPFIRE = 6    -- kg of firewood a campfire takes at most
cooking.HOT = 150       -- deg C of embers that still cook

-- Whether a fire burns (or embers glow) within reach of pos.
function cooking.fire_near(pos)
  for _, r in ipairs({ 0, 1.5, 3 }) do
    for k = 0, 7 do
      local c = sim.fire_at(world.move(pos, k * math.pi / 4, r, 0))
      if c.loaded and (c.burning or c.temperature > cooking.HOT) then return true end
      if r == 0 then break end
    end
  end
  return false
end

-- Lays a campfire of the pack's firewood at pos; returns the kg laid, or nil
-- and why.
function cooking.campfire(p, pos)
  local kg = 0
  local fuels = {}
  for id, it in pairs(items.all()) do
    if it.fuel and pack.count(p, id) > 0 then fuels[#fuels + 1] = id end
  end
  table.sort(fuels, function(a, b) return items.get(a).fuel > items.get(b).fuel end)
  for _, id in ipairs(fuels) do
    while kg < cooking.CAMPFIRE and pack.count(p, id) > 0 do
      pack.take(p, id, 1)
      kg = kg + items.get(id).fuel
    end
  end
  if kg == 0 then return nil, "no firewood" end
  if sim.add_fuel(pos, kg, 0.6) == 0 then
    return nil, "no place for a fire here"
  end
  return kg
end

-- Cooks, boils and brews everything it can over a fire within reach of pos;
-- returns { [made item] = count }, or nil and why.
function cooking.cook(p, pos)
  if not cooking.fire_near(pos) then return nil, "no fire near" end
  local made = {}
  local function turn(from, to, n)
    local taken = pack.take(p, from, n)
    if taken > 0 then made[to] = (made[to] or 0) + pack.add(p, to, taken, 0) end
  end
  for id, it in pairs(items.all()) do
    local n = pack.count(p, id)
    if n > 0 and it.cooked then turn(id, it.cooked, n) end
    if n > 0 and it.boiled then turn(id, it.boiled, n) end
  end
  for id, it in pairs(items.all()) do -- teas: a herb steeped in a cup of boiled water
    while it.brew and pack.count(p, id) > 0 and pack.count(p, "boiled_water") > 0 do
      pack.take(p, id, 1)
      pack.take(p, "boiled_water", 1)
      made[it.brew] = (made[it.brew] or 0) + pack.add(p, it.brew, 1, 0)
    end
  end
  if next(made) == nil then return nil, "nothing to cook" end
  return made
end

local function best(p, choose)
  local pick, score
  for id, st in pairs(p.stacks) do
    local s = choose(items.get(id), st)
    if s and (not score or s > score) then pick, score = id, s end
  end
  return pick
end

-- Eats the best food in the pack into survival s; returns its name, or nil
-- and why. warned(item): whether its dangers are known (default: all are);
-- an unknown poison is eaten like any food.
function cooking.eat(p, s, warned)
  local id = best(p, function(it, st)
    if it.kind ~= "food" and not (it.kind == "herb" and s.sick > 0 and it.cures) then return nil end
    if st.load >= smalllife.ROTTEN then return nil end
    local value = (it.kcal or 0) / 100
    if not it.raw then value = value + 2 end -- cooked or safe as it is
    if st.load >= smalllife.SPOILT then value = value - 5 end
    if (it.poison or 0) > 0 and (not warned or warned(it.id)) then value = value - 10 end
    if s.sick > 0 and it.cures and it.cures.sick then value = value + 5 end
    return value
  end)
  if not id then return nil, "nothing to eat" end
  local _, load = pack.take(p, id, 1)
  survival.consume(s, items.get(id), load)
  return items.name(id)
end

-- Drinks the best drink in the pack into survival s; returns its name, or nil
-- and why.
function cooking.drink(p, s)
  local id = best(p, function(it)
    if it.kind ~= "water" then return nil end
    local value = 10 - (it.dirty or 0) * 10
    if it.salty then value = value - 20 end
    if s.sick > 0 and it.cures and it.cures.sick then value = value + 20 end
    return value
  end)
  if not id then return nil, "nothing to drink" end
  local _, load = pack.take(p, id, 1)
  survival.consume(s, items.get(id), load)
  return items.name(id)
end

return cooking
