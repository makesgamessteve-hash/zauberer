-- SPDX-License-Identifier: MIT
-- Harvest (concept 5.12, 7.2): what the mage on foot can take within reach:
-- a carcass to butcher (its products, less what was eaten of it), a part of
-- a plant (by its life cycle; trees are not felled by hand), water from the
-- edge (four cups, of a kind by where it is), or the mushrooms of the patch.
-- What is taken is a sample of its kind for the Codex, and the right method
-- (life.method) gives more.
local fauna = require("life.fauna")
local items = require("life.items")
local plants = require("life.plants")
local smalllife = require("life.smalllife")
local pack = require("player.pack")

local harvest = {}

harvest.REACH = 3   -- metres
harvest.CUPS = 4    -- cups of water filled at once

local WATER_ITEM = { sea = "sea_water", lake = "lake_water", river = "river_water", pool = "pool_water",
  swamp = "swamp_water" }

local function on_ground(pos)
  local p = world.move(pos, 0, 0, 0)
  p.alt = world.ground_height(pos)
  return p
end

-- Butchers a carcass into the pack (factor: of the right method, default 1);
-- returns { item = count }.
function harvest.butcher(p, id, factor)
  local c = sim.creature(id)
  local sp = c and fauna.species()[c.species]
  if not c or c.alive or not sp or not sp.products then return nil end
  local got = {}
  for item, n in pairs(sp.products) do
    local share = item == "raw_meat" and (1 - (c.eaten or 0)) or 1
    local count = math.floor(n * share * (factor or 1) + 0.5)
    if count > 0 then got[item] = pack.add(p, item, count, 0) end
  end
  sim.remove_creature(id)
  return got
end

local function nearest_carcass(pos)
  local best, d = nil, math.huge
  for _, id in ipairs(sim.creatures_near(on_ground(pos), harvest.REACH + 1)) do
    local c = sim.creature(id)
    if c and not c.alive then
      local dd = world.distance(c.pos, pos)
      if dd < d then best, d = id, dd end
    end
  end
  return best
end

-- The best part to take from the plants within reach: the nearest plant with
-- an offer that is not a whole tree.
local function nearest_offer(state, pos, now)
  local season = world.sky(pos).season
  local best, bd
  for _, pl in ipairs(world.plants(pos, harvest.REACH)) do
    for _, offer in ipairs(plants.available(state, pl, now, season)) do
      local by_hand = not offer.whole or pl.layer ~= "tree" or pl.species == "snag"
      local d = world.distance(pl.pos, pos)
      if by_hand and (not best or d < bd or (d == bd and not offer.whole and best.offer.whole)) then
        best, bd = { plant = pl, offer = offer, season = season }, d
      end
    end
  end
  return best
end

-- Water within reach: its kind and where.
local function water_near(pos)
  for _, d in ipairs({ 0, 1.5, 3 }) do
    for k = 0, 7 do
      local q = world.move(pos, k * math.pi / 4, d, 0)
      local w = smalllife.water(q)
      if w then return w end
    end
  end
  return nil
end

-- Gathers what is within reach of pos into pack p; returns what was taken
-- ({ what, items = { id = count }, sources = { Codex entry } }), or nil and why.
-- life: { plants = plant state, smalllife = small-life state, method =
-- function(Codex entry) -> yield factor (optional) }
function harvest.gather(p, life, pos, now)
  local method = life.method or function() return 1 end
  local carcass = nearest_carcass(pos)
  if carcass then
    local entry = "animal:" .. sim.creature(carcass).species
    local got = harvest.butcher(p, carcass, method(entry))
    if got then return { what = "carcass", items = got, sources = { entry } } end
  end
  local offer = nearest_offer(life.plants, pos, now)
  if offer then
    local item, n = plants.take(life.plants, offer.plant, offer.offer.item, now, offer.season)
    if item then
      local entry = "plant:" .. offer.plant.species
      local factor = method(entry)
      if smalllife.pollination(life.smalllife, pos) > 0 and items.get(item).kind == "food" then
        factor = factor * 1.2 -- the bees did their work
      end
      local count = math.floor(n * factor + 0.5)
      return { what = "plant", items = { [item] = pack.add(p, item, count, 0) }, species = offer.plant.species,
        sources = { entry } }
    end
  end
  local w = water_near(pos)
  if w then
    local item = WATER_ITEM[w.kind] or "pool_water"
    return { what = "water", items = { [item] = pack.add(p, item, harvest.CUPS, 0) }, sources = {} }
  end
  local picked, found = smalllife.pick(life.smalllife, pos, now) -- (anywhere in the patch)
  if picked then
    local got, sources = {}, {}
    for _, m in ipairs(found) do
      local entry = "fungus:" .. m.fungus
      local n = math.floor(m.count * method(entry) + 0.5)
      got[m.item] = (got[m.item] or 0) + pack.add(p, m.item, n, 0)
      sources[#sources + 1] = entry
    end
    return { what = "mushrooms", items = got, sources = sources }
  end
  return nil, "nothing to gather here"
end

return harvest
