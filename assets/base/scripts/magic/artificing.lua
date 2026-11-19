-- SPDX-License-Identifier: MIT
-- Artificing (concept 3.4): enchanting binds a weave into a base object
-- with a power source and a trigger, making an artifact; or weaves an
-- upgrade into the carpet, or folds into the bag. The base's material
-- carries some colours better (items' `conducts`), which makes enchanting
-- cheaper. Artifacts work by their trigger while they hold charge: an
-- ambient one refills from the mana field where the mage is, a wearer's
-- from the mage, a crystal only from what it was given.
local items = require("life.items")
local pack = require("player.pack")

local artificing = {}

local registry
function artificing.enchantments()
  if not registry then
    local text = assert(vfs.read("data/enchantments.lua"))
    registry = assert(load(text, "=data/enchantments.lua", "t", {}))()
    for _, e in ipairs(registry) do
      assert(e.base == "carpet" or e.base == "bag" or items.get(e.base), "enchantment " .. e.id .. ": no base")
    end
  end
  return registry
end

function artificing.get(id)
  for _, e in ipairs(artificing.enchantments()) do
    if e.id == id then return e end
  end
end

-- The mana an enchantment costs on its base: each colour times 1.5 less how
-- well the base carries it; factor (mastery) scales it.
function artificing.cost(e, factor)
  local conducts = items.get(e.base) and items.get(e.base).conducts or {}
  local out = {}
  for colour, n in pairs(e.cost) do out[colour] = n * (1.5 - (conducts[colour] or 0.5)) * (factor or 1) end
  return out
end

-- What an enchantment lacks: { what = missing } (mana by colour, items, the base).
function artificing.lacks(e, p, mage, factor)
  local out = {}
  if items.get(e.base) and pack.count(p, e.base) < 1 then out[e.base] = 1 end
  for id, n in pairs(e.needs or {}) do
    if pack.count(p, id) < n then out[id] = n - pack.count(p, id) end
  end
  for colour, n in pairs(artificing.cost(e, factor)) do
    if (mage.mana[colour] or 0) + 1e-9 < n then out[colour .. " mana"] = math.ceil(n - (mage.mana[colour] or 0)) end
  end
  return out
end

-- Enchants. ctx: { pack, mage, condition (+ condition module cmod), bag,
-- artifacts (the list the new one joins), factor }. Returns what was made
-- (the artifact, or the upgrade's name), or nil and why.
function artificing.enchant(e, ctx)
  local lack = artificing.lacks(e, ctx.pack, ctx.mage, ctx.factor)
  if next(lack) then
    local parts = {}
    for id, n in pairs(lack) do parts[#parts + 1] = (items.get(id) and items.name(id):lower() or id) .. " " .. n end
    table.sort(parts)
    return nil, "not enough: " .. table.concat(parts, ", ")
  end
  if e.upgrade then
    local ok, why = ctx.cmod.install(ctx.condition, e.upgrade)
    if not ok then return nil, why end
  end
  for colour, n in pairs(artificing.cost(e, ctx.factor)) do ctx.mage.mana[colour] = ctx.mage.mana[colour] - n end
  for id, n in pairs(e.needs or {}) do pack.take(ctx.pack, id, n) end
  if e.folds then
    ctx.bag.folds = ctx.bag.folds + e.folds
    return e.name
  end
  if e.upgrade then return e.name end
  pack.take(ctx.pack, e.base, 1)
  local a = { id = e.id, charge = e.capacity or 0 }
  ctx.artifacts[#ctx.artifacts + 1] = a
  return a
end

-- Artifacts at work: dt seconds; ctx: { mage, pos (where the mage is) }.
-- Passive ones drain while charged and refill from their source.
function artificing.tick(list, dt, ctx)
  for _, a in ipairs(list) do
    local e = artificing.get(a.id)
    if e and e.trigger == "passive" then
      a.charge = math.max(a.charge - (e.drain or 0) * dt, 0)
      local want = (e.capacity or 0) - a.charge
      local colour = e.weave.colors[1]
      if want > 0 and e.source == "ambient" and ctx.pos then
        a.charge = a.charge + mana.draw(ctx.pos, colour, want)
      elseif want > 0 and e.source == "wearer" and ctx.mage then
        local got = math.min(ctx.mage.mana[colour] or 0, want)
        ctx.mage.mana[colour] = (ctx.mage.mana[colour] or 0) - got
        a.charge = a.charge + got
      end
      a.lit = a.charge > 0
    end
  end
end

-- Whether an artifact of an enchantment is working (lit, warming ...).
function artificing.working(list, id)
  for _, a in ipairs(list) do
    if a.id == id and a.lit then return true end
  end
  return false
end

function artificing.snapshot(list)
  local out = {}
  for i, a in ipairs(list) do out[i] = { id = a.id, charge = a.charge } end
  return out
end

function artificing.restore(saved)
  local out = {}
  for _, a in ipairs(type(saved) == "table" and saved or {}) do
    if type(a) == "table" and artificing.get(a.id) then
      out[#out + 1] = { id = a.id, charge = tonumber(a.charge) or 0 }
    end
  end
  return out
end

function artificing.line(e, p, mage, factor)
  local parts = {}
  for colour, n in pairs(artificing.cost(e, factor)) do parts[#parts + 1] = string.format("%s %.0f", colour, n) end
  table.sort(parts)
  local lack = next(artificing.lacks(e, p, mage, factor)) and " (lacking)" or ""
  return string.format("Enchant: %s on %s (%s)%s - K to enchant, . for the next", e.name,
    items.get(e.base) and items.name(e.base):lower() or e.base, table.concat(parts, ", "), lack)
end

function artificing.hud_lines(list)
  local lines = {}
  for _, a in ipairs(list) do
    local e = artificing.get(a.id)
    lines[#lines + 1] = string.format("%s: %s", e.name, a.lit and "alight" or "dark")
  end
  return lines
end

return artificing
