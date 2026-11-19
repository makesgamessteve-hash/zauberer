-- SPDX-License-Identifier: MIT
-- Chemistry (concept 5.11): substances, mixtures in vessels, a data-driven
-- table of reactions and the processes of the apparatus. Substances
-- (data/substances.lua) have a phase, a boiling point, whether they
-- dissolve, and the item they are bottled as. Goods put in a vessel bring
-- their substances (items' `contains`). Reactions (data/reactions.lua) turn
-- reactants into products in proportions within a temperature range, at a
-- rate fastest at their best temperature (and twice as fast ground), some
-- only in a solvent, with a catalyst, or with mana of a colour charged into
-- the vessel. The processes: grinding, heating (reactions run; liquids boil
-- off above their boiling point), distilling (what boils below a
-- temperature is caught in a new vessel), filtering (the undissolved
-- solids are taken out), reducing (water boiled off) and bottling (as
-- items). Substances are unknown until analysed.
local items = require("life.items")

local chemistry = {}

chemistry.EPS = 1e-9

local substances, reactions -- read on first use

local function read(path)
  local text = assert(vfs.read(path))
  return assert(load(text, "=" .. path, "t", {}))()
end

-- Checks substances and reactions; returns them by id, or nil and what is wrong.
function chemistry.check(subs, reacts)
  local by_id = {}
  for i, s in ipairs(subs) do
    local where = "substance " .. tostring(s.id or i)
    if type(s.id) ~= "string" or by_id[s.id] then return nil, where .. ": no id or twice" end
    if s.phase ~= "solid" and s.phase ~= "liquid" and s.phase ~= "gas" then return nil, where .. ": no phase" end
    if s.bottle and not items.get(s.bottle.item) then return nil, where .. ": bottled as no item" end
    by_id[s.id] = s
  end
  local rx = {}
  for i, r in ipairs(reacts) do
    local where = "reaction " .. tostring(r.id or i)
    if type(r.id) ~= "string" then return nil, where .. ": no id" end
    for _, side in ipairs({ r.reactants or {}, r.products or {} }) do
      for id, n in pairs(side) do
        if not by_id[id] then return nil, where .. ": no substance " .. id end
        if type(n) ~= "number" or n <= 0 then return nil, where .. ": proportions must be positive" end
      end
    end
    if not next(r.reactants or {}) or not next(r.products or {}) then return nil, where .. ": nothing reacts" end
    if type(r.temp) ~= "table" or not (r.temp[1] < r.best and r.best < r.temp[2]) then
      return nil, where .. ": temperature range and best"
    end
    if r.catalyst and not by_id[r.catalyst] then return nil, where .. ": no catalyst " .. r.catalyst end
    rx[#rx + 1] = r
  end
  return by_id, rx
end

function chemistry.substances()
  if not substances then
    substances, reactions = assert(chemistry.check(read("data/substances.lua"), read("data/reactions.lua")))
  end
  return substances
end

function chemistry.reactions()
  chemistry.substances()
  return reactions
end

function chemistry.get(id) return chemistry.substances()[id] end

-- ---- Vessels ----

function chemistry.vessel()
  return { amounts = {}, temp = 20, mana = {}, ground = false }
end

function chemistry.amount(v, id) return v.amounts[id] or 0 end

function chemistry.total(v)
  local kg = 0
  for _, a in pairs(v.amounts) do kg = kg + a end
  return kg
end

local function put(v, id, kg)
  if kg > chemistry.EPS then v.amounts[id] = (v.amounts[id] or 0) + kg end
end

local function take(v, id, kg)
  local have = v.amounts[id] or 0
  local n = math.min(have, kg)
  if have - n <= chemistry.EPS then v.amounts[id] = nil else v.amounts[id] = have - n end
  return n
end

-- Puts count of an item in the vessel: its substances by its weight.
-- Returns the kg added, or nil and why.
function chemistry.add(v, item, count)
  local it = items.get(item)
  if not it or not it.contains then return nil, "it does not go in a vessel" end
  local kg = 0
  for id, share in pairs(it.contains) do
    put(v, id, it.weight * count * share)
    kg = kg + it.weight * count * share
  end
  return kg
end

-- Charges mana of a colour into the vessel (for reactions that need it).
function chemistry.charge(v, colour, amount)
  v.mana[colour] = (v.mana[colour] or 0) + amount
end

function chemistry.grind(v) v.ground = true end

local function solvent_kg(v)
  local kg = 0
  for id, a in pairs(v.amounts) do
    if chemistry.get(id).solvent then kg = kg + a end
  end
  return kg
end

-- How fast a reaction goes at a temperature: 1 at its best, falling to 0 at
-- the ends of its range (0 outside).
local function warmth(r, temp)
  if temp <= r.temp[1] or temp >= r.temp[2] then return 0 end
  if temp < r.best then return (temp - r.temp[1]) / (r.best - r.temp[1]) end
  return (r.temp[2] - temp) / (r.temp[2] - r.best)
end

-- One reaction for hours at the vessel's temperature; returns the kg of
-- products made.
local function react(v, r, hours)
  local f = warmth(r, v.temp)
  if f <= 0 then return 0 end
  if r.catalyst and chemistry.amount(v, r.catalyst) <= chemistry.EPS then return 0 end
  -- How much could react: the reactant in shortest supply, per unit of the formula.
  local units = math.huge
  for id, n in pairs(r.reactants) do units = math.min(units, chemistry.amount(v, id) / n) end
  if r.solvent then units = math.min(units, solvent_kg(v)) end -- (needs as much solvent as matter)
  if units <= chemistry.EPS then return 0 end
  local share = math.min(r.rate * f * hours * (v.ground and 2 or 1), 1)
  local made = units * share
  local formula = 0
  for _, n in pairs(r.products) do formula = formula + n end
  for colour, per in pairs(r.mana or {}) do -- mana per kg of products
    local need = per * made * formula
    local have = v.mana[colour] or 0
    if have + chemistry.EPS < need then made = have / (per * formula) end
  end
  if made <= chemistry.EPS then return 0 end
  for colour, per in pairs(r.mana or {}) do
    v.mana[colour] = math.max((v.mana[colour] or 0) - per * made * formula, 0)
  end
  for id, n in pairs(r.reactants) do take(v, id, n * made) end
  local kg = 0
  for id, n in pairs(r.products) do
    put(v, id, n * made)
    kg = kg + n * made
  end
  return kg
end

-- Heats the vessel at a temperature for hours, in steps: reactions run, and
-- liquids boil off above their boiling point (an open vessel). Returns
-- { reaction id = kg made }.
function chemistry.heat(v, temp, hours)
  v.temp = temp
  local made = {}
  local steps = math.max(math.ceil(hours * 4), 1)
  for _ = 1, steps do
    for _, r in ipairs(chemistry.reactions()) do
      local kg = react(v, r, hours / steps)
      if kg > 0 then made[r.id] = (made[r.id] or 0) + kg end
    end
    for id, a in pairs(v.amounts) do
      local s = chemistry.get(id)
      if s.phase == "liquid" and s.boils and temp > s.boils then
        take(v, id, a * math.min((temp - s.boils) / 20 * hours / steps, 1))
      elseif s.phase == "gas" then
        take(v, id, a)
      end
    end
  end
  return made
end

-- Distils at a temperature: what boils below it is caught in a new vessel.
function chemistry.distil(v, temp)
  local out = chemistry.vessel()
  for id, a in pairs(v.amounts) do
    local s = chemistry.get(id)
    if s.boils and s.boils <= temp and s.phase ~= "solid" then
      put(out, id, a)
      v.amounts[id] = nil
    end
  end
  return out
end

-- Filters: the solids that do not dissolve are taken out (returned in a vessel).
function chemistry.filter(v)
  local residue = chemistry.vessel()
  for id, a in pairs(v.amounts) do
    local s = chemistry.get(id)
    if s.phase == "solid" and not s.dissolves then
      put(residue, id, a)
      v.amounts[id] = nil
    end
  end
  return residue
end

-- Boils off water until there is no more than kg of it left.
function chemistry.reduce(v, kg)
  local w = chemistry.amount(v, "water")
  if w > kg then take(v, "water", w - kg) end
end

-- Bottles what can be bottled as items: { item = count }; the rest stays.
function chemistry.bottle(v)
  local out = {}
  for id, a in pairs(v.amounts) do
    local s = chemistry.get(id)
    if s.bottle then
      local n = math.floor(a / s.bottle.kg + 1e-6)
      if n > 0 then
        out[s.bottle.item] = (out[s.bottle.item] or 0) + n
        take(v, id, n * s.bottle.kg)
      end
    end
  end
  return out
end

-- ---- Knowledge ----

-- What a substance is called by someone who knows (known[id]) or not.
function chemistry.name(id, known)
  local s = chemistry.get(id)
  if not s then return id end
  if known and known[id] then return s.name end
  return "an unknown " .. s.phase
end

-- Analysis (the laboratory): every substance in the vessel becomes known;
-- returns the newly known ids.
function chemistry.analyse(v, known)
  local new = {}
  for id in pairs(v.amounts) do
    if not known[id] then
      known[id] = true
      new[#new + 1] = id
    end
  end
  table.sort(new)
  return new
end

return chemistry
