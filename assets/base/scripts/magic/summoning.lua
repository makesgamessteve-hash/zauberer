-- SPDX-License-Identifier: MIT
-- Summoning (concept 3.4): calling, binding and keeping beings. Elementals
-- are formed from local matter and mana (a water elemental needs a body of
-- water close by); they cost mana to call and mana to keep (upkeep from the
-- mage each second). Unpaid, the binding weakens, the more the stronger the
-- being's will, and at nothing the being fades back into the land. A
-- familiar is one wild animal bonded for good: offered what it eats and
-- some life mana close by (unless it is too afraid; the offering and the
-- mana soothe it); it follows the mage and its trait grows with the days.
local items = require("life.items")
local pack = require("player.pack")
local smalllife = require("life.smalllife")

local summoning = {}

summoning.BOND_MANA = 15     -- life mana to bond a familiar
summoning.BOND_REACH = 6     -- metres to the animal
summoning.FOLLOW = 4         -- metres a bound being keeps from the mage
summoning.WEAKEN = 0.05      -- binding lost a second unpaid, times its will over 0.1

local registry
function summoning.data()
  if not registry then
    local text = assert(vfs.read("data/summons.lua"))
    registry = assert(load(text, "=data/summons.lua", "t", {}))()
    for _, f in ipairs(registry.familiars) do
      for id in pairs(f.offering) do assert(items.get(id), "familiar " .. f.species .. ": no item " .. id) end
    end
  end
  return registry
end

function summoning.get(id)
  for _, s in ipairs(summoning.data().summons) do
    if s.id == id then return s end
  end
end

function summoning.new(saved)
  saved = type(saved) == "table" and saved or {}
  local fam = type(saved.familiar) == "table" and saved.familiar or nil
  return { bound = {}, familiar = fam and { species = fam.species, since = tonumber(fam.since) or 0 } or nil }
end

-- Bound beings are not kept (they fade when the mage leaves); the familiar is.
function summoning.snapshot(s)
  return { familiar = s.familiar and { species = s.familiar.species, since = s.familiar.since } or nil }
end

-- Where water is within reach of pos, or nil.
local function water_near(pos, reach)
  for r = 0, reach, 2 do
    for k = 0, 11 do
      local q = world.move(pos, k * math.pi / 6, r, 0)
      if smalllife.water(q) then return q end
      if r == 0 then break end
    end
  end
  return nil
end

-- Calls a being (sm: a summons entry) at pos for the mage; factor (mastery)
-- scales its binding. Returns the bound one, or nil and why.
function summoning.call(s, sm, pos, mage, factor)
  for colour, n in pairs(sm.cost) do
    if (mage.mana[colour] or 0) < n then return nil, "too little " .. colour .. " mana" end
  end
  local at = pos
  if sm.needs == "water" then
    at = water_near(pos, sm.reach or 20)
    if not at then return nil, "no water near to form it from" end
  end
  for colour, n in pairs(sm.cost) do mage.mana[colour] = mage.mana[colour] - n end
  local id = sim.spawn_creature(sm.species, at)
  sim.bond(id, summoning.FOLLOW)
  local b = { creature = id, summons = sm.id, binding = math.min(1 * (factor or 1), 2) }
  s.bound[#s.bound + 1] = b
  return b
end

-- Lets every bound being go (it fades); returns how many.
function summoning.dismiss(s)
  local n = #s.bound
  for _, b in ipairs(s.bound) do
    local c = sim.creature(b.creature)
    if c then mana.release(c.pos, next(summoning.get(b.summons).cost), 5) end
    sim.remove_creature(b.creature)
  end
  s.bound = {}
  return n
end

-- Upkeep: dt seconds from the mage; unpaid, bindings weaken and beings at
-- nothing fade. Returns the names of those that faded.
function summoning.tick(s, dt, mage)
  local faded = {}
  local kept = {}
  for _, b in ipairs(s.bound) do
    local sm = summoning.get(b.summons)
    local paid = true
    for colour, per in pairs(sm.upkeep) do
      local need = per * dt
      if (mage.mana[colour] or 0) >= need then
        mage.mana[colour] = mage.mana[colour] - need
      else
        paid = false
      end
    end
    if not paid then b.binding = b.binding - summoning.WEAKEN * dt * (sm.will / 0.1) end
    local c = sim.creature(b.creature)
    if b.binding <= 0 or not c or not c.alive then
      if c then
        mana.release(c.pos, next(sm.cost), 5)
        sim.remove_creature(b.creature)
      end
      faded[#faded + 1] = sm.name
    else
      kept[#kept + 1] = b
    end
  end
  s.bound = kept
  return faded
end

-- The familiar entry of a species, or nil.
function summoning.familiar_kind(species)
  for _, f in ipairs(summoning.data().familiars) do
    if f.species == species then return f end
  end
end

-- Bonds the nearest calm animal within reach that can be a familiar, with
-- its offering from the pack and life mana. Returns the familiar, or nil and why.
function summoning.bond(s, pos, p, mage, now)
  if s.familiar then return nil, "you have a familiar" end
  local best, bd
  for _, id in ipairs(sim.creatures_near(pos, summoning.BOND_REACH)) do
    local c = sim.creature(id)
    if c and c.alive and summoning.familiar_kind(c.species) then
      local d = world.distance(c.pos, pos)
      if not bd or d < bd then best, bd = c, d end
    end
  end
  if not best then return nil, "no animal close by that would be a familiar" end
  if (best.fear or 0) > 0.8 then return nil, "it is too afraid" end -- (the offering and life mana soothe the rest)
  local f = summoning.familiar_kind(best.species)
  for id, n in pairs(f.offering) do
    if pack.count(p, id) < n then return nil, "it wants " .. items.name(id):lower() end
  end
  if (mage.mana.life or 0) < summoning.BOND_MANA then return nil, "too little life mana" end
  for id, n in pairs(f.offering) do pack.take(p, id, n) end
  mage.mana.life = mage.mana.life - summoning.BOND_MANA
  sim.bond(best.id, 2.5)
  s.familiar = { species = best.species, since = now, creature = best.id }
  return s.familiar
end

-- The familiar's trait level: 1, and one more each day bonded, up to 5.
function summoning.trait_level(s, now)
  if not s.familiar then return 0 end
  return math.min(1 + math.floor((now - s.familiar.since) / 86400), 5)
end

-- Brings the familiar back into the world beside the mage (after a load).
function summoning.restore_familiar(s, pos)
  local f = s.familiar
  if f and not (f.creature and sim.creature(f.creature)) then
    f.creature = sim.spawn_creature(f.species, world.move(pos, 0, 1.5, 0))
    sim.bond(f.creature, 2.5)
  end
end

function summoning.hud_lines(s, now)
  local lines = {}
  for _, b in ipairs(s.bound) do
    local name = summoning.get(b.summons).name
    lines[#lines + 1] = string.format("%s, bound %d%%", name, math.floor(b.binding * 100 + 0.5))
  end
  if s.familiar then
    local f = summoning.familiar_kind(s.familiar.species)
    lines[#lines + 1] = string.format("Familiar: %s (%s %d)", s.familiar.species, f.trait:gsub("_", " "),
      summoning.trait_level(s, now))
  end
  return lines
end

return summoning
