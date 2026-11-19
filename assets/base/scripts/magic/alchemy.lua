-- SPDX-License-Identifier: MIT
-- Alchemy in the laboratory (concept 3.4): a recipe's goods go into a
-- vessel and its steps run through the chemistry (life/chemistry.lua):
-- grinding, steeping and heating, filtering, mana charged from the mage,
-- reducing, distilling; what bottles comes out into the pack. Brewing takes
-- the recipe's hours of game time, and what passed through the vessel
-- becomes known.
local chemistry = require("life.chemistry")
local items = require("life.items")
local pack = require("player.pack")

local alchemy = {}

local registry
function alchemy.recipes()
  if not registry then
    local text = assert(vfs.read("data/recipes.lua"))
    registry = assert(load(text, "=data/recipes.lua", "t", {}))()
    for _, r in ipairs(registry) do
      assert(items.get(r.yields), "recipe " .. r.id .. ": no item " .. tostring(r.yields))
      for id in pairs(r.needs) do assert(items.get(id), "recipe " .. r.id .. ": no item " .. id) end
    end
  end
  return registry
end

-- The hours a recipe takes.
function alchemy.hours(r)
  local h = 0
  for _, s in ipairs(r.steps) do
    if s[1] == "heat" then h = h + s[3] end
  end
  return h + 0.25 * #r.steps
end

-- What a recipe lacks in the pack: { item = missing } (empty when ready).
function alchemy.lacks(r, p)
  local out = {}
  for id, n in pairs(r.needs) do
    local have = pack.count(p, id)
    if have < n then out[id] = n - have end
  end
  return out
end

-- Brews a recipe from the pack, the mage's mana charged where it calls for
-- it; known (substance ids) learns what was in the vessel. yield scales the
-- goods' worth (mastery). Returns { item = count } bottled and the hours
-- taken, or nil and why.
function alchemy.brew(r, p, mage, known, yield)
  local lack = alchemy.lacks(r, p)
  if next(lack) then
    local parts = {}
    for id, n in pairs(lack) do parts[#parts + 1] = items.name(id):lower() .. " " .. n end
    table.sort(parts)
    return nil, "not enough: " .. table.concat(parts, ", ")
  end
  local v = chemistry.vessel()
  for id, n in pairs(r.needs) do
    pack.take(p, id, n)
    chemistry.add(v, id, n * (yield or 1))
  end
  for _, s in ipairs(r.steps) do
    local kind = s[1]
    if kind == "grind" then
      chemistry.grind(v)
    elseif kind == "heat" then
      chemistry.heat(v, s[2], s[3])
    elseif kind == "filter" then
      chemistry.filter(v)
    elseif kind == "charge" then
      local paid = math.min(mage.mana[s[2]] or 0, s[3])
      mage.mana[s[2]] = (mage.mana[s[2]] or 0) - paid
      chemistry.charge(v, s[2], paid)
    elseif kind == "reduce" then
      chemistry.reduce(v, s[2])
    elseif kind == "distil" then
      v = chemistry.distil(v, s[2])
    end
    if known then chemistry.analyse(v, known) end -- (all that passes through the vessel is seen)
  end
  local bottled = chemistry.bottle(v)
  for id, n in pairs(bottled) do pack.add(p, id, n, 0) end
  return bottled, alchemy.hours(r)
end

-- The HUD line for the chosen recipe.
function alchemy.line(r, p)
  local parts = {}
  for id, n in pairs(r.needs) do
    parts[#parts + 1] = string.format("%s %d/%d", items.name(id):lower(), pack.count(p, id), n)
  end
  table.sort(parts)
  return string.format("Recipe: %s (%s; %.0f h) - K to brew, . for the next", r.name, table.concat(parts, ", "),
    alchemy.hours(r))
end

return alchemy
